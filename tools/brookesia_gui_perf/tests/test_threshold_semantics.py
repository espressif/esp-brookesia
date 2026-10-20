# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Guard metric semantics and scenario applicability of the gate.

Both defects these tests lock down were found by reviewers after the numbers had
already been published: a peak metric whose direction was inverted, and an
absolute fps floor applied to scenarios that legitimately stop redrawing.
"""

import unittest

from brookesia_gui_perf.compare import grade, improvement_pct, load_thresholds, scenario_row


class PeakMetricDirectionTest(unittest.TestCase):
    def test_peak_metrics_are_used_memory(self):
        """sram_peak_kb is capacity minus the minimum free figure, so less is better."""
        thresholds = load_thresholds()
        for name in ("sram_peak_kb", "psram_peak_kb"):
            self.assertEqual(
                thresholds[name]["direction"],
                "lower_better",
                "%s reports peak used memory, so a smaller value is a healthier run" % name,
            )

    def test_using_more_memory_is_a_regression(self):
        """Used memory 112 kB -> 212 kB must read as a regression, and back as a win."""
        row = load_thresholds()["sram_peak_kb"]
        self.assertLess(improvement_pct(row["direction"], 112.0, 212.0), 0.0)
        self.assertGreater(improvement_pct(row["direction"], 212.0, 112.0), 0.0)


class ScenarioApplicabilityTest(unittest.TestCase):
    def test_static_scenarios_do_not_grade_fps(self):
        """A screen that stops redrawing reports fps 0 without being a failure."""
        row = load_thresholds()["fps"]
        for scenario in ("steady_state", "screen_cycle"):
            self.assertTrue(
                scenario_row(row, scenario).get("record"),
                "%s must not be graded against the sustained-load fps floor" % scenario,
            )

    def test_sustained_load_still_grades_fps(self):
        row = load_thresholds()["fps"]
        for scenario in ("render_stress", "composite_stress"):
            effective = scenario_row(row, scenario)
            self.assertFalse(effective.get("record"))
            self.assertEqual(grade(effective, 60.0, baseline_value=None, metadata={}), "pass_a")
            self.assertEqual(grade(effective, 0.0, baseline_value=None, metadata={}), "fail")

    def test_multi_screen_loads_keep_an_explicit_target(self):
        """binding_push/concurrency still grade fps, with their own floor."""
        row = load_thresholds()["fps"]
        for scenario in ("binding_push", "concurrency"):
            effective = scenario_row(row, scenario)
            self.assertFalse(effective.get("record"))
            self.assertLess(effective["pass_b_min"], 40, "multi-screen loads run below the single-screen floor")

    def test_unknown_scenario_keeps_global_rule(self):
        row = load_thresholds()["fps"]
        self.assertEqual(scenario_row(row, "some_new_scenario")["pass_b_min"], 40)


if __name__ == "__main__":
    unittest.main()


class EndToEndScenarioGradingTest(unittest.TestCase):
    """A report that mixes sustained load with a legitimately idle screen must pass."""

    @staticmethod
    def _report(render_fps):
        def stats(value):
            return {"mean": value, "std": 0.0, "cv": 0.0, "p50": value, "p75": value,
                    "p95": value, "p99": value, "n": 30}

        return {
            "metadata": {"display": {"refresh_period_ms": 16.7}},
            "scenarios": {
                "render_stress": {"metrics": {"fps": stats(render_fps)}},
                "steady_state": {"metrics": {"fps": stats(0.0)}},
                "screen_cycle": {"metrics": {"fps": stats(1.0)}},
            },
        }

    def test_idle_scenarios_do_not_fail_a_sustained_load_report(self):
        from brookesia_gui_perf.compare import compare_reports

        result = compare_reports(self._report(60.0), self._report(60.0))
        self.assertTrue(result["pass"], "steady_state 0 fps and screen_cycle 1 fps must not fail the run")
        self.assertEqual(result["metrics"]["steady_state"]["fps"]["optimized_grade"], "record")

    def test_low_sustained_fps_still_fails(self):
        from brookesia_gui_perf.compare import compare_reports

        result = compare_reports(self._report(60.0), self._report(20.0))
        self.assertFalse(result["pass"], "20 fps on a sustained-load scenario is still a failure")


class PeakBudgetTest(unittest.TestCase):
    def test_psram_direction_matches_sram(self):
        thresholds = load_thresholds()
        self.assertEqual(thresholds["psram_peak_kb"]["direction"], "lower_better")

    def test_budget_makes_the_grade_worse(self):
        from brookesia_gui_perf.compare import grade

        row = load_thresholds()["sram_peak_kb"]
        meta = {"mem_budget_kb": {"sram": 512}}
        order = ["pass_a", "pass_b", "fail", "record"]
        within = grade(row, 400.0, baseline_value=None, metadata=meta)
        over = grade(row, 900.0, baseline_value=None, metadata=meta)
        self.assertLess(order.index(within), order.index(over),
                        "exceeding the memory budget must never grade better than staying inside it")


if __name__ == "__main__":
    unittest.main()


class SelfCompareTest(unittest.TestCase):
    """A real report compared with itself must pass: it is the gate's own sanity check."""

    @staticmethod
    def _report():
        def stats(value, cv=0.02):
            return {"mean": value, "std": value * cv, "cv": cv, "p50": value, "p75": value,
                    "p95": value, "p99": value, "n": 32}

        return {
            "metadata": {"display": {"refresh_period_ms": 16.7}, "mem_budget_kb": {"sram": 512}},
            "scenarios": {
                "render_stress": {"metrics": {"fps": stats(67.8), "frame_time_ms_p95": stats(15.0)}},
                "steady_state": {"metrics": {"fps": stats(0.0)}},
                "screen_cycle": {"metrics": {"fps": stats(1.0)}},
                "concurrency": {"metrics": {"flush_apply_ms": stats(0.77, cv=0.065)}},
                "feedback_delay": {"metrics": {"feedback_delay_ms": stats(1.4, cv=0.354)}},
            },
        }

    def test_jitter_metrics_do_not_fail_a_self_compare(self):
        from brookesia_gui_perf.compare import compare_reports

        report = self._report()
        result = compare_reports(report, report)
        self.assertTrue(result["pass"], "self-compare of a real report must pass")

    def test_jitter_metrics_are_marked_cv_exempt(self):
        thresholds = load_thresholds()
        for name in ("flush_apply_ms", "feedback_delay_ms"):
            self.assertTrue(thresholds[name].get("cv_exempt"),
                            "%s is a jitter metric; the mean-based cv gate cannot judge it" % name)


class ExemptionDeclarationTest(unittest.TestCase):
    """Every cv_exempt row must also be a declared, reasoned exemption."""

    def test_every_cv_exempt_row_is_declared(self):
        from brookesia_gui_perf.spec_coverage import STAT_EXEMPTIONS

        declared = {name for name, info in STAT_EXEMPTIONS.items() if info.get("kind") == "cv_exempt"}
        exempt_rows = {name for name, row in load_thresholds().items() if row.get("cv_exempt")}
        self.assertEqual(
            exempt_rows, declared,
            "thresholds.cv_exempt and spec_coverage.STAT_EXEMPTIONS must name the same rows: "
            "an undeclared exemption silently changes the acceptance verdict",
        )

    def test_declared_exemptions_carry_a_reason(self):
        from brookesia_gui_perf.spec_coverage import STAT_EXEMPTIONS

        for name, info in STAT_EXEMPTIONS.items():
            self.assertTrue(info.get("reason"), "%s needs a stated reason" % name)


class DiagnosticProbeReportTest(unittest.TestCase):
    """One-shot probes must not erase the report's sample budget."""

    def test_single_sample_probes_keep_the_top_level_count(self):
        from brookesia_gui_perf.report import build_report
        from tests.test_report import make_records

        records = make_records(40)
        for scenario in ("refresh_probe_static", "refresh_probe_local", "refresh_probe_full"):
            records.samples.append(
                {
                    "record": "sample",
                    "scenario": scenario,
                    "sample_index": 0,
                    "metrics": {"refreshes": 59.0, "flushes": 59.0, "ticks": 64.0},
                }
            )
        report = build_report(records)
        self.assertEqual(report["sample_count"], 37,
                         "a one-shot probe must not pull the shared sample count to 0")
        self.assertGreater(len(report["scenarios"]["app_a_scroll"]["metrics"]), 0)
        probe = report["scenarios"]["refresh_probe_local"]
        self.assertTrue(probe.get("diagnostic"))
        self.assertEqual(probe["metrics"]["refreshes"]["value"], 59.0)
