# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""v1.8 §5.1 canonical-field coverage.

§六-6 fails a report whose mandatory memory items are *silently* missing
("缺失且无 N/A 声明即验收不通过").  This module enumerates the canonical §5.1
fields and resolves each to exactly one explicit status, so no field can be
silent:

  collected               - present in the report (data-driven)
  not_applicable          - genuinely inapplicable to this build (reason required)
  pending_instrumentation - required by v1.8 §3.3 but the emitter/hook is not built yet
                            (reason required; a documented gap, not a silent omission)

`assess_coverage(report)` returns ``{field: {"status": ..., "reason": ...}}``.
`uncovered(report)` returns fields with no explicit status (must be empty for a
report produced by `build_report`, which embeds this block into metadata).
"""

from typing import Any, Dict, List

COLLECTED = "collected"
NOT_APPLICABLE = "not_applicable"
PENDING = "pending_instrumentation"

# name -> (metrics, static_status, reason)
#   metrics: any scenario exposing one of these metric keys marks the field `collected`.
#   static_status/reason: used when the metric is absent.
# Order follows the v1.8 §5.1 sample schema.
_FIELDS: Dict[str, Dict[str, Any]] = {
    # --- frame / latency -------------------------------------------------
    "fps": {"metrics": ["fps"]},
    "fps_1pct_low": {
        "metrics": ["fps_1pct_low"],
        "status": PENDING,
        "reason": "frame-time series aggregated on-device; 1% low needs the raw per-frame samples",
    },
    "refresh_area_ratio_pct": {"metrics": ["refresh_area_ratio_pct"]},
    "flush_apply_ms": {"metrics": ["flush_apply_ms"]},
    "binding_saturated_updates_per_s": {
        "metrics": ["binding_saturated_updates_per_s"],
        "status": PENDING,
        "reason": "ladder recorded as flush_apply_ladder_ms; saturation point not yet derived",
    },
    "batch_invalidate_ms": {"metrics": ["batch_invalidate_ms"]},
    # --- heap free / peak / steady --------------------------------------
    "sram_free_kb": {"metrics": ["sram_free_kb"]},
    "psram_free_kb": {"metrics": ["psram_free_kb"]},
    "sram_peak_kb": {"metrics": ["sram_peak_kb"]},
    "psram_peak_kb": {"metrics": ["psram_peak_kb"]},
    "sram_steady_kb": {
        "status": PENDING,
        "reason": "steady-state free recorded (steady_state.sram_free_kb); steady_kb not derived",
    },
    "psram_steady_kb": {
        "status": PENDING,
        "reason": "steady-state free recorded (steady_state.psram_free_kb); steady_kb not derived",
    },
    # --- fragmentation ---------------------------------------------------
    "sram_frag_rate_pct": {
        "status": PENDING,
        "reason": "heap_caps_get_info() fragmentation rate not emitted",
    },
    "psram_frag_rate_pct": {
        "status": PENDING,
        "reason": "heap_caps_get_info() fragmentation rate not emitted",
    },
    "frag_trend": {
        "status": PENDING,
        "reason": "largest block recorded (steady_state.*_largest_kb); free-block count/trend not emitted",
    },
    # --- LVGL heap view --------------------------------------------------
    "lvgl_mem": {
        "status": NOT_APPLICABLE,
        "reason": "CLIB malloc build: LVGL heap view N/A (v1.8 §3.3)",
    },
    # --- task stacks -----------------------------------------------------
    "stack_hwm_kb": {
        "status": PENDING,
        "reason": "display/worker/touch stack high-water marks not instrumented",
    },
    # --- allocation behaviour -------------------------------------------
    "alloc_churn_per_s": {
        "status": PENDING,
        "reason": "HeapTrace alloc/free churn counter not emitted",
    },
    "oom_events": {"metrics": ["oom_events"]},
    # --- runtime inner heap ---------------------------------------------
    "runtime_heap_kb": {
        "status": NOT_APPLICABLE,
        "reason": "runtime_type=JSON_UI: no QuickJS/WASM/Lua inner heap",
    },
    "runtime_heap_note": {
        "status": NOT_APPLICABLE,
        "reason": "runtime_type=JSON_UI: no inner runtime heap",
    },
    # --- normalised memory ----------------------------------------------
    "bytes_per_lvgl_object": {
        "status": PENDING,
        "reason": "runtime LVGL object count not instrumented",
    },
    "bytes_per_json_node": {
        "status": PENDING,
        "reason": "derivable from doc_complexity.nodes + heap, not yet emitted",
    },
    # --- attributed memory ----------------------------------------------
    "mem_attr_kb": {
        "status": PENDING,
        "reason": "HeapTrace per-attribute (noderecord/style_cache/images) attribution not emitted",
    },
    # --- build-time memory (v1.8 §3.3 mandatory) ------------------------
    "doc_load_transient_kb": {
        "status": PENDING,
        "reason": "v1.8 §3.3 required: parse/build stage peak (4 capture hooks) not built",
    },
    "build_copy_bytes": {
        "status": PENDING,
        "reason": "v1.8 §3.3 required: per-path copy byte counter not built",
    },
    "alloc_resident_pct": {
        "status": PENDING,
        "reason": "v1.8 §3.3 required: allocation-lifetime resident pct not built",
    },
    "task_heap_max_kb": {
        "status": PENDING,
        "reason": "v1.8 §3.3 required: per-task heap peak (CONFIG_HEAP_TASK_TRACKING) not built",
    },
    # --- cache hit rates -------------------------------------------------
    "style_cache_hit_rate_pct": {
        "status": PENDING,
        "reason": "gui.style_cache hit counter not emitted",
    },
    "image_preload_hit_rate_pct": {
        "status": PENDING,
        "reason": "image preload hit counter not emitted",
    },
    # --- bridge / FFI ----------------------------------------------------
    "ffi_call_us": {
        "status": NOT_APPLICABLE,
        "reason": "JSON_UI local binding path: no JS<->Native FFI segments",
    },
    # --- CPU -------------------------------------------------------------
    "cpu_pct": {
        "status": PENDING,
        "reason": "per-task CPU (FreeRTOS run-time stats) not emitted",
    },
    # --- firmware / resource size (host artefacts) ----------------------
    "firmware_bin_bytes": {
        "status": PENDING,
        "reason": "app.bin size is a host artefact (measure_size / --firmware-bin)",
    },
    "resource_image_bytes": {
        "status": PENDING,
        "reason": "littlefs/resource image size is a host artefact (--resource-image)",
    },
}

SPEC_5_1_FIELDS: List[str] = list(_FIELDS)


def canonical_fields() -> List[str]:
    """The v1.8 §5.1 field names, in schema order."""
    return list(SPEC_5_1_FIELDS)


def _report_metric_names(report: Dict[str, Any]) -> set:
    names = set()
    for body in (report.get("scenarios") or {}).values():
        names.update((body.get("metrics") or {}).keys())
    return names


def assess_coverage(report: Dict[str, Any]) -> Dict[str, Dict[str, str]]:
    """Resolve every canonical §5.1 field to an explicit status."""
    present = _report_metric_names(report)
    coverage: Dict[str, Dict[str, str]] = {}
    for field, rule in _FIELDS.items():
        hit = next((m for m in rule.get("metrics", []) if m in present), None)
        if hit is not None:
            coverage[field] = {"status": COLLECTED, "metric": hit}
        else:
            coverage[field] = {
                "status": rule.get("status", PENDING),
                "reason": rule.get("reason", "not collected"),
            }
    return coverage


def uncovered(report: Dict[str, Any]) -> List[str]:
    """Canonical fields that neither appear in the report nor carry a declaration."""
    embedded = (report.get("metadata") or {}).get("acceptance") or {}
    missing = []
    for field in SPEC_5_1_FIELDS:
        entry = embedded.get(field)
        if not entry or not entry.get("status"):
            missing.append(field)
    return missing


def summarize(coverage: Dict[str, Dict[str, str]]) -> Dict[str, int]:
    counts: Dict[str, int] = {COLLECTED: 0, NOT_APPLICABLE: 0, PENDING: 0}
    for entry in coverage.values():
        counts[entry["status"]] = counts.get(entry["status"], 0) + 1
    return counts


COVERED = "covered"
PARTIAL = "partial"
MISSING = "missing"

# v1.8 §3.9 dimension matrix: representative metrics per dimension. A dimension is
# `covered` when every listed metric is present, `partial` when some are, `missing`
# otherwise. §3.9 cross-rule (3) requires D1-D4 + D5-D7 in the same report.
DIMENSIONS: Dict[str, Dict[str, Any]] = {
    "D1": {"question": "complex UI load: which stage is slowest?", "metrics": ["flush_apply_ms"]},
    "D2": {"question": "first visible / page switch", "metrics": ["mount_ms"]},
    "D3": {"question": "interaction response", "metrics": ["feedback_delay_ms", "event_dispatch_ms"]},
    "D4": {
        "question": "frame quality",
        "metrics": ["fps", "jank_rate_pct", "hitch_ratio_ms_s", "frozen_frame_count", "refresh_area_ratio_pct"],
    },
    "D5": {"question": "build-time temporaries (v1.8)", "metrics": ["doc_load_transient_kb", "build_copy_bytes"]},
    "D6": {
        "question": "resident / placement",
        "metrics": ["sram_peak_kb", "psram_peak_kb", "task_heap_max_kb", "bytes_per_lvgl_object"],
    },
    "D7": {
        "question": "lifecycle (leak / frag / stack / churn)",
        "metrics": ["sram_leak_kb", "alloc_resident_pct", "stack_hwm_kb", "alloc_churn_per_s"],
    },
    "D8": {"question": "architecture overhead", "metrics": ["ffi_call_us", "runtime_heap_kb"]},
    "D9": {"question": "resource scheme S1-S4", "metrics": ["resource_scheme_comparison"]},
}

# §3.9 cross-rule (3): these are the dimensions a scheme-level conclusion MUST cover.
REQUIRED_DIMENSIONS = ("D1", "D2", "D3", "D4", "D5", "D6", "D7")


def assess_dimensions(report: Dict[str, Any]) -> Dict[str, Any]:
    """Per-dimension coverage plus the §3.9 cross-rule (3) completeness flag."""
    present = _report_metric_names(report)
    dims: Dict[str, Any] = {}
    for name, rule in DIMENSIONS.items():
        wanted = rule["metrics"]
        seen = [m for m in wanted if m in present]
        if len(seen) == len(wanted):
            status = COVERED
        elif seen:
            status = PARTIAL
        else:
            status = MISSING
        dims[name] = {"status": status, "metrics_seen": seen, "question": rule["question"]}
    missing_required = [d for d in REQUIRED_DIMENSIONS if dims[d]["status"] != COVERED]
    return {"dimensions": dims, "required": list(REQUIRED_DIMENSIONS), "missing": missing_required}


# Declared statistical exemptions (v1.8 1.2 CV<=5% / section 4 judgement). Anything
# here is a KNOWN weakening of the spec limit, so it must carry a reason and the
# measured CV. feedback_delay_ms is deliberately absent: its CV exceeds the limit
# and must be re-measured rather than exempted.
STAT_EXEMPTIONS: Dict[str, Dict[str, str]] = {
    "event_dispatch_ms": {
        "kind": "cv_exempt",
        "reason": "event timing on the serial SystemGui/SystemAppInput task group has structural jitter beyond the 5% limit",
    },
    "refresh_area_ratio_pct": {
        "kind": "record_only",
        "reason": "v1.8 section 4 first-round record semantics for interaction-scene area ratio",
    },
}


def assess_exemptions(report: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    declared: Dict[str, Dict[str, Any]] = {}
    for metric, info in STAT_EXEMPTIONS.items():
        measured = None
        for body in (report.get("scenarios") or {}).values():
            stats = (body.get("metrics") or {}).get(metric)
            if stats and "cv" in stats:
                measured = max(measured or 0.0, float(stats["cv"]))
        declared[metric] = {
            "kind": info["kind"],
            "measured_cv": measured,
            "limit": 0.05,
            "reason": info["reason"],
        }
    return declared
