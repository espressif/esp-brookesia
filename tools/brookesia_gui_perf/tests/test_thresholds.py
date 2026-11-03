# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import pathlib
import unittest

from brookesia_gui_perf import spec_coverage

from brookesia_gui_perf.compare import load_thresholds


class ThresholdsTests(unittest.TestCase):
    def test_threshold_names_exist_in_the_shipped_baseline(self):
        """A threshold keyed by a metric the carrier never emits can never fire.

        avg_fps sat in the table while the carrier emitted fps, so the rule was
        silently skipped. Checking the keys against the shipped baseline report
        catches that drift without rejecting single-scenario comparisons.
        """
        baseline = pathlib.Path(__file__).resolve().parents[3] / "reports" / "baseline" \
            / "esp32_p4x_function_ev" / "Baseline.esp32_p4x_function_ev.json"
        if not baseline.is_file():
            self.skipTest(f"baseline report not available: {baseline}")
        report = json.loads(baseline.read_text(encoding="utf-8"))
        emitted = set()
        for scenario in report.get("scenarios", {}).values():
            emitted.update(scenario.get("metrics", {}))
        # Metrics that §5.1 declares as not yet instrumented are allowed to have a
        # threshold without appearing in the report; anything else is drift.
        allowed = set(emitted)
        for entry in spec_coverage._FIELDS.values():
            if entry.get("status") in (spec_coverage.PENDING, spec_coverage.NOT_APPLICABLE):
                allowed.update(entry.get("metrics", []))
        # Thresholds whose metric the carrier does not emit yet. They are listed
        # explicitly so a new typo fails this test instead of joining the blind spot;
        # each one needs either instrumentation or an explicit record_only decision.
        known_uninstrumented = {
            "alloc_resident_pct", "build_copy_bytes", "doc_load_ms",
            "doc_load_transient_kb", "first_frame_ms", "first_visible_ms",
            "page_switch_ms", "task_heap_max_kb",
        }
        allowed |= known_uninstrumented
        table = load_thresholds()
        unknown = sorted(name for name, row in table.items()
                         if not row.get("record_only") and name not in allowed)
        self.assertEqual(unknown, [], f"threshold keys never emitted by the carrier: {unknown}")

    def test_required_rows_present(self):
        table = load_thresholds()
        required = [
            "first_frame_ms", "feedback_delay_ms", "event_dispatch_ms",
            "first_visible_ms", "page_switch_ms", "mount_ms", "doc_load_ms",
            "fps", "frame_time_ms", "jank_rate_pct",
            "hitch_ratio_ms_s", "frozen_frame_count", "frame_overrun_mean_ms",
            "refresh_area_ratio_pct", "flush_apply_ms",
            "batch_invalidate_ms", "binding_saturated_updates_per_s",
            "sram_peak_kb", "psram_peak_kb",
            "doc_load_transient_kb", "build_copy_bytes",
            "alloc_resident_pct", "task_heap_max_kb",
        ]
        for row in required:
            self.assertIn(row, table, f"missing §4 row: {row}")

    def test_row_schema(self):
        table = load_thresholds()
        row = table["first_frame_ms"]
        self.assertEqual(row["direction"], "lower_better")
        self.assertEqual(row["judge"], "p95")
        self.assertEqual(row["pass_b_max"], 150)
        self.assertEqual(row["fail_above"], 300)

    def test_mount_uses_page_switch_bands(self):
        table = load_thresholds()
        self.assertEqual(table["mount_ms"]["judge"], "p95")
        self.assertEqual(table["mount_ms"]["pass_a_max"], 150)
        self.assertEqual(table["mount_ms"]["pass_b_max"], 300)
        self.assertEqual(table["mount_ms"]["fail_above"], 500)

    def test_cv_exempt_only_where_cv_exceeds_spec_limit(self):
        table = load_thresholds()
        # Per-second dispatch and per-tap feedback are jitter metrics: a real P4 capture
        # showed cv 6.5 % (flush_apply_ms) and 35.4 % (feedback_delay_ms), so the
        # mean-based cv gate rejected a report against itself. Those rows opt out and are
        # graded on p95 instead; everything else must still pass the cv gate.
        for key in ("event_dispatch_ms", "feedback_delay_ms", "flush_apply_ms"):
            self.assertTrue(table[key].get("cv_exempt"), key)
        self.assertFalse(table["frame_time_ms"].get("cv_exempt", False))

    def test_fps_is_higher_better_mean(self):
        table = load_thresholds()
        self.assertEqual(table["fps"]["direction"], "higher_better")
        self.assertEqual(table["fps"]["judge"], "mean")
        self.assertEqual(table["fps"]["pass_b_min"], 40)

    def test_relative_and_budget_rows(self):
        table = load_thresholds()
        self.assertEqual(table["batch_invalidate_ms"]["relative"], "max")
        self.assertEqual(table["binding_saturated_updates_per_s"]["relative"], "min")
        self.assertEqual(table["sram_peak_kb"]["budget"], "sram")
        for key in ("doc_load_transient_kb", "build_copy_bytes", "alloc_resident_pct", "task_heap_max_kb"):
            self.assertEqual(table[key]["relative"], "max", key)


if __name__ == "__main__":
    unittest.main()
