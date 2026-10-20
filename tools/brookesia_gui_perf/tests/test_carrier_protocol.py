# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Guard the carrier/pytest protocol against drifting apart.

The device log text and the expectations in pytest_perf_eval.py live in different
files and drifted once already (the mount line gained "over the whole Golden Case"
and the test kept waiting for the old wording). This reads both sides as text and
checks the patterns still match a canonical line.
"""

import pathlib
import re
import unittest

REPO = pathlib.Path(__file__).resolve().parents[3]
CARRIER_PYTEST = REPO / "examples" / "system" / "perf_eval" / "pytest_perf_eval.py"

MOUNT_LINE = ("I (54386) PerfEval: mounted 24 screens over the whole Golden Case "
              "(0 failed); runtime LVGL objects=1303 (section 3.1 gate >=600)")
COMPLETED_LINE = '[BENCH] {"record":"completed"}'


class CarrierProtocolTest(unittest.TestCase):
    def setUp(self):
        self.source = CARRIER_PYTEST.read_text(encoding="utf-8")

    def test_mount_pattern_matches_carrier_line(self):
        pattern = re.search(r"r'(PerfEval: mounted [^']+)'", self.source)
        self.assertIsNotNone(pattern, "mount expectation not found in pytest_perf_eval.py")
        match = re.search(pattern.group(1), MOUNT_LINE)
        self.assertIsNotNone(match, f"pattern {pattern.group(1)!r} no longer matches the carrier line")
        self.assertEqual(match.group(1), "24")
        self.assertEqual(match.group(2), "0")

    def test_completion_marker_matches_carrier_line(self):
        line = next((row for row in self.source.splitlines()
                     if row.startswith("COMPLETION_MARKER")), None)
        self.assertIsNotNone(line, "COMPLETION_MARKER not found in pytest_perf_eval.py")
        marker = line.split("=", 1)[1].strip().strip("'\"")
        self.assertIn(marker, COMPLETED_LINE)


if __name__ == "__main__":
    unittest.main()
