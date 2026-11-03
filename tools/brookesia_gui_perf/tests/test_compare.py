# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import pathlib
import unittest

from brookesia_gui_perf.compare import compare_reports, grade, improvement_pct, load_thresholds
from brookesia_gui_perf.report import build_report
from tests.test_report import make_records


def scaled_report(scale: float):
    records = make_records(33)
    for sample in records.samples[3:]:
        sample["metrics"]["first_frame_ms"] = 100.0 * scale + (sample["sample_index"] % 7)
    return build_report(records)


class CompareTests(unittest.TestCase):
    def test_improvement_directions(self):
        self.assertAlmostEqual(improvement_pct("lower_better", 200.0, 100.0), 50.0)
        self.assertAlmostEqual(improvement_pct("higher_better", 100.0, 150.0), 50.0)
        self.assertAlmostEqual(improvement_pct("lower_better", 100.0, 150.0), -50.0)
        self.assertEqual(improvement_pct("lower_better", 0.0, 0.0), 0.0)
        with self.assertRaises(ValueError):
            improvement_pct("lower_better", 0.0, 1.0)

    def test_absolute_grading(self):
        row = load_thresholds()["first_frame_ms"]
        self.assertEqual(grade(row, 70.0, baseline_value=None, metadata={}), "pass_a")
        self.assertEqual(grade(row, 140.0, baseline_value=None, metadata={}), "pass_b")
        self.assertEqual(grade(row, 400.0, baseline_value=None, metadata={}), "fail")

    def test_t_refresh_resolution(self):
        row = load_thresholds()["frame_overrun_mean_ms"]
        meta = {"display": {"refresh_period_ms": 16.7}}
        self.assertEqual(grade(row, 16.0, baseline_value=None, metadata=meta), "pass_b")
        self.assertEqual(grade(row, 40.0, baseline_value=None, metadata=meta), "fail")

    def test_budget_grading(self):
        row = load_thresholds()["sram_peak_kb"]
        meta = {"mem_budget_kb": {"sram": 1000, "psram": 8192}}
        self.assertEqual(grade(row, 1000.0, baseline_value=None, metadata=meta), "pass_a")
        self.assertEqual(grade(row, 1090.0, baseline_value=None, metadata=meta), "pass_b")
        self.assertEqual(grade(row, 1300.0, baseline_value=None, metadata=meta), "fail")

    def test_relative_first_round_is_record(self):
        row = load_thresholds()["batch_invalidate_ms"]
        self.assertEqual(grade(row, 12.0, baseline_value=None, metadata={}), "record")
        self.assertEqual(grade(row, 13.0, baseline_value=10.0, metadata={}), "fail")
        self.assertEqual(grade(row, 10.5, baseline_value=10.0, metadata={}), "pass_b")

    def test_band_between_pass_b_and_fail_is_fail(self):
        """Acceptance is Pass-B and above, so a value worse than pass_b fails.

        The spec table's Fail column is the rejection boundary, but a value
        that misses pass_b is not acceptable regardless of whether it is still
        short of the Fail bound, so that band is graded fail.
        """
        row = {
            "direction": "lower_better",
            "judge": "p95",
            "pass_a_max": 80,
            "pass_b_max": 150,
            "fail_above": 300,
        }
        self.assertEqual(grade(row, 80.0, baseline_value=None, metadata={}), "pass_a")
        self.assertEqual(grade(row, 150.0, baseline_value=None, metadata={}), "pass_b")
        self.assertEqual(grade(row, 200.0, baseline_value=None, metadata={}), "fail")

    def test_compare_improvement_gate(self):
        base = scaled_report(1.0)
        opt = scaled_report(0.85)
        result = compare_reports(base, opt)
        metric = result["metrics"]["app_a_scroll"]["first_frame_ms"]
        self.assertGreaterEqual(metric["improvement_pct"], 10.0)
        self.assertTrue(metric["ok"])
        self.assertTrue(result["pass"])

    def test_flush_apply_uses_default_tolerance(self):
        self.assertEqual(load_thresholds()["flush_apply_ms"].get("regression_tolerance_pct"), 2.0)

    def test_composite_stress_is_record_only(self):
        from brookesia_gui_perf.compare import RECORD_ONLY_SCENARIOS
        self.assertIn("composite_stress", RECORD_ONLY_SCENARIOS)

    def test_record_only_grade_is_record(self):
        row = load_thresholds()["refresh_area_ratio_pct"]
        self.assertTrue(row.get("record_only"))
        self.assertEqual(grade(row, 100.0, baseline_value=100.0, metadata={}), "record")

    def test_missing_gated_metric_breaks_coverage(self):
        base = scaled_report(1.0)
        opt = scaled_report(1.0)
        for scenario in opt["scenarios"].values():
            scenario["metrics"].pop("first_frame_ms", None)
        result = compare_reports(base, opt)
        self.assertFalse(result["pass"])
        self.assertTrue(any(g["metric"] == "first_frame_ms" for g in result["coverage_gaps"]))

    def test_empty_optimized_is_not_a_pass(self):
        base = scaled_report(1.0)
        opt = scaled_report(1.0)
        opt["scenarios"] = {}
        result = compare_reports(base, opt)
        self.assertFalse(result["pass"])
        self.assertTrue(result["coverage_gaps"])

    def test_relative_rule_is_not_short_circuited_by_baseline_record(self):
        base = scaled_report(1.0)
        opt = scaled_report(3.0)
        thr = {"first_frame_ms": {"direction": "lower_better", "judge": "p95", "relative": "max"}}
        row = compare_reports(base, opt, thresholds=thr)["metrics"]["app_a_scroll"]["first_frame_ms"]
        self.assertEqual(row["optimized_grade"], "fail")
        self.assertFalse(row["ok"])

    def test_same_metric_across_scenarios_keeps_both_results(self):
        base = scaled_report(1.0)
        opt = scaled_report(1.0)
        worse = json.loads(json.dumps(opt["scenarios"]["app_a_scroll"]))
        for stats in worse["metrics"].values():
            if not isinstance(stats, dict) or "mean" not in stats:
                continue
            for key in ("mean", "p50", "p95", "p99"):
                stats[key] *= 20.0
        opt["scenarios"]["second_scene"] = worse
        base["scenarios"]["second_scene"] = json.loads(json.dumps(base["scenarios"]["app_a_scroll"]))
        result = compare_reports(base, opt)
        self.assertFalse(result["pass"])
        self.assertFalse(result["metrics"]["second_scene"]["first_frame_ms"]["ok"])
        self.assertTrue(result["metrics"]["app_a_scroll"]["first_frame_ms"]["ok"])

    def test_sub_tolerance_delta_is_not_a_regression(self):
        base = scaled_report(1.0)
        opt = scaled_report(1.005)
        metric = compare_reports(base, opt)["metrics"]["app_a_scroll"]["first_frame_ms"]
        self.assertFalse(metric["regressed"])

    def test_compare_catches_fail_grade(self):
        base = scaled_report(0.9)
        opt = scaled_report(3.5)
        result = compare_reports(base, opt)
        metric = result["metrics"]["app_a_scroll"]["first_frame_ms"]
        self.assertEqual(metric["optimized_grade"], "fail")
        self.assertFalse(metric["ok"])
        self.assertFalse(result["pass"])

    def test_compare_catches_regression_within_bands(self):
        base = scaled_report(1.0)
        opt = scaled_report(1.05)
        result = compare_reports(base, opt)
        metric = result["metrics"]["app_a_scroll"]["first_frame_ms"]
        self.assertTrue(metric["regressed"])
        self.assertFalse(metric["ok"])
        self.assertFalse(result["pass"])


if __name__ == "__main__":
    unittest.main()


class UnknownThresholdTests(unittest.TestCase):
    """A threshold keyed by a misspelled metric must not be silently skipped."""

    BASELINE = pathlib.Path(__file__).resolve().parents[3] / "reports" / "baseline" \
        / "esp32_p4x_function_ev" / "Baseline.esp32_p4x_function_ev.json"

    def _report(self):
        return json.loads(self.BASELINE.read_text(encoding="utf-8"))

    def _thresholds_with_typo(self):
        table = load_thresholds()
        table["fps_typo"] = dict(table["fps"])
        return table

    def test_typo_fails_without_extra_flags(self):
        report = self._report()
        result = compare_reports(report, report, thresholds=self._thresholds_with_typo())
        self.assertFalse(result["pass"])
        self.assertIn("fps_typo", result["unknown_thresholds"])

    def test_typo_is_reported(self):
        report = self._report()
        result = compare_reports(report, report, thresholds=self._thresholds_with_typo())
        self.assertIn("fps_typo", result["unknown_thresholds"])

    def test_typo_fails_on_an_otherwise_passing_pair(self):
        base, opt = scaled_report(1.0), scaled_report(0.9)
        catalog = set(load_thresholds())
        self.assertTrue(compare_reports(base, opt, metric_catalog=catalog)["pass"])
        table = load_thresholds()
        table["fps_typo"] = dict(table["fps"])
        self.assertFalse(compare_reports(base, opt, thresholds=table,
                                         metric_catalog=catalog)["pass"])

    def test_missing_catalog_fails_closed(self):
        base, opt = scaled_report(1.0), scaled_report(0.9)
        self.assertFalse(compare_reports(base, opt, metric_catalog=set())["pass"])

    def test_metric_catalog_ships_with_the_package(self):
        from brookesia_gui_perf.compare import _shipped_baseline_metrics
        catalog = _shipped_baseline_metrics()
        self.assertIsNotNone(catalog)
        self.assertIn("fps", catalog)

    def test_unknown_provenance_needs_an_explicit_exemption(self):
        from brookesia_gui_perf.gate import check_gate
        base, opt = scaled_report(1.0), scaled_report(0.95)
        for report in (base, opt):
            report["metadata"]["worktree_state_known"] = False
            report["metadata"]["worktree_exempt"] = False
        self.assertNotEqual(check_gate(base, opt), [])
        for report in (base, opt):
            report["metadata"]["worktree_exempt"] = True
        self.assertEqual(check_gate(base, opt), [])

    def test_record_only_rows_alone_are_not_a_pass(self):
        base, opt = scaled_report(1.0), scaled_report(0.95)
        for report in (base, opt):
            for scenario in report["scenarios"].values():
                scenario["metrics"] = {k: v for k, v in scenario["metrics"].items()
                                       if k == "refresh_area_ratio_pct"}
        self.assertFalse(compare_reports(base, opt, metric_catalog=set(load_thresholds()))["pass"])

    def test_declared_pending_metrics_do_not_count_as_unknown(self):
        report = self._report()
        result = compare_reports(report, report)
        self.assertNotIn("doc_load_ms", result["unknown_thresholds"])
        self.assertNotIn("task_heap_max_kb", result["unknown_thresholds"])
