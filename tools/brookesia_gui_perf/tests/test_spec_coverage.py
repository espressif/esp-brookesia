# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import unittest

from brookesia_gui_perf.report import build_report, validate_report
from brookesia_gui_perf.parser import RunRecords
from brookesia_gui_perf.spec_coverage import (
    COLLECTED,
    assess_coverage,
    assess_dimensions,
    canonical_fields,
    summarize,
    uncovered,
)


def _records():
    records = RunRecords()
    records.metadata = {
        "record": "metadata",
        "metadata": {
            "board_id": "esp32_p4_function_ev",
            "git_sha": "x",
            "idf_version": "6.0",
            "idf_preview": False,
            "sdkconfig_hash": "sha256:x",
            "cpu_freq_mhz": 360,
            "lvgl_version": "9",
            "esp_lv_adapter_version": "1",
            "json_ui_document_version": "0.1.1",
            "fixture_version": "3",
            "fixture_provenance": "in_repo_super",
            "golden_case": "super",
            "resource_scheme": "S1",
            "mem_budget_kb": {"sram": 512, "psram": 4096},
            "display": {"width_px": 800, "height_px": 480, "refresh_period_ms": 16.7},
            "touch": {},
            "core_affinity": {},
            "profile_switches": True,
            "fast_action_dispatch": False,
            "doc_complexity": {},
            "stat_basis": "p95",
            "present_semantics": "submitted",
            "benchmark_validity": {"draw_buf_ok": True},
        },
    }
    for _ in range(30):
        records.samples.append(
            {"scenario": "composite_stress", "metrics": {"fps": 48.0, "sram_free_kb": 100.0}}
        )
    return records


class TestSpecCoverage(unittest.TestCase):
    def test_canonical_fields_are_unique_and_nonempty(self):
        fields = canonical_fields()
        self.assertTrue(fields)
        self.assertEqual(len(fields), len(set(fields)))

    def test_every_field_gets_an_explicit_status(self):
        coverage = assess_coverage({"scenarios": {}})
        self.assertEqual(set(coverage), set(canonical_fields()))
        for entry in coverage.values():
            self.assertIn(entry["status"], {"collected", "not_applicable", "pending_instrumentation"})
            if entry["status"] != COLLECTED:
                self.assertTrue(entry.get("reason"))

    def test_present_metric_marks_field_collected(self):
        report = {"scenarios": {"s": {"metrics": {"fps": {"mean": 48.0}, "sram_free_kb": {}}}}}
        coverage = assess_coverage(report)
        self.assertEqual(coverage["fps"]["status"], COLLECTED)
        self.assertEqual(coverage["sram_free_kb"]["status"], COLLECTED)

    def test_build_report_embeds_acceptance_and_validates(self):
        report = build_report(_records(), warmup=0)
        self.assertIn("acceptance", report["metadata"])
        self.assertIn("dimension_coverage", report["metadata"])
        self.assertEqual(uncovered(report), [])
        self.assertEqual([v for v in validate_report(report) if "section 5.1" in v], [])

    def test_dimensions_flag_required_gaps(self):
        dims = assess_dimensions({"scenarios": {}})
        self.assertEqual(set(dims["dimensions"]), {"D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9"})
        self.assertEqual(dims["missing"], list(dims["required"]))
        covered = assess_dimensions(
            {"scenarios": {"s": {"metrics": {m: {} for m in (
                "fps", "jank_rate_pct", "hitch_ratio_ms_s", "frozen_frame_count",
                "refresh_area_ratio_pct", "mount_ms", "feedback_delay_ms", "event_dispatch_ms",
                "sram_leak_kb",
            )}}}}
        )
        self.assertEqual(covered["dimensions"]["D4"]["status"], "covered")
        self.assertEqual(covered["dimensions"]["D3"]["status"], "covered")

    def test_uncovered_flags_missing_declaration(self):
        self.assertEqual(uncovered({"metadata": {}, "scenarios": {}}), canonical_fields())

    def test_summarize_counts_cover_all_fields(self):
        counts = summarize(assess_coverage({"scenarios": {}}))
        self.assertEqual(sum(counts.values()), len(canonical_fields()))


if __name__ == "__main__":
    unittest.main()
