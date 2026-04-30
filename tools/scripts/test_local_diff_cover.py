#!/usr/bin/env python3
"""Fixture tests for tools/scripts/local_diff_cover.sh.

We deliberately don't run the full coverage build in these tests —
that's minutes of clang time and the real toolchain already runs in
.github/workflows/coverage.yml. Instead we exercise:

  1. The PULP_SKIP_DIFF_COVER=1 bypass — must exit 0 with the
     documented skip message before doing any work.
  2. The config-driven threshold — assert that coverage_config.json
     parses, the threshold is the documented integer, and the script
     reads the same value via its embedded python helper.
  3. The dependency preflight — when a required binary is missing
     from PATH, the script must exit 2 with a clear message (not
     wedge or run a partial build).

Run:
    python3 tools/scripts/test_local_diff_cover.py
"""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SCRIPT = REPO_ROOT / "tools" / "scripts" / "local_diff_cover.sh"
CONFIG = REPO_ROOT / "tools" / "scripts" / "coverage_config.json"
CI_SCRIPT = REPO_ROOT / "scripts" / "run_coverage.sh"


class ConfigTests(unittest.TestCase):
    """coverage_config.json is well-formed and exposes the expected keys."""

    def test_config_exists(self) -> None:
        self.assertTrue(CONFIG.exists(), f"missing: {CONFIG}")

    def test_config_parses(self) -> None:
        with CONFIG.open() as f:
            cfg = json.load(f)
        # Required keys consumed by .github/workflows/coverage.yml AND
        # tools/scripts/local_diff_cover.sh — keep them in lockstep.
        self.assertIn("diff_coverage_fail_under", cfg)
        self.assertIn("compare_branch", cfg)
        self.assertIsInstance(cfg["diff_coverage_fail_under"], int)
        self.assertGreaterEqual(cfg["diff_coverage_fail_under"], 0)
        self.assertLessEqual(cfg["diff_coverage_fail_under"], 100)
        self.assertIsInstance(cfg["compare_branch"], str)


class SkipFlagTests(unittest.TestCase):
    """PULP_SKIP_DIFF_COVER=1 short-circuits before any work."""

    def test_skip_flag_exits_zero(self) -> None:
        env = os.environ.copy()
        env["PULP_SKIP_DIFF_COVER"] = "1"
        result = subprocess.run(
            ["bash", str(SCRIPT)],
            env=env,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("skipped via PULP_SKIP_DIFF_COVER=1", result.stderr)

    def test_skip_flag_works_with_args(self) -> None:
        # Even with positional targets, the skip flag must short-circuit.
        env = os.environ.copy()
        env["PULP_SKIP_DIFF_COVER"] = "1"
        result = subprocess.run(
            ["bash", str(SCRIPT), "pulp-test-state", "pulp-test-widget-bridge"],
            env=env,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("skipped via PULP_SKIP_DIFF_COVER=1", result.stderr)


class DependencyPreflightTests(unittest.TestCase):
    """The dep-preflight branch is structured to exit 2 with a clear msg."""

    def test_preflight_exits_two_branch_present(self) -> None:
        # Static check: the script must declare an exit-2 path with a
        # "missing required deps" message. We can't reliably hide clang
        # from PATH on macOS (clang lives in /usr/bin alongside dirname
        # and other essentials this script needs), so we assert the
        # exit-2 contract is present in the source.
        text = SCRIPT.read_text()
        self.assertIn("missing required deps", text)
        self.assertIn("exit 2", text)
        # Must check for the canonical Clang toolchain.
        self.assertRegex(text, r"\bclang\b")
        self.assertRegex(text, r"\bllvm-cov\b")
        self.assertRegex(text, r"\bllvm-profdata\b")
        # Must check for diff-cover importability via python3.
        self.assertIn("import diff_cover", text)


class WorkflowSourceOfTruthTests(unittest.TestCase):
    """coverage.yml reads --fail-under from coverage_config.json (anti-drift)."""

    WORKFLOW = REPO_ROOT / ".github" / "workflows" / "coverage.yml"

    def test_workflow_reads_threshold_from_json(self) -> None:
        text = self.WORKFLOW.read_text()
        # The workflow must extract the threshold via jq from the
        # config file BEFORE invoking diff-cover. If a future edit
        # hardcodes --fail-under=NN again, this test fails loudly.
        self.assertIn(
            "jq -r '.diff_coverage_fail_under' tools/scripts/coverage_config.json",
            text,
            "coverage.yml must read the threshold from coverage_config.json — "
            "do not hardcode --fail-under in the workflow",
        )
        # And the diff-cover step must use the resolved variable, not
        # a literal.
        self.assertIn('--fail-under="${THRESHOLD}"', text)
        # Defense in depth: assert no literal --fail-under=<number>
        # remains in the workflow. A hardcoded literal would silently
        # win over the JSON-driven value.
        import re
        literals = re.findall(r"--fail-under=\d+", text)
        self.assertEqual(
            literals, [],
            f"workflow has hardcoded --fail-under literal(s): {literals}; "
            "remove and read from coverage_config.json instead",
        )


class ObjectDiscoveryParityTests(unittest.TestCase):
    """local_diff_cover.sh must mirror run_coverage.sh's object-set passes.

    Anti-drift guard for #919: the local mirror previously only added
    build/test/* binaries to llvm-cov, so coverage data from CLI
    shell-out tests (cmd_coverage.cpp, cmd_loop.cpp, anything reached
    via pulp-cli / pulp-standalone / pulp-inspect) never propagated.
    The fix lifts run_coverage.sh's wider find passes; this test fails
    if either side drops one.
    """

    REQUIRED_FIND_ROOTS = [
        # (substring that must appear in the script, human description)
        ('"${BUILD_DIR}/test"', "test executables"),
        ("libpulp-*.a", "first-party static archives (Unix)"),
        ("pulp-*.lib", "first-party static archives (Windows)"),
        ('"${BUILD_DIR}/tools"', "non-test executables (CLI / standalone)"),
        ('"${BUILD_DIR}/inspect"', "non-test executables (inspector)"),
        ('"${BUILD_DIR}/bindings"', "loadable first-party modules"),
    ]

    def test_local_script_includes_all_object_roots(self) -> None:
        text = SCRIPT.read_text()
        for needle, desc in self.REQUIRED_FIND_ROOTS:
            self.assertIn(
                needle, text,
                f"local_diff_cover.sh missing object-discovery for {desc} "
                f"({needle!r}); without it llvm-cov drops coverage from "
                f"that surface — see #919.",
            )

    def test_ci_script_still_includes_all_object_roots(self) -> None:
        # If the CI script ever drops one of these, the local mirror is
        # the wrong place to keep it — fix CI first, then update both.
        if not CI_SCRIPT.exists():
            self.skipTest(f"CI script not present: {CI_SCRIPT}")
        text = CI_SCRIPT.read_text()
        for needle, desc in self.REQUIRED_FIND_ROOTS:
            self.assertIn(
                needle, text,
                f"scripts/run_coverage.sh missing object-discovery for "
                f"{desc} ({needle!r}); local_diff_cover.sh mirrors this "
                f"surface set, so dropping it here desyncs both lanes.",
            )


class DiffCoverExcludeContractTests(unittest.TestCase):
    """Locks in the diff_cover_excludes pattern + flag-shape contract.

    Anti-drift for the latent bug surfaced on PR #1005: every entry in
    `diff_cover_excludes` was a silent no-op since #919. Two compounding
    causes:

      1. PATTERN MATCHING — diff-cover's `--exclude` matches via fnmatch
         against the file's basename and absolute path only; literal
         relative paths like `tools/cli/cmd_loop.cpp` match neither and
         do nothing. Entries must be a basename (no slash) or a glob.
      2. ARGPARSE OVERWRITE — diff-cover's `--exclude` is `nargs='+'`
         with default action; repeated `--exclude=foo --exclude=bar`
         keeps only the LAST entry. Both the local script and the
         workflow must splat all entries under a single `--exclude
         val1 val2 ...` flag, not a per-entry `--exclude=PATH` loop.

    These tests make the next regression load.
    """

    WORKFLOW = REPO_ROOT / ".github" / "workflows" / "coverage.yml"

    def test_local_script_uses_single_exclude_flag_form(self) -> None:
        text = SCRIPT.read_text()
        # Old broken shape — must NOT appear:
        self.assertNotIn(
            '("--exclude=${excl}")', text,
            "local_diff_cover.sh appends --exclude=PATH per entry; "
            "argparse nargs='+' silently keeps only the last one. "
            "Build a list and splat under a single --exclude flag.",
        )
        # New shape: collect entries into EXCLUDE_LIST then pass under one --exclude.
        self.assertIn(
            '"--exclude" "${EXCLUDE_LIST[@]}"', text,
            "local_diff_cover.sh must build the diff-cover invocation "
            "with all excludes under a single --exclude flag (multi-value), "
            "not as repeated --exclude=PATH flags.",
        )

    def test_workflow_uses_single_exclude_flag_form(self) -> None:
        text = self.WORKFLOW.read_text()
        # Old broken shape:
        self.assertNotIn(
            '"--exclude=$e"', text,
            ".github/workflows/coverage.yml appends --exclude=PATH per entry; "
            "argparse nargs='+' silently keeps only the last one. "
            "Splat all entries under a single --exclude flag instead.",
        )
        # New shape:
        self.assertIn(
            '("--exclude" "${EXCLUDES[@]}")', text,
            ".github/workflows/coverage.yml must build the diff-cover "
            "invocation with all excludes under a single --exclude flag.",
        )

    def test_diff_cover_exclude_patterns_match_via_basename_or_glob(self) -> None:
        """Each `diff_cover_excludes` entry must be either a basename
        (no slash) or contain a glob character (`*`). Literal relative
        paths like `tools/cli/cmd_loop.cpp` don't match diff-cover's
        fnmatch-against-basename-or-abspath check and silently exclude
        nothing."""
        with CONFIG.open() as f:
            config = json.load(f)
        excludes = config.get("diff_cover_excludes", [])
        self.assertIsInstance(excludes, list)
        for pattern in excludes:
            is_basename = "/" not in pattern
            is_glob = "*" in pattern
            self.assertTrue(
                is_basename or is_glob,
                f"diff_cover_excludes entry {pattern!r} is a literal "
                f"relative path; diff-cover's --exclude won't match it "
                f"(fnmatch checks basename + absolute path only). "
                f"Use a basename like 'cmd_loop.cpp' or a glob like "
                f"'**/cmd_loop.cpp' instead.",
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
