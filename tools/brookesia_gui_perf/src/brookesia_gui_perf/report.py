# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import hashlib
import math
import subprocess
from typing import Any, Callable, Dict, List, Optional

from brookesia_gui_perf.parser import RunRecords
from brookesia_gui_perf.spec_coverage import (
    assess_coverage,
    assess_dimensions,
    assess_exemptions,
    uncovered,
)
from brookesia_gui_perf.stats import DEFAULT_WARMUP, MIN_SAMPLES, aggregate

DIAGNOSTIC_SCENARIOS = {
    # One-shot probes: raw values only, never statistics, never part of the shared
    # sample budget (see build_report).
    "refresh_probe_static",
    "refresh_probe_local",
    "refresh_probe_full",
}

REQUIRED_METADATA_FIELDS: List[str] = [
    "board_id", "git_sha", "idf_version", "idf_preview", "sdkconfig_hash",
    "cpu_freq_mhz", "lvgl_version", "esp_lv_adapter_version",
    "json_ui_document_version", "fixture_version", "fixture_provenance",
    "golden_case", "resource_scheme", "mem_budget_kb", "display", "touch",
    "core_affinity", "profile_switches", "fast_action_dispatch", "doc_complexity",
    "stat_basis", "present_semantics", "benchmark_validity",
]

REQUIRED_TOP_KEYS = ("test_suite", "app_id", "runtime_type", "sample_count", "metadata", "scenarios")
PERCENTILE_KEYS = ("mean", "std", "cv", "p50", "p75", "p95", "p99", "n")
JUDGES = ("mean", "p95", "p99")


def _worktree_state(run: "Optional[Callable[..., str]]" = None) -> "tuple[bool, str, bool]":
    """Return (dirty, fingerprint) for the *actual* source state.

    A clean tree returns (False, ""). Untracked and staged content are part of the
    fingerprint, because `git diff` alone misses both. A failing git command reports
    dirty with no fingerprint: an unproven state must never look clean, since gate.py
    treats the pair as a non-volatile pairing condition.
    """

    def git(*args: str) -> str:
        result = subprocess.run(
            ["git", *args], capture_output=True, text=True, timeout=20, check=False
        )
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "git command failed")
        return result.stdout

    run = run or git

    try:
        unstaged = run("diff")
        staged = run("diff", "--cached")
        # -z plus quotepath=false keeps non-ASCII names raw; without it git emits an
        # escaped, quoted form that cannot be opened and every such state collapses to
        # the same (failed) fingerprint.
        names = [n for n in run("-c", "core.quotepath=false", "ls-files", "-z",
                                "--others", "--exclude-standard").split("\0") if n]
        untracked = []
        for name in sorted(names):
            with open(name, "rb") as handle:
                untracked.append(name + ":" + hashlib.sha256(handle.read()).hexdigest())
    except (OSError, subprocess.SubprocessError, RuntimeError):
        return True, "", False
    if not (unstaged.strip() or staged.strip() or untracked):
        return False, "", True
    payload = (unstaged + "\x00--staged--\x00" + staged
               + "\x00--untracked--\x00" + "\n".join(untracked))
    return True, hashlib.sha256(payload.encode("utf-8")).hexdigest(), True


def build_report(
    records: RunRecords,
    *,
    warmup: int = DEFAULT_WARMUP,
    worktree_dirty: bool = False,
    worktree_diff_sha256: str = "",
    probe_worktree: bool = False,
    allow_unknown_worktree: bool = False,
) -> Dict[str, Any]:
    if probe_worktree:
        worktree_flag, worktree_diff, worktree_known = _worktree_state()
    else:
        worktree_flag = bool(worktree_dirty or worktree_diff_sha256)
        worktree_diff = worktree_diff_sha256
        worktree_known = True
    head = records.metadata or {}
    per_scenario: Dict[str, List[Dict[str, Any]]] = {}
    for sample in records.samples:
        per_scenario.setdefault(sample["scenario"], []).append(sample)

    scenarios: Dict[str, Dict[str, Any]] = {}
    min_kept = None
    for scenario, samples in sorted(per_scenario.items()):
        if scenario in DIAGNOSTIC_SCENARIOS:
            # A diagnostic probe reports one measurement per condition; applying the
            # warmup drop and the 30-sample minimum would erase it and, worse, drag the
            # top-level sample_count to 0. Keep the raw values out of the statistics and
            # out of the shared sample budget.
            raw: Dict[str, Any] = {}
            for sample in samples:
                for key, value in sample["metrics"].items():
                    raw[key] = {"value": value}
            scenarios[scenario] = {"metrics": raw, "diagnostic": True}
            continue
        kept = samples[warmup:]
        numeric: Dict[str, List[float]] = {}
        passthrough: Dict[str, Any] = {}
        for sample in kept:
            for key, value in sample["metrics"].items():
                if isinstance(value, bool):
                    passthrough[key] = value
                elif isinstance(value, (int, float)):
                    numeric.setdefault(key, []).append(float(value))
                else:
                    passthrough[key] = value
        metrics: Dict[str, Any] = {k: {"value": v} for k, v in passthrough.items()}
        insufficient: Dict[str, int] = {}
        for key, values in numeric.items():
            if len(values) < MIN_SAMPLES:
                # A torn UART record can drop or corrupt a single metric key, which
                # would otherwise abort the whole aggregate over one lost byte. Refuse
                # to aggregate the metric but keep the run usable and record the
                # shortfall explicitly rather than dropping it silently.
                insufficient[key] = len(values)
                continue
            metrics[key] = aggregate(values)
        scenarios[scenario] = {"metrics": metrics}
        if insufficient:
            scenarios[scenario]["insufficient_metrics"] = insufficient
        min_kept = len(kept) if min_kept is None else min(min_kept, len(kept))

    assert min_kept is not None, "log contains no scenarios"
    metadata: Dict[str, Any] = dict(head.get("metadata") or {})
    metadata["worktree_dirty"] = worktree_flag
    # False means "the source state could not be fingerprinted" (git unavailable);
    # pairing such a report with anything proves nothing about the sources matching.
    metadata["worktree_state_known"] = worktree_known
    # Controlled use without git: the caller accepts that provenance is unprovable.
    metadata["worktree_exempt"] = bool(allow_unknown_worktree)
    metadata["worktree_diff_sha256"] = worktree_diff
    report: Dict[str, Any] = {
        "test_suite": head.get("test_suite"),
        "app_id": head.get("app_id"),
        "runtime_type": head.get("runtime_type"),
        "sample_count": min_kept,
        # Torn records are counted rather than raised on, so this is where a capture
        # says so. A non-zero value means the UART lost part of a record and the
        # sample count below already excludes it.
        "malformed_records": records.malformed,
        "metadata": metadata,
        "scenarios": scenarios,
    }
    # v1.8 §5.1/§六-6: every canonical field must resolve to an explicit status so a
    # mandatory memory item can never be silently missing.
    metadata["acceptance"] = assess_coverage(report)
    metadata["dimension_coverage"] = assess_dimensions(report)
    metadata["stat_exemptions"] = assess_exemptions(report)
    return report


def validate_report(report: Dict[str, Any]) -> List[str]:
    violations: List[str] = []
    for key in REQUIRED_TOP_KEYS:
        if key not in report:
            violations.append(f"missing top-level key: {key}")
    # Section 1.2 requires N >= 30 usable samples. A capture that lost records, or
    # had every metric dropped by warmup, used to produce a structurally valid but
    # empty report that both the gate and the paired comparison accepted.
    sample_count = report.get("sample_count")
    count_is_valid = (
        not isinstance(sample_count, bool)
        and isinstance(sample_count, (int, float))
        and math.isfinite(sample_count)
    )
    if not count_is_valid:
        violations.append(f"sample_count {sample_count!r} is not a finite sample count")
    elif sample_count < MIN_SAMPLES:
        violations.append(
            f"sample_count {sample_count!r} is below the section 1.2 minimum of {MIN_SAMPLES}"
        )
    # The parser already excludes torn UART records from the sample count; one or
    # two on this board are normal. Only a capture that lost a meaningful share of
    # its records is unusable, otherwise every future device run would GATE-FAIL.
    tol_basis = sample_count if count_is_valid else 0
    malformed_tolerance = max(2, math.ceil(tol_basis * 0.05))
    malformed = report.get("malformed_records") or 0
    if isinstance(malformed, (int, float)) and not isinstance(malformed, bool) and malformed > malformed_tolerance:
        violations.append(
            f"malformed_records: {malformed} torn records exceed the {malformed_tolerance}-record tolerance"
        )
    for scenario, body in report.get("scenarios", {}).items():
        insufficient = body.get("insufficient_metrics") or {}
        if not isinstance(insufficient, dict):
            violations.append(f"scenario {scenario}: insufficient_metrics is not a mapping")
            continue
        for metric, count in insufficient.items():
            violations.append(
                f"scenario {scenario} metric {metric}: only {count} samples (needs {MIN_SAMPLES})"
            )
    metadata = report.get("metadata", {})
    for key in REQUIRED_METADATA_FIELDS:
        if key not in metadata:
            violations.append(f"missing metadata field: {key}")
    for scenario, body in report.get("scenarios", {}).items():
        for metric, stats in body.get("metrics", {}).items():
            if "value" in stats and len(stats) == 1:
                continue
            for pk in PERCENTILE_KEYS:
                if pk not in stats:
                    violations.append(f"scenario {scenario} metric {metric}: missing {pk}")
    for field in uncovered(report):
        violations.append(f"spec section 5.1 field without declaration: {field}")
    return violations


def judgment_value(metric_stats: Dict[str, float], judge: str) -> float:
    if judge not in JUDGES:
        raise KeyError(f"unsupported judge: {judge}")
    return float(metric_stats[judge])
