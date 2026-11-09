# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import pathlib
import unittest

from brookesia_gui_perf.parser import PREFIX, assess_log_integrity, parse_log

FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "sample_device.log"


class ParserTests(unittest.TestCase):
    def test_prefix_constant(self):
        self.assertEqual(PREFIX, "[BENCH] ")

    def test_parse_fixture(self):
        records = parse_log(FIXTURE.read_text(encoding="utf-8"))
        self.assertIsNotNone(records.metadata)
        self.assertEqual(records.metadata["metadata"]["board_id"], "esp32_p4x_function_ev")
        self.assertEqual(len(records.samples), 2)
        self.assertEqual(records.samples[0]["metrics"]["first_frame_ms"], 91.0)

    def test_missing_metadata_raises(self):
        with self.assertRaisesRegex(ValueError, "no metadata"):
            parse_log('[BENCH] {"record":"sample","scenario":"s","metrics":{}}\n')

    def test_malformed_json_is_counted_not_raised(self):
        # A record torn mid-JSON by UART congestion is counted rather than raised.
        # Raising here used to discard every intact record in the capture over one bad
        # line, while dropping it silently would shorten the run unnoticed. The count
        # reaches the report as malformed_records so neither failure mode is possible.
        bad = '[BENCH] {"record": broken\n'
        self.assertEqual(parse_log(bad).malformed, 1)

    def test_unknown_record_raises(self):
        with self.assertRaisesRegex(ValueError, "unknown record"):
            parse_log('[BENCH] {"record":"meta2"}\n')

    def test_duplicate_metadata_raises(self):
        line = '[BENCH] {"record":"metadata"}\n'
        with self.assertRaisesRegex(ValueError, "duplicate metadata"):
            parse_log(line + line)

    def test_concatenated_records_on_one_line(self):
        line = (
            '[BENCH] {"record":"metadata"}\n'
            '[BENCH] {"record":"sample","scenario":"a","metrics":{"m":1}}'
            '[BENCH] {"record":"sample","scenario":"b","metrics":{"m":2}}\n'
        )
        records = parse_log(line)
        self.assertEqual([s["scenario"] for s in records.samples], ["a", "b"])


class RealLogParserTests(unittest.TestCase):
    """Real P4 serial stream must not break the parser.

    The real log carries no [BENCH] records yet (device emitter is a later
    phase), so we assert only that parsing does not raise and that no
    records are produced.
    """

    def test_real_p4_coldboot_log_parses_without_bench_records(self):
        path = pathlib.Path(__file__).parent / "fixtures" / "real_p4_coldboot.log"
        self.assertTrue(path.is_file(), f"missing real fixture: {path}")
        text = path.read_text(encoding="utf-8", errors="replace")
        records = parse_log(text)
        self.assertIsNone(records.metadata)
        self.assertEqual(records.samples, [])

    def test_real_log_noise_does_not_raise(self):
        path = pathlib.Path(__file__).parent / "fixtures" / "real_p4_coldboot.log"
        text = path.read_text(encoding="utf-8", errors="replace")
        # Boot banners, ESP_LOG lines, a WDT panic dump and a truncated tail
        # must all be tolerated.
        for chunk in (text[:5000], text[-5000:], text):
            parse_log(chunk)  # must not raise

    def test_integrity_counts_clean_log(self):
        text = "I (10) Tag: : a\nI (20) Tag: : b\nplain line\n"
        self.assertEqual(
            assess_log_integrity(text),
            {"lines": 3, "concatenated_lines": 0, "timestamp_regressions": 0},
        )

    def test_integrity_detects_concatenated_line(self):
        text = "I (10) Tag: : a\nI (20) Tag: : bI (30) Tag: : c\n"
        report = assess_log_integrity(text)
        self.assertEqual(report["concatenated_lines"], 1)
        self.assertEqual(report["timestamp_regressions"], 0)

    def test_integrity_detects_timestamp_regression(self):
        text = "I (30) Tag: : a\nI (10) Tag: : b\n"
        self.assertEqual(assess_log_integrity(text)["timestamp_regressions"], 1)

    def test_integrity_on_real_capture(self):
        path = pathlib.Path(__file__).parent / "fixtures" / "real_p4_coldboot.log"
        report = assess_log_integrity(path.read_text(encoding="utf-8", errors="replace"))
        self.assertGreater(report["lines"], 100)
        self.assertEqual(report["timestamp_regressions"], 0)
        self.assertLess(
            report["concatenated_lines"] / report["lines"],
            0.01,
            "real captures show a handful of concatenated lines out of ~1.2k",
        )


if __name__ == "__main__":
    unittest.main()
