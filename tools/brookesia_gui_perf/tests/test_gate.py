# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import copy
import unittest

from brookesia_gui_perf.gate import check_gate, check_validity
from brookesia_gui_perf.report import build_report
from tests.test_report import make_records


class GateTests(unittest.TestCase):
    def setUp(self):
        self.base = build_report(make_records(33))

    def test_identical_pair_passes_despite_git_sha(self):
        opt = copy.deepcopy(self.base)
        opt["metadata"]["git_sha"] = "def5678"
        self.assertEqual(check_gate(self.base, opt), [])

    def test_board_id_mismatch(self):
        opt = copy.deepcopy(self.base)
        opt["metadata"]["board_id"] = "esp32_s31_korvo1"
        violations = check_gate(self.base, opt)
        self.assertTrue(any("board_id" in v for v in violations))

    def test_environment_field_mismatch(self):
        opt = copy.deepcopy(self.base)
        opt["metadata"]["idf_version"] = "6.1.0"
        violations = check_gate(self.base, opt)
        self.assertTrue(any("idf_version" in v for v in violations))

    def test_invalid_benchmark_validity(self):
        opt = copy.deepcopy(self.base)
        opt["metadata"]["benchmark_validity"]["heap_trace_overflow"] = True
        violations = check_gate(self.base, opt)
        self.assertTrue(any("heap_trace_overflow" in v for v in violations))

    def test_validity_rules(self):
        meta = copy.deepcopy(self.base["metadata"])
        self.assertEqual(check_validity(meta), [])
        bv = meta["benchmark_validity"]
        bv["asserts_enabled"] = True
        bv["log_level"] = "DEBUG"
        violations = check_validity(meta)
        self.assertEqual(len(violations), 2)

    def test_opa_scene_accepts_half_buffer(self):
        meta = copy.deepcopy(self.base["metadata"])
        bv = meta["benchmark_validity"]
        bv["draw_buf_ratio"] = 0.06
        # Spec section 1.4: opa_layer scenes may use >= 1/20 screen area, all
        # others >= 1/10. The opa branch is keyed on an explicit opa_scene
        # flag, so the test must set it rather than rely on it being present.
        bv["opa_scene"] = True
        self.assertEqual(check_validity(meta), [])
        bv["opa_scene"] = False
        violations = check_validity(meta)
        self.assertTrue(any("draw_buf_ratio" in v for v in violations))


class WorktreeGateTests(unittest.TestCase):
    """A dirty-tree baseline must not be paired with a clean-tree run.

    The two fields are set explicitly rather than through build_report
    kwargs: build_report auto-detects the real working-tree state, so in
    this intentionally-dirty repo an empty diff digest argument is
    backfilled with the real digest and no "clean" report can be produced
    that way. These tests exercise the gate's field comparison, which is
    the behaviour under test.
    """

    def test_gate_rejects_dirty_vs_clean_worktree(self):
        base = build_report(make_records(40))
        opt = copy.deepcopy(base)
        base["metadata"]["worktree_dirty"] = True
        opt["metadata"]["worktree_dirty"] = False
        self.assertTrue(
            any("worktree_dirty" in v for v in check_gate(base, opt)),
            "gate must refuse pairing a dirty-tree baseline with a clean-tree run",
        )

    def test_gate_rejects_differing_worktree_diff(self):
        base = build_report(make_records(40))
        opt = copy.deepcopy(base)
        base["metadata"]["worktree_diff_sha256"] = "a" * 64
        opt["metadata"]["worktree_diff_sha256"] = "b" * 64
        self.assertTrue(
            any("worktree_diff_sha256" in v for v in check_gate(base, opt)),
            "gate must refuse two runs with different dirty-state digests",
        )

    def test_gate_accepts_identical_worktree_state(self):
        base = build_report(make_records(40))
        opt = copy.deepcopy(base)
        opt["metadata"]["git_sha"] = "deadbee"
        self.assertEqual(check_gate(base, opt), [])


if __name__ == "__main__":
    unittest.main()
