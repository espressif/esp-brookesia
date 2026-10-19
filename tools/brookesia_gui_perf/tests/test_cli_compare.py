# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Regression: the compare CLI must handle the per-scenario metric nesting."""

import os
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
BASELINE = REPO / "reports/baseline/esp32_p4_function_ev/Baseline.esp32_p4_function_ev.json"


def test_compare_cli_handles_nested_metrics():
    assert BASELINE.is_file(), f"missing baseline report: {BASELINE}"
    env = dict(os.environ)
    env["PYTHONPATH"] = str(REPO / "tools/brookesia_gui_perf/src")
    proc = subprocess.run(
        [sys.executable, "-m", "brookesia_gui_perf.cli", "compare",
         str(BASELINE), str(BASELINE)],
        cwd=str(REPO), capture_output=True, text=True, env=env, timeout=120,
    )
    assert "KeyError" not in (proc.stderr + proc.stdout), proc.stderr
    assert proc.returncode in (0, 1), proc.stderr
    assert "PASS=" in proc.stdout
    assert any("/" in line for line in proc.stdout.splitlines()), proc.stdout
