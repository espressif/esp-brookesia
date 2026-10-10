# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
from importlib import resources
from typing import Any, Dict, Optional

from brookesia_gui_perf.report import judgment_value
from brookesia_gui_perf.stats import cv_ok

IMPROVEMENT_GATE_PCT = 10.0
RELATIVE_FAIL_FACTOR = {"max": 1.10, "min": 0.90}
# A single 20-minute run carries sub-1% noise on several metrics; a delta inside
# this band is measurement noise, not a regression, so it must not fail the pair.
REGRESSION_TOLERANCE_PCT = 1.0
# composite_stress is the section 3.8 native-LVGL ceiling probe: a reference
# workload, not a JSON-UI acceptance target, so it is recorded but never gates.
RECORD_ONLY_SCENARIOS = {"composite_stress"}


def load_thresholds() -> Dict[str, Any]:
    data = resources.files("brookesia_gui_perf").joinpath("thresholds.json").read_text(encoding="utf-8")
    return json.loads(data)


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


def compare_reports(
    baseline: Dict[str, Any],
    optimized: Dict[str, Any],
    *,
    thresholds: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    thresholds = thresholds if thresholds is not None else load_thresholds()
    base_meta = baseline.get("metadata", {})
    opt_meta = optimized.get("metadata", {})
    metrics_out: Dict[str, Dict[str, Any]] = {}
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
            base_stats = base_metrics[name]
            opt_stats = opt_metrics[name]
            if "value" in base_stats and "value" in opt_stats:
                continue
            base_value = judgment_value(base_stats, row["judge"])
            opt_value = judgment_value(opt_stats, row["judge"])
            base_grade = grade(row, base_value, baseline_value=None, metadata=base_meta)
            opt_grade = grade(row, opt_value, baseline_value=base_value, metadata=opt_meta)
            if opt_grade == "record" or base_grade == "record":
                improvement = None
                regressed = False
                ok = True
            else:
                improvement = improvement_pct(row["direction"], base_value, opt_value)
                # A noisier metric needs a wider noise band before a negative delta is
                # called a regression; rows may override the global default.
                regression_tolerance = row.get("regression_tolerance_pct", REGRESSION_TOLERANCE_PCT)
                regressed = improvement < -regression_tolerance
                ok = improvement >= IMPROVEMENT_GATE_PCT or (
                    not regressed and opt_grade != "fail"
                )
            if not cv_ok(opt_stats) and not row.get("cv_exempt"):
                ok = False
            all_ok = all_ok and ok
            metrics_out[name] = {
                "improvement_pct": improvement,
                "baseline_grade": base_grade,
                "optimized_grade": opt_grade,
                "regressed": regressed,
                "ok": ok,
            }
    return {"pass": all_ok, "metrics": metrics_out}
