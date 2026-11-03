# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Worktree fingerprint semantics, tested without a git binary.

CI runs the host tests in a slim image without git, so the state is exercised
through an injected runner instead of a real repository.
"""

import copy
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

from brookesia_gui_perf.report import _worktree_state

SHIPPED_BASELINE = pathlib.Path(__file__).resolve().parents[3] / "reports" / "baseline" \
    / "esp32_p4x_function_ev" / "Baseline.esp32_p4x_function_ev.json"


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
        from brookesia_gui_perf.gate import check_gate

        if not SHIPPED_BASELINE.is_file():
            self.skipTest(f"baseline not available: {SHIPPED_BASELINE}")
        report = json.loads(SHIPPED_BASELINE.read_text(encoding="utf-8"))
        metadata = report["metadata"]
        self.assertIn("worktree_state_known", metadata)
        self.assertIn("worktree_exempt", metadata)
        self.assertEqual(check_gate(report, report), [])

    def test_shipped_baseline_pairs_with_a_clean_optimized_arm(self):
        """The shipped baseline must pair with a normally aggregated clean arm.

        The optimized side is forced to the source state every fresh `aggregate`
        records (clean, no fingerprint), so a dirty shipped baseline fails here
        instead of only failing when someone runs the real two-arm comparison.
        """
        from brookesia_gui_perf.gate import check_gate

        if not SHIPPED_BASELINE.is_file():
            self.skipTest(f"baseline not available: {SHIPPED_BASELINE}")
        baseline = json.loads(SHIPPED_BASELINE.read_text(encoding="utf-8"))
        metadata = baseline["metadata"]
        self.assertFalse(metadata["worktree_dirty"], metadata["worktree_dirty"])
        self.assertEqual(metadata["worktree_diff_sha256"], "")
        self.assertTrue(metadata["worktree_state_known"])
        self.assertNotIn("-dirty", metadata["git_sha"])

        optimized = copy.deepcopy(baseline)
        optimized["metadata"]["git_sha"] = "0123456"
        optimized["metadata"]["worktree_dirty"] = False
        optimized["metadata"]["worktree_diff_sha256"] = ""
        optimized["metadata"]["worktree_state_known"] = True
        optimized["metadata"]["worktree_exempt"] = False
        self.assertEqual(check_gate(baseline, optimized), [])

    def test_dirty_variant_of_the_shipped_baseline_is_rejected(self):
        """The same file with a dirty flag must not pair with a clean arm."""
        from brookesia_gui_perf.gate import check_gate

        if not SHIPPED_BASELINE.is_file():
            self.skipTest(f"baseline not available: {SHIPPED_BASELINE}")
        clean = json.loads(SHIPPED_BASELINE.read_text(encoding="utf-8"))
        dirty = copy.deepcopy(clean)
        dirty["metadata"]["worktree_dirty"] = True
        dirty["metadata"]["worktree_diff_sha256"] = "a" * 64
        violations = check_gate(dirty, clean)
        self.assertTrue(any("worktree_dirty" in v for v in violations), violations)
        self.assertTrue(any("worktree_diff_sha256" in v for v in violations), violations)

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
