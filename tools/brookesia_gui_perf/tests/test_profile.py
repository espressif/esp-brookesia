# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import textwrap
import unittest

from brookesia_gui_perf.profile import ProfileLog, parse_profile_log

# Lines below are byte-for-byte in the shape emitted by the probes in
# gui/brookesia_gui_interface/src/parser.cpp, including the ESP-IDF header and
# the %1%/%2% substitutions that macro performs.
DEVICE_LOG = textwrap.dedent(
    """\
    I (73321) GuiIntf: GUI parser profile: base_dir(/app), stage(parse_root_json), bytes(2048), elapsed_ms(4412), total_ms(4412)
    I (73351) GuiIntf: GUI parser profile: base_dir(/app), stage(load_asset_entries), assets(9), dependencies(9), elapsed_ms(0), total_ms(4412)
    I (73360) GuiIntf: GUI parser profile: base_dir(/app), stage(resolve_constants), assets(9), elapsed_ms(2), total_ms(4414)
    I (84880) GuiIntf: GUI parser profile: base_dir(/app), stage(resolve_assets), resolved_assets(9), elapsed_ms(11520), total_ms(15934)
    I (84890) GuiIntf: GUI parser profile: base_dir(/app), stage(parse_image_assets), images(39), elapsed_ms(0), total_ms(15934)
    """
)


class TestParseProfileLog(unittest.TestCase):
    def test_recovers_every_stage(self):
        result = parse_profile_log(DEVICE_LOG)
        self.assertEqual(
            result.stages(),
            [
                "parse_root_json",
                "load_asset_entries",
                "resolve_constants",
                "resolve_assets",
                "parse_image_assets",
            ],
        )
        self.assertEqual(result.undecodable, 0)

    def test_elapsed_and_running_total(self):
        result = parse_profile_log(DEVICE_LOG)
        by_stage = result.by_stage()
        self.assertAlmostEqual(by_stage["parse_root_json"][0].elapsed_ms, 4412.0)
        self.assertAlmostEqual(by_stage["resolve_assets"][0].elapsed_ms, 11520.0)

    def test_total_is_max_not_sum(self):
        # total_ms is a running total from the probe. The stages happen to sum
        # to it exactly in this capture, so the discriminator is that a
        # fabricated overlap must not be added: a duplicated stage line would
        # inflate the sum past the running total, and max() must ignore it.
        result = parse_profile_log(DEVICE_LOG)
        self.assertAlmostEqual(result.total_parse_ms(), 15934.0)

        doubled = DEVICE_LOG + DEVICE_LOG
        result2 = parse_profile_log(doubled)
        self.assertEqual(len(result2.segments), 10)
        summed = sum(s.elapsed_ms for s in result2.segments)
        self.assertAlmostEqual(summed, 2 * 15934.0)
        self.assertAlmostEqual(result2.total_parse_ms(), 15934.0)

    def test_counters_are_preserved(self):
        result = parse_profile_log(DEVICE_LOG)
        by_stage = result.by_stage()
        self.assertEqual(by_stage["parse_image_assets"][0].counters["images"], 39.0)
        self.assertEqual(by_stage["parse_root_json"][0].counters["bytes"], 2048.0)

    def test_structural_counters_are_not_counters(self):
        result = parse_profile_log(DEVICE_LOG)
        seg = result.by_stage()["parse_root_json"][0]
        for key in ("stage", "base_dir", "elapsed_ms", "total_ms"):
            self.assertNotIn(key, seg.counters)

    def test_ignores_unrelated_log_lines(self):
        noise = textwrap.dedent(
            """\
            I (100) PerfEval: perf_eval: board=esp32_p4_function_ev
            [BENCH] {"record":"sample","scenario":"steady_state","sample_index":0,"metrics":{}}
            I (200) Gui: some other message
            """
        )
        result = parse_profile_log(noise)
        self.assertEqual(result.segments, [])
        self.assertEqual(result.undecodable, 0)

    def test_recovers_both_records_from_concatenated_capture(self):
        # A lost newline glues two records onto one physical line. Both stages
        # are still recoverable, because the field scan is not line-anchored;
        # assess_log_integrity is what flags the missing newline, so this
        # parser reports what it can read rather than counting it twice.
        damaged = (
            "I (1) GuiIntf: GUI parser profile: base_dir(/app), stage(a), elapsed_ms(1), total_ms(1)"
            "I (2) GuiIntf: GUI parser profile: base_dir(/app), stage(b), elapsed_ms(2), total_ms(3)"
        )
        result = parse_profile_log(damaged)
        self.assertEqual([s.stage for s in result.segments], ["a", "b"])
        self.assertEqual(result.undecodable, 0)
        self.assertAlmostEqual(result.total_parse_ms(), 3.0)

    def test_marks_undecodable_rather_than_raising(self):
        broken = (
            "I (1) GuiIntf: GUI parser profile: base_dir(/app), stage(x), elapsed_ms(NOT_A_NUMBER)\n"
            "I (2) GuiIntf: GUI parser profile: base_dir(/app), elapsed_ms(5)\n"
        )
        result = parse_profile_log(broken)
        self.assertEqual(result.segments, [])
        self.assertEqual(result.undecodable, 2)

    def test_as_dict_shape(self):
        out = parse_profile_log(DEVICE_LOG).as_dict()
        self.assertIn("stages", out)
        self.assertAlmostEqual(out["doc_load_total_ms"], 15934.0)
        self.assertNotIn("undecodable", out)

    def test_empty_log(self):
        result = parse_profile_log("")
        self.assertIsInstance(result, ProfileLog)
        self.assertEqual(result.segments, [])
        self.assertIsNone(result.total_parse_ms())


if __name__ == "__main__":
    unittest.main()
