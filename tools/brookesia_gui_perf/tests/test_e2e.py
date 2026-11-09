# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import pathlib
import shutil
import tempfile
import unittest

from brookesia_gui_perf.cli import main
from brookesia_gui_perf.report import validate_report
from tests.test_report import make_records


def emit_log(records, git_sha: str) -> str:
    lines = [
        "I (100) boot: noise before marker",
        "[BENCH] "
        + json.dumps(
            {
                "record": "metadata",
                "test_suite": records.metadata["test_suite"],
                "app_id": records.metadata["app_id"],
                "runtime_type": records.metadata["runtime_type"],
                "metadata": {**records.metadata["metadata"], "git_sha": git_sha},
            }
        ),
    ]
    for sample in records.samples:
        lines.append("[BENCH] " + json.dumps(sample))
    lines.append('[BENCH] {"record": "completed"}')
    return "\n".join(lines) + "\n"


def scaled_records(scale: float):
    records = make_records(40)
    for sample in records.samples[3:]:
        sample["metrics"]["first_frame_ms"] = 100.0 * scale + (sample["sample_index"] % 7)
    return records


class EndToEndTests(unittest.TestCase):
    def test_full_pipeline_two_boards(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            bdir, odir, logs = root / "baseline", root / "optimized", root / "logs"
            for d in (bdir, odir, logs):
                d.mkdir()
            for board in ("esp32_p4x_function_ev", "esp32_s31_korvo1"):
                base_log = logs / f"base_{board}.log"
                opt_log = logs / f"opt_{board}.log"
                base_log.write_text(emit_log(scaled_records(1.0), "aaa1111"), encoding="utf-8")
                opt_log.write_text(emit_log(scaled_records(0.85), "bbb2222"), encoding="utf-8")
                # CI images have no git, so provenance cannot be fingerprinted there;
                # the pipeline must opt in explicitly, exactly as a real user would.
                extra = [] if shutil.which("git") else ["--allow-unknown-worktree"]
                self.assertEqual(
                    main(["aggregate", str(base_log), "-o",
                          str(bdir / f"Baseline.{board}.json"), *extra]),
                    0,
                )
                self.assertEqual(
                    main(["aggregate", str(opt_log), "-o",
                          str(odir / f"Optimized.{board}.json"), *extra]),
                    0,
                )
            report = json.loads(
                (bdir / "Baseline.esp32_p4x_function_ev.json").read_text(encoding="utf-8")
            )
            self.assertEqual(validate_report(report), [])
            self.assertEqual(report["metadata"]["git_sha"], "aaa1111")
            self.assertEqual(report["sample_count"], 37)
            rc = main(
                [
                    "verify",
                    "--baseline-dir", str(bdir),
                    "--optimized-dir", str(odir),
                ]
            )
            self.assertEqual(rc, 0)

    def test_gate_rejects_cross_board_compare(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            base_log = root / "base.log"
            opt_log = root / "opt.log"
            base = scaled_records(1.0)
            opt = scaled_records(0.85)
            opt.metadata["metadata"]["board_id"] = "esp32_s31_korvo1"
            base_log.write_text(emit_log(base, "aaa1111"), encoding="utf-8")
            opt_log.write_text(emit_log(opt, "bbb2222"), encoding="utf-8")
            base_json = root / "Baseline.esp32_p4x_function_ev.json"
            opt_json = root / "Optimized.esp32_p4x_function_ev.json"
            self.assertEqual(main(["aggregate", str(base_log), "-o", str(base_json)]), 0)
            self.assertEqual(main(["aggregate", str(opt_log), "-o", str(opt_json)]), 0)
            self.assertEqual(main(["gate", str(base_json), str(opt_json)]), 1)

    def test_e2e_real_p4_fixture_does_not_crash(self):
        """The genuine P4 serial stream must flow through the whole CLI.

        The real log carries no [BENCH] records because the device-side emitter
        is out of scope for this phase, so aggregate is expected to fail
        cleanly rather than emit a malformed report or raise.
        """
        path = pathlib.Path(__file__).parent / "fixtures" / "real_p4_coldboot.log"
        self.assertTrue(path.is_file(), f"missing real fixture: {path}")
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / "out.json"
            rc = main(["aggregate", str(path), "-o", str(out)])
            self.assertIn(rc, (0, 1), f"unexpected rc={rc}")
            if rc == 0 and out.is_file():
                self.assertEqual(validate_report(json.loads(out.read_text(encoding="utf-8"))), [])


if __name__ == "__main__":
    unittest.main()
