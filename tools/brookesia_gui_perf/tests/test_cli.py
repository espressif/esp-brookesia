# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import pathlib
import tempfile
import unittest

from brookesia_gui_perf.cli import main
from brookesia_gui_perf.report import build_report
from tests.test_report import make_records


def write_report(path: pathlib.Path, scale: float = 1.0):
    records = make_records(33)
    for sample in records.samples[3:]:
        sample["metrics"]["first_frame_ms"] = 100.0 * scale + (sample["sample_index"] % 7)
    path.write_text(json.dumps(build_report(records)), encoding="utf-8")


class CliTests(unittest.TestCase):
    def test_aggregate_writes_valid_report(self):
        records = make_records(33)
        header = json.dumps(
            {
                "record": "metadata",
                "test_suite": records.metadata["test_suite"],
                "app_id": records.metadata["app_id"],
                "runtime_type": records.metadata["runtime_type"],
                "metadata": records.metadata["metadata"],
            }
        )
        lines = [f"[BENCH] {header}"]
        for sample in records.samples:
            lines.append(f"[BENCH] {json.dumps(sample)}")
        lines.append('[BENCH] {"record": "completed"}')
        with tempfile.TemporaryDirectory() as tmp:
            log_path = pathlib.Path(tmp) / "run.log"
            out_path = pathlib.Path(tmp) / "Baseline.esp32_p4x_function_ev.json"
            log_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            rc = main(["aggregate", str(log_path), "-o", str(out_path)])
            self.assertEqual(rc, 0)
            report = json.loads(out_path.read_text(encoding="utf-8"))
            self.assertEqual(report["sample_count"], 30)

    def test_gate_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = pathlib.Path(tmp) / "Baseline.esp32_p4x_function_ev.json"
            opt_ok = pathlib.Path(tmp) / "Optimized.esp32_p4x_function_ev.json"
            opt_bad = pathlib.Path(tmp) / "Optimized.esp32_p4x_function_ev.bad.json"
            write_report(base, 1.0)
            write_report(opt_ok, 0.9)
            write_report(opt_bad, 0.9)
            data = json.loads(opt_bad.read_text(encoding="utf-8"))
            data["metadata"]["idf_version"] = "9.9.9"
            opt_bad.write_text(json.dumps(data), encoding="utf-8")
            self.assertEqual(main(["gate", str(base), str(opt_ok)]), 0)
            self.assertEqual(main(["gate", str(base), str(opt_bad)]), 1)

    def test_compare_exit_and_verify_multi_board(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            bdir, odir = root / "baseline", root / "optimized"
            bdir.mkdir(), odir.mkdir()
            for board in ("esp32_p4x_function_ev", "esp32_s31_korvo1"):
                write_report(bdir / f"Baseline.{board}.json", 1.0)
                write_report(odir / f"Optimized.{board}.json", 0.85)
            rc = main(
                [
                    "verify",
                    "--baseline-dir", str(bdir),
                    "--optimized-dir", str(odir),
                ]
            )
            self.assertEqual(rc, 0)
            write_report(odir / "Optimized.esp32_s31_korvo1.json", 1.05)
            rc = main(
                [
                    "verify",
                    "--baseline-dir", str(bdir),
                    "--optimized-dir", str(odir),
                ]
            )
            self.assertEqual(rc, 1)
            empty = root / "empty"
            empty.mkdir()
            rc = main(
                [
                    "verify",
                    "--baseline-dir", str(empty),
                    "--optimized-dir", str(odir),
                ]
            )
            self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main()
