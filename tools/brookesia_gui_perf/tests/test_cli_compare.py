# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Regression: the compare CLI must handle the per-scenario metric nesting."""

import os
import pathlib
import subprocess
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[3]
BASELINE = REPO / "reports" / "baseline" / "esp32_p4x_function_ev" / "Baseline.esp32_p4x_function_ev.json"


class CompareCliTest(unittest.TestCase):
    def test_compare_handles_nested_metrics(self):
        self.assertTrue(BASELINE.is_file(), f"missing baseline report: {BASELINE}")
        env = dict(os.environ)
        env["PYTHONPATH"] = str(REPO / "tools" / "brookesia_gui_perf" / "src")
        proc = subprocess.run(
            [sys.executable, "-m", "brookesia_gui_perf.cli", "compare",
             str(BASELINE), str(BASELINE)],
            cwd=str(REPO), capture_output=True, text=True, env=env, timeout=120,
        )
        combined = proc.stderr + proc.stdout
        self.assertNotIn("KeyError", combined, combined)
        self.assertIn(proc.returncode, (0, 1), proc.stderr)
        self.assertIn("PASS=", proc.stdout)
        self.assertTrue(any("/" in line for line in proc.stdout.splitlines()), proc.stdout)


if __name__ == "__main__":
    unittest.main()
