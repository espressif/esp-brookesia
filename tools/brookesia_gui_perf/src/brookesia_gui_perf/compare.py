# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import math
import pathlib
from importlib import resources
from typing import Any, Dict, Optional, List

from brookesia_gui_perf import spec_coverage as _spec_coverage
from brookesia_gui_perf.report import judgment_value
from brookesia_gui_perf.stats import cv_ok

RELATIVE_FAIL_FACTOR = {"max": 1.10, "min": 0.90}
# A single 20-minute run carries sub-1% noise on several metrics; a delta inside
# this band is measurement noise, not a regression, so it must not fail the pair.
REGRESSION_TOLERANCE_PCT = 1.0
# composite_stress is the section 3.8 native-LVGL ceiling probe: a reference
# workload, not a JSON-UI acceptance target, so it is recorded but never gates.
RECORD_ONLY_SCENARIOS = {
    "composite_stress",
    # refresh_probe_* are the process_timers() acceptance diagnostic: they
    # report refreshes / flushes / tick cost for static, local and full
    # invalidation, with no pass/fail target of their own.
    "refresh_probe_static",
    "refresh_probe_local",
    "refresh_probe_full",
}


def load_thresholds() -> Dict[str, Any]:
    data = resources.files("brookesia_gui_perf").joinpath("thresholds.json").read_text(encoding="utf-8")
    return json.loads(data)


def scenario_row(row: Dict[str, object], scenario: str) -> Dict[str, object]:
    """Threshold row with this scenario's overrides folded in.

    A single global rule cannot describe every scenario: fps is graded on a
    sustained-load phase but is 0 whenever the carrier legitimately stops
    redrawing, so those scenarios opt out instead of failing the run.
    """
    overrides = row.get("scenario_overrides") or {}
    override = overrides.get(scenario)
    if not override:
        return row
    merged = dict(row)
    merged.update(override)
    merged.pop("scenario_overrides", None)
    return merged


def improvement_pct(direction: str, baseline: float, optimized: float) -> float:
    if baseline == 0:
        if optimized == 0:
            return 0.0
        raise ValueError("zero baseline cannot produce an improvement ratio")
    if direction == "higher_better":
        return (optimized - baseline) / baseline * 100.0
    if direction == "lower_better":
        return (baseline - optimized) / baseline * 100.0
    raise ValueError(f"unknown direction: {direction}")


def _resolve_band(value: Any, metadata: Dict[str, Any]) -> float:
    if isinstance(value, (int, float)):
        return float(value)
    period = float(metadata.get("display", {}).get("refresh_period_ms", 0.0))
    if period <= 0:
        raise ValueError("display.refresh_period_ms required to resolve T_refresh bands")
    if value == "T_refresh":
        return period
    if value == "2*T_refresh":
        return period * 2.0
    raise ValueError(f"unsupported band expression: {value!r}")


def grade(
    row: Dict[str, Any],
    value: float,
    *,
    baseline_value: Optional[float],
    metadata: Dict[str, Any],
) -> str:
    if row.get("record_only"):
        return "record"
    if "budget" in row:
        budget = float(metadata.get("mem_budget_kb", {}).get(row["budget"], 0))
        if budget <= 0:
            raise ValueError(f"mem_budget_kb.{row['budget']} missing or non-positive")
        if value <= budget:
            return "pass_a"
        if value <= budget * 1.10:
            return "pass_b"
        return "fail"
    if "relative" in row:
        if baseline_value is None:
            return "record"
        if row["relative"] == "max" and value > baseline_value * RELATIVE_FAIL_FACTOR["max"]:
            return "fail"
        if row["relative"] == "min" and value < baseline_value * RELATIVE_FAIL_FACTOR["min"]:
            return "fail"
        return "pass_b"
    if "pass_a_max" in row:
        if value <= _resolve_band(row["pass_a_max"], metadata):
            return "pass_a"
        if value <= _resolve_band(row["pass_b_max"], metadata):
            return "pass_b"
        return "fail"
    if value >= _resolve_band(row["pass_a_min"], metadata):
        return "pass_a"
    if value >= _resolve_band(row["pass_b_min"], metadata):
        return "pass_b"
    return "fail"


def _shipped_baseline_metrics(path: Optional[pathlib.Path] = None) -> Optional[set]:
    """Metric names this carrier is known to emit, or None when unknown.

    The catalogue is shipped inside the package so an installed wheel validates
    thresholds without a repository checkout; the repo baseline is only a fallback.
    """
    try:
        catalog = resources.files("brookesia_gui_perf").joinpath("metric_catalog.json")
        return set(json.loads(catalog.read_text(encoding="utf-8"))["metrics"])
    except (OSError, ValueError, KeyError):
        pass
    try:
        root = pathlib.Path(__file__).resolve().parents[4]
        report = path or (root / "reports" / "baseline" / "esp32_p4x_function_ev"
                          / "Baseline.esp32_p4x_function_ev.json")
        if not report.is_file():
            return None
        payload = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    names = set()
    for scenario in payload.get("scenarios", {}).values():
        names.update(scenario.get("metrics", {}))
    return names


DECLARED_UNINSTRUMENTED = {
    "alloc_resident_pct", "build_copy_bytes", "doc_load_transient_kb",
    "task_heap_max_kb", "doc_load_ms", "first_frame_ms", "first_visible_ms",
    "page_switch_ms",
}

_SPEC_PENDING_METRICS = [
    metric
    for entry in _spec_coverage._FIELDS.values()
    if entry.get("status") in (_spec_coverage.PENDING, _spec_coverage.NOT_APPLICABLE)
    for metric in entry.get("metrics", [])
]


def compare_reports(
    baseline: Dict[str, Any],
    optimized: Dict[str, Any],
    *,
    thresholds: Optional[Dict[str, Any]] = None,
    metric_catalog: "Optional[set]" = None,
) -> Dict[str, Any]:
    thresholds = thresholds if thresholds is not None else load_thresholds()
    base_meta = baseline.get("metadata", {})
    opt_meta = optimized.get("metadata", {})
    metrics_out: Dict[str, Dict[str, Dict[str, Any]]] = {}
    all_ok = True

    base_scenarios = baseline.get("scenarios", {})
    opt_scenarios = optimized.get("scenarios", {})
    for scenario in sorted(set(base_scenarios) & set(opt_scenarios)):
        if scenario in RECORD_ONLY_SCENARIOS:
            continue
        base_metrics = base_scenarios[scenario]["metrics"]
        opt_metrics = opt_scenarios[scenario]["metrics"]
        for name in sorted(set(base_metrics) & set(opt_metrics)):
            row = thresholds.get(name)
            if row is None:
                continue
            row = scenario_row(row, scenario)
            if row.get("record"):
                # A static screen stops redrawing by design, so an absolute fps floor
                # cannot apply there: keep the value in the report without grading it.
                metrics_out.setdefault(scenario, {})[name] = {
                    "improvement_pct": None,
                    "baseline_grade": "record",
                    "optimized_grade": "record",
                    "regressed": False,
                    "ok": True,
                }
                continue
            base_stats = base_metrics[name]
            opt_stats = opt_metrics[name]
            if "value" in base_stats and "value" in opt_stats:
                continue
            base_value = judgment_value(base_stats, row["judge"])
            opt_value = judgment_value(opt_stats, row["judge"])
            base_grade = grade(row, base_value, baseline_value=None, metadata=base_meta)
            opt_grade = grade(row, opt_value, baseline_value=base_value, metadata=opt_meta)
            # A relative rule always grades the baseline with baseline_value=None, which
            # is "record" by construction; gating on base_grade would therefore skip
            # every relative metric and hide real regressions.
            if opt_grade == "record":
                improvement = None
                regressed = False
                ok = True
            elif base_value == 0:
                # Section 5.2 step 3 is a ratio, and a zero baseline cannot define
                # one: json.loads'd captures legitimately carry p95=0 rows (the
                # shipped P4X baseline has feedback_delay/event_dispatch_ms at 0).
                # Report the ratio as N/A and let the absolute bands decide, so a
                # 0 -> non-zero move is neither an optimization nor an auto-pass.
                improvement = None
                regressed = False
                ok = opt_grade in ("pass_a", "pass_b")
            else:
                improvement = improvement_pct(row["direction"], base_value, opt_value)
                # A noisier metric needs a wider noise band before a negative delta is
                # called a regression; rows may override the global default.
                if row.get("relative"):
                    # Section 4 grades a relative row on the same band that decides
                    # its Fail (10% either side), so the global 1% noise floor must
                    # not narrow it: a same-firmware re-run drifted 2.4% on
                    # batch_invalidate_ms and was reported as a regression.
                    regression_tolerance = abs(
                        (RELATIVE_FAIL_FACTOR[row["relative"]] - 1.0) * 100.0
                    )
                else:
                    regression_tolerance = row.get(
                        "regression_tolerance_pct", REGRESSION_TOLERANCE_PCT
                    )
                regressed = improvement < -regression_tolerance
                # json.loads accepts NaN/Infinity; every comparison against them is
                # false, so a non-finite ratio must never look "unregressed".
                if not math.isfinite(improvement):
                    regressed = True
                # Spec section 5.2 step 3 qualifies a metric either by a >=+10%
                # improvement or by "no regression and no Fail item". Since a graded
                # metric is only pass_a/pass_b/fail, the latter subsumes the former;
                # keep the single explicit form so a relative improvement can never
                # again short-circuit an absolute Fail grade.
                ok = (not regressed) and (opt_grade in ("pass_a", "pass_b"))
                # A record-only row (and a relative row in its first round) has no
                # pass/fail target, so its variance must not veto the verdict.
                # refresh_area_ratio_pct is record_only with no cv_exempt flag; a
                # noisy capture used to turn the whole paired run into a failure.
                if not cv_ok(opt_stats) and not row.get("cv_exempt"):
                    ok = False
            all_ok = all_ok and ok
            metrics_out.setdefault(scenario, {})[name] = {
                "improvement_pct": improvement,
                "baseline_grade": base_grade,
                "optimized_grade": opt_grade,
                "regressed": regressed,
                "ok": ok,
            }
    # Intersecting only covers what both sides emitted: a gated baseline metric that
    # the optimized run lost would be skipped and still return pass. Fail explicitly.
    coverage_gaps: List[Dict[str, str]] = []
    for scenario, scenario_data in base_scenarios.items():
        if scenario in RECORD_ONLY_SCENARIOS:
            continue
        opt_metrics = opt_scenarios.get(scenario, {}).get("metrics", {})
        for name in scenario_data.get("metrics", {}):
            row = thresholds.get(name)
            if row is None or row.get("record_only"):
                continue
            if name not in opt_metrics:
                coverage_gaps.append({"scenario": scenario, "metric": name})
                all_ok = False

    # A threshold keyed by a metric name that appears in neither report can never fire,
    # so a typo silently disables the rule (avg_fps vs fps did exactly that).
    seen_names = set()
    for scenario_data in list(base_scenarios.values()) + list(opt_scenarios.values()):
        seen_names.update(scenario_data.get("metrics", {}))
    unknown_thresholds = [
        name for name, row in thresholds.items()
        if not row.get("record_only") and name not in seen_names
        and name not in DECLARED_UNINSTRUMENTED
        and name not in _SPEC_PENDING_METRICS
    ]
    # A threshold key that the shipped baseline never emits can never fire. Checking
    # against that reference (not against the pair under comparison) keeps partial
    # comparisons legal while still failing on a misspelled key such as fps_typo, which
    # difflib's 0.8 cutoff does not catch (fps_typo vs fps is only 0.55).
    reference = metric_catalog if metric_catalog is not None else _shipped_baseline_metrics()
    if reference is None:
        # Without a known metric catalogue a threshold key cannot be validated, and an
        # unvalidated gate is worse than a failing one.
        if unknown_thresholds:
            all_ok = False
    else:
        unknown_thresholds = [n for n in unknown_thresholds if n not in reference]
        if unknown_thresholds:
            all_ok = False
    # Warmup can leave too few samples for every metric; an empty comparison must
    # not be reported as a pass, and the coverage gaps above still apply.
    if not metrics_out:
        all_ok = False
    # Record-only rows are not graded, so a comparison made only of them proves
    # nothing; at least one metric must have actually been judged.
    graded = any(row.get("optimized_grade") not in (None, "record")
                 for scenario in metrics_out.values() for row in scenario.values())
    if not graded:
        all_ok = False
    return {"pass": all_ok, "metrics": metrics_out, "coverage_gaps": coverage_gaps,
            "unknown_thresholds": unknown_thresholds}
