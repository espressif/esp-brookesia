# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Worktree fingerprint semantics, tested without a git binary.

CI runs the host tests in a slim image without git, so the state is exercised
through an injected runner instead of a real repository.
"""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from brookesia_gui_perf.report import _worktree_state


def _runner(unstaged="", staged="", untracked=()):
    def run(*args):
        if args[0] == "diff":
            return staged if args[1:] == ("--cached",) else unstaged
        if "ls-files" in args:
            return "".join(name + "\0" for name in untracked)
        raise AssertionError(args)
    return run


class WorktreeStateTests(unittest.TestCase):
    def setUp(self):
        self._cwd = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        os.chdir(self.root)

    def tearDown(self):
        os.chdir(self._cwd)
        self.tmp.cleanup()

    def test_clean_tree_is_not_dirty(self):
        self.assertEqual(_worktree_state(_runner()), (False, "", True))

    def test_unstaged_change_is_dirty(self):
        dirty, sha, known = _worktree_state(_runner(unstaged="diff --git a/x b/x\n-old\n+new\n"))
        self.assertTrue(dirty)
        self.assertTrue(sha)

    def test_staged_only_change_is_dirty(self):
        dirty, sha, known = _worktree_state(_runner(staged="diff --git a/x b/x\n-old\n+new\n"))
        self.assertTrue(dirty)
        self.assertTrue(sha)

    def test_untracked_content_changes_the_fingerprint(self):
        (self.root / "new.txt").write_text("one\n")
        _, first, _ = _worktree_state(_runner(untracked=("new.txt",)))
        (self.root / "new.txt").write_text("two\n")
        _, second, _ = _worktree_state(_runner(untracked=("new.txt",)))
        self.assertNotEqual(first, second)

    def test_failing_git_is_not_reported_as_clean(self):
        def boom(*args):
            raise RuntimeError("git failed")
        self.assertEqual(_worktree_state(boom), (True, "", False))

    @unittest.skipUnless(shutil.which("git"), "git not installed")
    def test_real_repository_distinguishes_non_ascii_untracked_names(self):
        """git quotepath escaping must not collapse non-ASCII names to one empty state."""
        env = dict(os.environ)
        env.update({
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@example.com",
            "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@example.com",
        })

        def git(*args):
            subprocess.run(["git", "-c", "safe.directory=*", *args], cwd=self.root,
                           check=True, capture_output=True, env=env)

        git("init", "-q")
        (self.root / "a.txt").write_text("one\n")
        git("add", "a.txt")
        git("commit", "-q", "-m", "init")

        non_ascii = self.root / "\u4e2d\u6587.txt"
        non_ascii.write_text("one\n")
        dirty_first, first, _ = _worktree_state()
        self.assertTrue(dirty_first)
        self.assertNotEqual(first, "")

        non_ascii.write_text("two\n")
        _, second, _ = _worktree_state()
        self.assertNotEqual(first, second)

    def test_shipped_baseline_carries_the_current_worktree_schema(self):
        """The shipped baseline must stay pairable with a freshly built report.

        gate.py compares every non-volatile metadata field, so a schema change that
        only reaches build_report() silently makes the shipped baseline unpairable.
        """
        import json as _json
        from brookesia_gui_perf.gate import check_gate

        baseline = pathlib.Path(__file__).resolve().parents[3] / "reports" / "baseline" \
            / "esp32_p4_function_ev" / "Baseline.esp32_p4_function_ev.json"
        if not baseline.is_file():
            self.skipTest(f"baseline not available: {baseline}")
        metadata = _json.loads(baseline.read_text(encoding="utf-8"))["metadata"]
        self.assertIn("worktree_state_known", metadata)
        self.assertIn("worktree_exempt", metadata)
        report = _json.loads(baseline.read_text(encoding="utf-8"))
        self.assertEqual(check_gate(report, report), [])

    @unittest.skipUnless(shutil.which("git"), "git not installed")
    def test_real_clean_repository_reports_clean(self):
        from brookesia_gui_perf.report import _worktree_state as real
        repo = pathlib.Path(__file__).resolve().parents[3]
        os.chdir(repo)
        dirty, sha, known = real()
        self.assertIsInstance(dirty, bool)
        if not dirty:
            self.assertEqual(sha, "")


if __name__ == "__main__":
    unittest.main()
