#!/usr/bin/env python3
"""Tests for the desktop review verdict command."""

from argparse import Namespace
import json
from pathlib import Path
import tempfile
import unittest

from module_test_utils import load_local_ci_module


def load_module():
    return load_local_ci_module("desktop_review_commands_cli.py")


class FakeProc:
    def __init__(self, returncode=0, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


class DesktopReviewVerdictTests(unittest.TestCase):
    def setUp(self):
        self.mod = load_module()
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps({"label": "demo"}))
        self.written = {}

    def tearDown(self):
        self.tmp.cleanup()

    def _args(self, **over):
        base = dict(
            manifest=str(self.manifest),
            approved=False,
            needs_work=False,
            notes="",
            reviewer="",
            issue_url="",
            comment_issue=False,
            close_issue=False,
            close_reason="completed",
            json=False,
        )
        base.update(over)
        return Namespace(**base)

    def _atomic_write(self, path, text):
        Path(path).write_text(text)

    def _run(self, args, run_fn=None):
        out = []
        rc = self.mod.cmd_desktop_verdict(
            args,
            now_iso_fn=lambda: "2026-06-16T00:00:00+00:00",
            atomic_write_text_fn=self._atomic_write,
            run_fn=run_fn or (lambda *a, **k: FakeProc()),
            print_fn=out.append,
        )
        return rc, "\n".join(out)

    def test_missing_manifest_errors(self):
        rc, out = self._run(self._args(manifest=str(self.root / "nope.json"), approved=True))
        self.assertEqual(rc, 1)
        self.assertIn("manifest not found", out)

    def test_approved_records_review_and_writes_verdict(self):
        rc, out = self._run(self._args(approved=True, notes="looks good"))
        self.assertEqual(rc, 0)
        manifest = json.loads(self.manifest.read_text())
        self.assertEqual(manifest["review"]["status"], "approved")
        self.assertTrue(manifest["review"]["close_review_issue"])
        self.assertTrue((self.root / "review-verdict.md").exists())
        self.assertTrue((self.root / "review-verdict.json").exists())

    def test_needs_work_sets_follow_up(self):
        rc, out = self._run(self._args(needs_work=True, notes="fix the knob"))
        self.assertEqual(rc, 0)
        manifest = json.loads(self.manifest.read_text())
        self.assertEqual(manifest["review"]["status"], "needs-work")
        self.assertFalse(manifest["review"]["close_review_issue"])
        self.assertTrue(manifest["review"]["follow_up_required"])

    def test_comment_issue_requires_url(self):
        rc, out = self._run(self._args(approved=True, comment_issue=True))
        self.assertEqual(rc, 1)
        self.assertIn("--comment-issue requires --issue-url", out)

    def test_comment_issue_invokes_gh(self):
        calls = []

        def run_fn(argv, **kw):
            calls.append(argv)
            return FakeProc(returncode=0)

        rc, out = self._run(
            self._args(approved=True, comment_issue=True, issue_url="https://gh/issue/1"),
            run_fn=run_fn,
        )
        self.assertEqual(rc, 0)
        self.assertEqual(calls[0][:3], ["gh", "issue", "comment"])

    def test_close_issue_requires_approved(self):
        rc, out = self._run(self._args(needs_work=True, close_issue=True, issue_url="https://gh/issue/1"))
        self.assertEqual(rc, 1)
        self.assertIn("--close-issue requires --approved", out)


if __name__ == "__main__":
    unittest.main()
