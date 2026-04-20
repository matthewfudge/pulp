#!/usr/bin/env python3
"""Tests for tools/deps/audit.py.

These tests exercise the markdown/JSON parsers directly and run the
strict audit over the real repo inventory. Run with:

    python3 -m pytest tools/deps/test_audit.py -v

or as a bare script:

    python3 tools/deps/test_audit.py
"""

from __future__ import annotations

import json
import subprocess
import sys
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools" / "deps" / "audit.py"

sys.path.insert(0, str(ROOT / "tools" / "deps"))
import audit  # noqa: E402  (path-injected import)


class ParserTests(unittest.TestCase):
    """The three markdown parsers extract the right names from table rows
    and ## headers. These are regression tests for the attribution audit
    added on 2026-04-21 after the docs/reference/licensing.md drift (7
    bundled deps were silently missing from the public licensing table)."""

    def test_licensing_md_extracts_bolded_first_column(self) -> None:
        sample = textwrap.dedent(
            """\
            | Name | License | Purpose |
            |------|---------|---------|
            | **Highway** | Apache-2.0 | SIMD |
            | **pugixml** | MIT | XML |
            | non-bold | ??? | should be skipped |
            """
        )
        tmp = ROOT / "tools" / "deps" / "_test_licensing.md"
        tmp.write_text(sample)
        try:
            original = audit.LICENSING_MD
            audit.LICENSING_MD = tmp
            names = audit.parse_licensing_md()
        finally:
            audit.LICENSING_MD = original
            tmp.unlink()
        self.assertIn("Highway", names)
        self.assertIn("pugixml", names)
        self.assertNotIn("non-bold", names)

    def test_notice_md_extracts_h2_headings(self) -> None:
        sample = "## foo\n\nbody\n\n## bar baz\n\nbody\n"
        tmp = ROOT / "tools" / "deps" / "_test_notice.md"
        tmp.write_text(sample)
        try:
            original = audit.NOTICE_MD
            audit.NOTICE_MD = tmp
            names = audit.parse_notice_md()
        finally:
            audit.NOTICE_MD = original
            tmp.unlink()
        self.assertEqual(names, {"foo", "bar baz"})

    def test_manifest_json_is_valid(self) -> None:
        manifest = json.loads((ROOT / "tools" / "deps" / "manifest.json").read_text())
        names = [d["name"] for d in manifest["dependencies"]]
        self.assertGreater(len(names), 0)
        # Every manifest entry must declare doc coverage flags + source_files.
        for dep in manifest["dependencies"]:
            self.assertIn("documented_in_dependencies_md", dep, dep["name"])
            self.assertIn("documented_in_notice_md", dep, dep["name"])
            self.assertIn("source_files", dep, dep["name"])

    def test_manifest_is_alphabetical(self) -> None:
        manifest = json.loads((ROOT / "tools" / "deps" / "manifest.json").read_text())
        names = [d["name"] for d in manifest["dependencies"]]
        self.assertEqual(
            names,
            sorted(names, key=str.casefold),
            "manifest.json entries must be alphabetical (case-insensitive)",
        )


class StrictAuditTests(unittest.TestCase):
    """Running the real audit script with --strict against origin/main
    inventory should succeed. If this fails, something is missing from
    DEPENDENCIES.md, NOTICE.md, or docs/reference/licensing.md."""

    def test_audit_strict_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, str(AUDIT), "--strict"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(
            result.returncode,
            0,
            msg=f"audit.py --strict failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )


if __name__ == "__main__":
    unittest.main()
