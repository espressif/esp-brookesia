# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

from brookesia_gui_asset_bundle.cli import main


class CliTests(unittest.TestCase):
    def setUp(self):
        self._temp = tempfile.TemporaryDirectory()
        self.addCleanup(self._temp.cleanup)
        self.root = Path(self._temp.name) / "res"
        (self.root / "screens").mkdir(parents=True)
        (self.root / "root.json").write_text('{"type": "root"}', encoding="utf-8")
        (self.root / "screens" / "home.json").write_text("{}", encoding="utf-8")
        self.bundle = Path(self._temp.name) / "assets.bundle"

    def run_cli(self, argv):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = main(argv)
        return code, out.getvalue(), err.getvalue()

    def test_pack_reports_the_entry_count(self):
        code, out, _ = self.run_cli(["pack", "--res-root", str(self.root), "--out", str(self.bundle)])
        self.assertEqual(code, 0)
        self.assertIn("packed 2 entries", out)
        self.assertTrue(self.bundle.is_file())

    def test_verify_accepts_a_fresh_bundle(self):
        self.run_cli(["pack", "--res-root", str(self.root), "--out", str(self.bundle)])
        code, out, _ = self.run_cli(["verify", "--bundle", str(self.bundle), "--res-root", str(self.root)])
        self.assertEqual(code, 0)
        self.assertIn("bundle matches", out)

    def test_verify_reports_a_stale_bundle(self):
        self.run_cli(["pack", "--res-root", str(self.root), "--out", str(self.bundle)])
        (self.root / "root.json").write_text('{"type": "changed"}', encoding="utf-8")
        code, _, err = self.run_cli(["verify", "--bundle", str(self.bundle), "--res-root", str(self.root)])
        self.assertEqual(code, 1)
        self.assertIn("differs", err)

    def test_list_prints_entries(self):
        self.run_cli(["pack", "--res-root", str(self.root), "--out", str(self.bundle)])
        code, out, _ = self.run_cli(["list", "--bundle", str(self.bundle)])
        self.assertEqual(code, 0)
        self.assertIn("root.json", out)
        self.assertIn("screens/home.json", out)
        self.assertIn("2 entries", out)

    def test_missing_root_exits_with_two(self):
        code, _, err = self.run_cli(["pack", "--res-root", str(self.root / "missing"), "--out", str(self.bundle)])
        self.assertEqual(code, 2)
        self.assertIn("error:", err)


if __name__ == "__main__":
    unittest.main()
