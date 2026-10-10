# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import unittest

from brookesia_gui_perf.compare import load_thresholds


class ThresholdsTests(unittest.TestCase):
    def test_required_rows_present(self):
        table = load_thresholds()
        required = [
            "first_frame_ms", "feedback_delay_ms", "event_dispatch_ms",
            "first_visible_ms", "page_switch_ms", "mount_ms", "doc_load_ms",
            "avg_fps", "frame_time_ms", "jank_rate_pct",
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

    def test_event_timing_metrics_are_cv_exempt(self):
        table = load_thresholds()
        for key in ("feedback_delay_ms", "flush_apply_ms", "event_dispatch_ms"):
            self.assertTrue(table[key].get("cv_exempt"), key)

    def test_fps_is_higher_better_mean(self):
        table = load_thresholds()
        self.assertEqual(table["avg_fps"]["direction"], "higher_better")
        self.assertEqual(table["avg_fps"]["judge"], "mean")
        self.assertEqual(table["avg_fps"]["pass_b_min"], 40)

    def test_relative_and_budget_rows(self):
        table = load_thresholds()
        self.assertEqual(table["batch_invalidate_ms"]["relative"], "max")
        self.assertEqual(table["binding_saturated_updates_per_s"]["relative"], "min")
        self.assertEqual(table["sram_peak_kb"]["budget"], "sram")
        for key in ("doc_load_transient_kb", "build_copy_bytes", "alloc_resident_pct", "task_heap_max_kb"):
            self.assertEqual(table[key]["relative"], "max", key)


if __name__ == "__main__":
    unittest.main()
