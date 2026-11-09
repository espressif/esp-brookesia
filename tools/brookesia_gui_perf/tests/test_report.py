# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import unittest

from brookesia_gui_perf.parser import RunRecords
from brookesia_gui_perf.report import build_report, judgment_value, validate_report

META_KEYS = [
    "board_id", "git_sha", "idf_version", "idf_preview", "sdkconfig_hash",
    "cpu_freq_mhz", "lvgl_version", "esp_lv_adapter_version",
    "json_ui_document_version", "fixture_version", "fixture_provenance",
    "golden_case", "resource_scheme", "mem_budget_kb", "display", "touch",
    "core_affinity", "profile_switches", "fast_action_dispatch", "doc_complexity",
    "stat_basis", "present_semantics", "benchmark_validity",
]


def full_metadata():
    meta = {k: None for k in META_KEYS}
    meta.update(
        board_id="esp32_p4x_function_ev",
        git_sha="abc1234",
        fixture_provenance="in_repo_super",
        stat_basis="p95_for_latency",
        present_semantics="submitted",
        mem_budget_kb={"sram": 1024, "psram": 8192},
        display={"refresh_period_ms": 16.7},
        benchmark_validity={
            "draw_buf_ratio": 0.25,
            "asserts_enabled": False,
            "opt_enabled": True,
            "log_level": "WARN",
            "heap_trace_overflow": False,
        },
    )
    return meta


def make_records(n_samples=33, warmup_values=(999.0, 998.0, 997.0)):
    metadata = {
        "record": "metadata",
        "test_suite": "ESP-Brookesia-Perf",
        "app_id": "com.bench.app_a",
        "runtime_type": "JSON_UI",
        "metadata": full_metadata(),
    }
    samples = []
    kept = list(range(n_samples - len(warmup_values)))
    values = list(warmup_values) + [100.0 + i for i in kept]
    for i, v in enumerate(values):
        samples.append(
            {
                "record": "sample",
                "scenario": "app_a_scroll",
                "sample_index": i,
                "metrics": {"first_frame_ms": v, "note": "steady"},
            }
        )
    return RunRecords(metadata=metadata, samples=samples)


class ReportTests(unittest.TestCase):
    def test_warmup_dropped_then_min_samples(self):
        report = build_report(make_records(33))
        self.assertEqual(report["sample_count"], 30)
        stats = report["scenarios"]["app_a_scroll"]["metrics"]["first_frame_ms"]
        self.assertEqual(stats["n"], 30)
        self.assertLess(stats["mean"], 150.0)  # warmup 999 outliers excluded

    def test_too_few_samples_is_flagged_not_fatal(self):
        # A metric with fewer than the minimum samples (e.g. a torn UART record that
        # corrupted one key) must not abort the whole aggregate; it is skipped and its
        # shortfall recorded so the gap stays explicit rather than silently dropped.
        report = build_report(make_records(30))  # 30 - 3 warmup = 27
        scenario = report["scenarios"]["app_a_scroll"]
        self.assertNotIn("first_frame_ms", scenario["metrics"])
        self.assertEqual(scenario["insufficient_metrics"]["first_frame_ms"], 27)

    def test_non_numeric_passthrough(self):
        report = build_report(make_records(33))
        note = report["scenarios"]["app_a_scroll"]["metrics"]["note"]
        self.assertEqual(note, {"value": "steady"})

    def test_validate_full_report_passes(self):
        report = build_report(make_records(33))
        self.assertEqual(validate_report(report), [])

    def test_validate_missing_metadata_field(self):
        report = build_report(make_records(33))
        del report["metadata"]["present_semantics"]
        violations = validate_report(report)
        self.assertTrue(any("present_semantics" in v for v in violations))

    def test_judgment_value(self):
        stats = {"mean": 1.0, "p95": 2.0, "p99": 3.0}
        self.assertEqual(judgment_value(stats, "p95"), 2.0)
        with self.assertRaises(KeyError):
            judgment_value(stats, "p50")


class WorktreeMetadataTests(unittest.TestCase):
    def test_worktree_fields_present_in_report(self):
        from brookesia_gui_perf.report import build_report
        from tests.test_report import make_records

        records = make_records(40)
        report = build_report(records, worktree_dirty=True, worktree_diff_sha256="deadbeef")
        self.assertIs(report["metadata"]["worktree_dirty"], True)
        self.assertEqual(report["metadata"]["worktree_diff_sha256"], "deadbeef")

    def test_worktree_fields_always_present_with_defaults(self):
        from brookesia_gui_perf.report import build_report
        from tests.test_report import make_records

        report = build_report(make_records(40), worktree_diff_sha256="")
        # Defaults must still emit both keys; absence would let a dirty tree
        # pass unnoticed because the key would simply be absent.
        self.assertIn("worktree_dirty", report["metadata"])
        self.assertIn("worktree_diff_sha256", report["metadata"])

    def test_worktree_fields_not_in_required_set(self):
        from brookesia_gui_perf.report import REQUIRED_METADATA_FIELDS

        self.assertNotIn("worktree_dirty", REQUIRED_METADATA_FIELDS)
        self.assertNotIn("worktree_diff_sha256", REQUIRED_METADATA_FIELDS)


if __name__ == "__main__":
    unittest.main()
