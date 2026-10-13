# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import hashlib
import subprocess
from typing import Any, Dict, List

from brookesia_gui_perf.parser import RunRecords
from brookesia_gui_perf.spec_coverage import (
    assess_coverage,
    assess_dimensions,
    assess_exemptions,
    uncovered,
)
from brookesia_gui_perf.stats import DEFAULT_WARMUP, MIN_SAMPLES, aggregate

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


def _worktree_diff_sha256() -> str:
    """sha256 over tracked diff + untracked file list. Empty string if git absent."""
    try:
        diff = subprocess.run(
            ["git", "diff"], capture_output=True, text=True, timeout=20, check=False
        ).stdout
        names = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard"],
            capture_output=True, text=True, timeout=20, check=False,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return ""
    payload = diff + "\n--untracked--\n" + names
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def build_report(
    records: RunRecords,
    *,
    warmup: int = DEFAULT_WARMUP,
    worktree_dirty: bool = False,
    worktree_diff_sha256: str = "",
) -> Dict[str, Any]:
    worktree_diff = worktree_diff_sha256 or _worktree_diff_sha256()
    worktree_flag = worktree_dirty or bool(worktree_diff)
    head = records.metadata or {}
    per_scenario: Dict[str, List[Dict[str, Any]]] = {}
    for sample in records.samples:
        per_scenario.setdefault(sample["scenario"], []).append(sample)

    scenarios: Dict[str, Dict[str, Any]] = {}
    min_kept = None
    for scenario, samples in sorted(per_scenario.items()):
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
