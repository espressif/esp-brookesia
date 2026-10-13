# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

from typing import Any, Dict, List

from brookesia_gui_perf.report import validate_report

# Spec section 5.2 step 1: everything except git_sha and test time must match
# between a baseline and an optimized run. The worktree provenance fields
# (worktree_dirty / worktree_diff_sha256) are deliberately NOT volatile: this
# repository carries an intentional uncommitted change, so a dirty-tree run must
# not be paired with a clean-tree run.
VOLATILE_METADATA_FIELDS = {"git_sha"}
VALID_LOG_LEVELS = {"WARN", "ERROR", "NONE"}


def check_validity(metadata: Dict[str, Any]) -> List[str]:
    """Spec section 1.4 benchmark-validity rules. Empty list means valid."""
    violations: List[str] = []
    bv = metadata.get("benchmark_validity") or {}
    ratio = bv.get("draw_buf_ratio")
    min_ratio = 0.05 if bv.get("opa_scene") else 0.10
    if not isinstance(ratio, (int, float)) or ratio < min_ratio:
        violations.append(f"benchmark_validity.draw_buf_ratio {ratio!r} < {min_ratio}")
    if bv.get("asserts_enabled") is not False:
        violations.append("benchmark_validity.asserts_enabled must be false")
    if bv.get("opt_enabled") is not True:
        violations.append("benchmark_validity.opt_enabled must be true")
    if bv.get("log_level") not in VALID_LOG_LEVELS:
        violations.append(
            f"benchmark_validity.log_level {bv.get('log_level')!r} not in {sorted(VALID_LOG_LEVELS)}"
        )
    if bv.get("heap_trace_overflow") is not False:
        violations.append("benchmark_validity.heap_trace_overflow must be false")
    return violations


def _comparable_metadata(metadata: Dict[str, Any]) -> Dict[str, Any]:
    """Metadata as the paired gate must see it.

    ``stat_exemptions`` carries a per-run ``measured_cv``, which by construction
    cannot be equal across two runs; only its declared content (kind / reason /
    limit) is a configuration property and must match between baseline and
    optimized.
    """
    comparable = dict(metadata)
    exemptions = comparable.get("stat_exemptions")
    if isinstance(exemptions, dict):
        comparable["stat_exemptions"] = {
            metric: (
                {key: value for key, value in info.items() if key != "measured_cv"}
                if isinstance(info, dict) else info
            )
            for metric, info in exemptions.items()
        }
    return comparable


def check_gate(baseline: Dict[str, Any], optimized: Dict[str, Any]) -> List[str]:
    """Spec section 5.2 step 1 paired-comparison gate. Empty list means pass."""
    violations: List[str] = []
    violations += [f"baseline: {v}" for v in validate_report(baseline)]
    violations += [f"optimized: {v}" for v in validate_report(optimized)]
    base_meta = _comparable_metadata(baseline.get("metadata", {}))
    opt_meta = _comparable_metadata(optimized.get("metadata", {}))
    if base_meta.get("board_id") != opt_meta.get("board_id"):
        violations.append(
            f"board_id mismatch: {base_meta.get('board_id')!r} != {opt_meta.get('board_id')!r}"
        )
    keys = set(base_meta) | set(opt_meta)
    for key in sorted(keys - VOLATILE_METADATA_FIELDS):
        if base_meta.get(key, "<missing>") != opt_meta.get(key, "<missing>"):
            violations.append(f"metadata field differs: {key}")
    violations += [f"baseline: {v}" for v in check_validity(base_meta)]
    violations += [f"optimized: {v}" for v in check_validity(opt_meta)]
    return violations
