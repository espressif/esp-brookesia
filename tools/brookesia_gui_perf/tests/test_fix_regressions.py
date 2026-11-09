# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Regression tests for the paired-verdict and capture-integrity fixes.

Each case below reproduces a concrete defect that let an invalid run pass the
host gate. They are kept together so a future change to the verdict semantics
has one place that states what must never be accepted again.
"""

import json
import unittest
from pathlib import Path

from brookesia_gui_perf.compare import compare_reports, load_thresholds
from brookesia_gui_perf.gate import check_gate
from brookesia_gui_perf.report import build_report, validate_report
from brookesia_gui_perf.stats import aggregate, cv_ok
from tests.test_report import make_records

ROOT = Path(__file__).resolve().parents[3]
BASELINE = (
    ROOT / "reports" / "baseline" / "esp32_p4x_function_ev"
    / "Baseline.esp32_p4x_function_ev.json"
)


def graded_stats(value):
    return {"mean": value, "std": 0.0, "cv": 0.0, "p50": value,
            "p75": value, "p95": value, "p99": value, "n": 32}


class PairedVerdictTests(unittest.TestCase):
    """Spec section 5.2 step 3: improvement >= 10% AND no Fail item."""

    @staticmethod
    def _pair(baseline_value, optimized_value):
        name = "first_frame_ms"  # pass_a_max=80, pass_b_max=150 -> both values Fail
        base = {"metadata": {}, "scenarios": {"x": {"metrics": {name: graded_stats(baseline_value)}}}}
        opt = {"metadata": {}, "scenarios": {"x": {"metrics": {name: graded_stats(optimized_value)}}}}
        return name, base, opt

    def test_large_improvement_does_not_mask_absolute_fail(self):
        name, base, opt = self._pair(1000.0, 800.0)
        result = compare_reports(base, opt, metric_catalog=set(load_thresholds()))
        row = result["metrics"]["x"][name]
        self.assertEqual(row["optimized_grade"], "fail")
        self.assertAlmostEqual(row["improvement_pct"], 20.0)
        self.assertFalse(row["ok"])
        self.assertFalse(result["pass"])

    def test_unregressed_pass_b_still_passes(self):
        """The fix must not over-tighten: a flat, passing pair remains a pass."""
        name, base, opt = self._pair(100.0, 100.0)
        result = compare_reports(base, opt, metric_catalog=set(load_thresholds()))
        row = result["metrics"]["x"][name]
        self.assertEqual(row["optimized_grade"], "pass_b")
        self.assertTrue(row["ok"])
        self.assertTrue(result["pass"])

    def test_regression_still_fails(self):
        _, base, opt = self._pair(100.0, 200.0)
        result = compare_reports(base, opt, metric_catalog=set(load_thresholds()))
        self.assertFalse(result["pass"])

    def test_record_only_row_high_cv_does_not_veto_verdict(self):
        """refresh_area_ratio_pct is record_only with no cv_exempt flag."""
        name = "refresh_area_ratio_pct"
        stats = dict(graded_stats(40.0), cv=0.2)
        base = {"metadata": {}, "scenarios": {"x": {"metrics": {name: stats}}}}
        opt = {"metadata": {}, "scenarios": {"x": {"metrics": {name: stats}}}}
        result = compare_reports(base, opt, metric_catalog=set(load_thresholds()))
        row = result["metrics"]["x"][name]
        self.assertEqual(row["optimized_grade"], "record")
        self.assertTrue(row["ok"])


class CaptureIntegrityTests(unittest.TestCase):
    """Spec section 1.2: a run without >=30 usable samples is not a baseline."""

    def test_healthy_report_still_validates(self):
        self.assertEqual(validate_report(build_report(make_records(33))), [])

    def test_shipped_baseline_still_validates_and_pairs(self):
        """The new checks must not invalidate the already captured P4X baseline."""
        report = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.assertEqual(validate_report(report), [])
        self.assertEqual(check_gate(report, report), [])

    def test_shipped_baseline_verdict_is_unchanged(self):
        """Guard the baseline's paired verdict, not just its metadata pairing."""
        report = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.assertTrue(compare_reports(report, report)["pass"])

    def test_single_torn_record_is_tolerated(self):
        """One torn UART line is normal on this board and must not lock out a run."""
        report = build_report(make_records(33))
        report["malformed_records"] = 1
        self.assertEqual(validate_report(report), [])
        self.assertEqual(check_gate(report, report), [])

    def test_torn_and_empty_capture_is_rejected_by_gate(self):
        report = build_report(make_records(33))
        report["sample_count"] = 0
        report["malformed_records"] = 12
        for body in report["scenarios"].values():
            body["insufficient_metrics"] = {"fps": 27}
            break
        violations = validate_report(report)
        self.assertTrue(any("sample_count" in v for v in violations))
        self.assertTrue(any("malformed_records" in v for v in violations))
        self.assertTrue(any("only 27 samples" in v for v in violations))
        self.assertNotEqual(check_gate(report, report), [])

    def test_sample_count_below_minimum_is_rejected(self):
        # Keep the key present: REQUIRED_TOP_KEYS already rejects a missing key,
        # so only this assertion proves the new minimum-sample check works.
        report = build_report(make_records(33))
        report["sample_count"] = 29
        self.assertTrue(any("below the section 1.2 minimum" in v for v in validate_report(report)))


class CvGateTests(unittest.TestCase):
    def test_negative_mean_does_not_disable_the_noise_gate(self):
        stats = aggregate([-1.0, -100.0] * 15)
        self.assertGreater(stats["cv"], 0.05)
        self.assertFalse(cv_ok(stats))

    def test_non_finite_and_negative_cv_are_rejected(self):
        for value in (float("nan"), float("inf"), -0.5):
            self.assertFalse(cv_ok({"cv": value}))


class TruncatedCaptureTests(unittest.TestCase):
    """A truncated capture must never become a report."""

    def _log_without_marker(self, path):
        records = make_records(40)
        header = json.dumps({
            "record": "metadata",
            "test_suite": records.metadata["test_suite"],
            "app_id": records.metadata["app_id"],
            "runtime_type": records.metadata["runtime_type"],
            "metadata": records.metadata["metadata"],
        })
        lines = [f"[BENCH] {header}"] + [f"[BENCH] {json.dumps(s)}" for s in records.samples]
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def test_aggregate_rejects_a_log_without_the_end_marker(self):
        import tempfile
        from brookesia_gui_perf.cli import main
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "truncated.log"
            out = Path(tmp) / "out.json"
            self._log_without_marker(log)
            self.assertEqual(main(["aggregate", str(log), "-o", str(out)]), 1)
            self.assertFalse(out.exists())

    def test_two_torn_records_are_tolerated_on_a_32_sample_capture(self):
        if not BASELINE.is_file():
            self.skipTest(f"baseline not available: {BASELINE}")
        report = json.loads(BASELINE.read_text(encoding="utf-8"))
        report["malformed_records"] = 2
        self.assertEqual(validate_report(report), [])
        report["malformed_records"] = 3
        self.assertTrue(any("malformed_records" in v for v in validate_report(report)))


class RelativeRowToleranceTests(unittest.TestCase):
    """Section 4 grades a relative row on its own 10% band, not the 1% noise floor."""

    NAME = "batch_invalidate_ms"  # relative:max, Fail is > baseline + 10%

    def _result(self, ratio):
        base_value = 26.599
        base = {"metadata": {}, "scenarios": {"x": {"metrics": {self.NAME: graded_stats(base_value)}}}}
        opt = {"metadata": {}, "scenarios": {"x": {"metrics": {self.NAME: graded_stats(base_value * ratio)}}}}
        return compare_reports(base, opt, metric_catalog=set(load_thresholds()))

    def test_same_firmware_drift_inside_the_spec_band_passes(self):
        result = self._result(1.024)  # +2.4%: observed between two identical runs
        row = result["metrics"]["x"][self.NAME]
        self.assertFalse(row["regressed"], row)
        self.assertTrue(row["ok"], row)
        self.assertTrue(result["pass"], row)

    def test_beyond_the_spec_band_still_fails(self):
        result = self._result(1.15)  # +15% > +10%
        row = result["metrics"]["x"][self.NAME]
        self.assertFalse(row["ok"], row)
        self.assertFalse(result["pass"], row)


class ZeroBaselineRatioTests(unittest.TestCase):
    """A zero baseline cannot define a ratio; the bands must still decide."""

    def _pair(self, opt_p95):
        report = json.loads(BASELINE.read_text(encoding="utf-8"))
        base = {"metadata": report["metadata"], "scenarios": report["scenarios"]}
        opt = json.loads(json.dumps(base))
        name = "event_dispatch_ms"  # feedback_delay arm: pass_a_max=10 ms
        opt["scenarios"]["feedback_delay"]["metrics"].setdefault(
            name, {"mean": opt_p95, "std": 0.0, "cv": 0.0, "p50": opt_p95,
                   "p75": opt_p95, "p95": opt_p95, "p99": opt_p95, "n": 37}
        )["p95"] = opt_p95
        return compare_reports(base, opt, metric_catalog=set(load_thresholds()))

    def test_zero_to_half_threshold_is_graded_not_raising(self):
        result = self._pair(0.5)
        row = result["metrics"]["feedback_delay"]["event_dispatch_ms"]
        self.assertIsNone(row["improvement_pct"])
        self.assertEqual(row["optimized_grade"], "pass_a")
        self.assertTrue(row["ok"], row)
        self.assertTrue(result["pass"], row)

    def test_zero_to_over_threshold_fails(self):
        result = self._pair(100.0)  # > fail_above 50 ms
        row = result["metrics"]["feedback_delay"]["event_dispatch_ms"]
        self.assertEqual(row["optimized_grade"], "fail")
        self.assertFalse(row["ok"], row)
        self.assertFalse(result["pass"], row)

    def test_zero_to_zero_passes_without_raising(self):
        result = self._pair(0.0)
        row = result["metrics"]["feedback_delay"]["event_dispatch_ms"]
        self.assertEqual(row["optimized_grade"], "pass_a")
        self.assertTrue(result["pass"], row)

    def test_cli_compare_survives_the_zero_baseline_pair(self):
        import tempfile
        from brookesia_gui_perf.cli import main
        with tempfile.TemporaryDirectory() as tmp:
            report = json.loads(BASELINE.read_text(encoding="utf-8"))
            report["scenarios"]["feedback_delay"]["metrics"].setdefault(
                "event_dispatch_ms",
                {"mean": 0.5, "std": 0.0, "cv": 0.0, "p50": 0.5, "p75": 0.5,
                 "p95": 0.5, "p99": 0.5, "n": 37},
            )["p95"] = 0.5
            base = Path(tmp) / "base.json"
            opt = Path(tmp) / "opt.json"
            base.write_text(json.dumps(json.loads(BASELINE.read_text(encoding="utf-8"))), encoding="utf-8")
            opt.write_text(json.dumps(report), encoding="utf-8")
            rc = main(["compare", str(base), str(opt)])
            self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
