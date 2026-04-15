#!/usr/bin/env python3
"""Tests for tools/check_status_ladder.py."""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from textwrap import dedent

_HERE = Path(__file__).resolve().parent
_SPEC = importlib.util.spec_from_file_location(
    "check_status_ladder", _HERE / "check_status_ladder.py"
)
assert _SPEC and _SPEC.loader
csl = importlib.util.module_from_spec(_SPEC)
sys.modules["check_status_ladder"] = csl
_SPEC.loader.exec_module(csl)


# A tiny synthetic matrix exercising every status branch.
SYNTHETIC_MATRIX = dedent(
    """\
    schema_version: 2
    capability_groups:
      group_a:
        feature_well_linked:
          status: usable
          ci_test: pulp-test-feature-a
          notes: Properly linked.
        feature_two_tests:
          status: usable
          ci_test:
            - pulp-test-feature-b
            - pulp-test-feature-b-roundtrip
          notes: List-form ci_test.
        feature_missing_link:
          status: usable
          notes: No ci_test.
        feature_bogus_link:
          status: usable
          ci_test: pulp-test-does-not-exist
          notes: Points at non-existent target.
        feature_partial_ok:
          status: partial
          notes: partial does not require ci_test.
        feature_planned_ok:
          status: planned
          notes: planned does not require ci_test.
        feature_stable_linked:
          status: stable
          ci_test: pulp-test-feature-c
          notes: stable still requires ci_test.
    """
)

# Synthetic test/CMakeLists.txt with a known target set.
SYNTHETIC_CMAKE = dedent(
    """\
    add_executable(pulp-test-feature-a test_a.cpp)
    add_executable(pulp-test-feature-b test_b.cpp)
    add_executable(pulp-test-feature-b-roundtrip test_b_rt.cpp)
    add_executable(pulp-test-feature-c test_c.cpp)
    """
)


def _write(text: str, suffix: str) -> Path:
    f = tempfile.NamedTemporaryFile("w", suffix=suffix, delete=False)
    f.write(text)
    f.close()
    return Path(f.name)


class ParserTests(unittest.TestCase):
    def test_parse_entries_finds_every_status(self):
        entries = csl.parse_entries(SYNTHETIC_MATRIX)
        statuses = sorted(e["status"] for e in entries)
        self.assertEqual(
            statuses,
            ["partial", "planned", "stable", "usable", "usable", "usable", "usable"],
        )

    def test_parse_entries_captures_ci_test_inline(self):
        entries = csl.parse_entries(SYNTHETIC_MATRIX)
        e = next(e for e in entries if e["path"].endswith("feature_well_linked"))
        self.assertEqual(e["ci_test"], ["pulp-test-feature-a"])

    def test_parse_entries_captures_ci_test_list(self):
        entries = csl.parse_entries(SYNTHETIC_MATRIX)
        e = next(e for e in entries if e["path"].endswith("feature_two_tests"))
        self.assertEqual(
            e["ci_test"],
            ["pulp-test-feature-b", "pulp-test-feature-b-roundtrip"],
        )

    def test_load_test_targets(self):
        cmake_path = _write(SYNTHETIC_CMAKE, ".txt")
        targets = csl.load_test_targets(cmake_path)
        self.assertEqual(
            targets,
            {
                "pulp-test-feature-a",
                "pulp-test-feature-b",
                "pulp-test-feature-b-roundtrip",
                "pulp-test-feature-c",
            },
        )


class CheckTests(unittest.TestCase):
    def setUp(self):
        self.entries = csl.parse_entries(SYNTHETIC_MATRIX)
        self.targets = {
            "pulp-test-feature-a",
            "pulp-test-feature-b",
            "pulp-test-feature-b-roundtrip",
            "pulp-test-feature-c",
        }

    def test_well_linked_entries_pass(self):
        ok = [e for e in self.entries if e["path"].endswith(
            ("feature_well_linked", "feature_two_tests", "feature_stable_linked")
        )]
        problems = csl.check(ok, self.targets)
        self.assertEqual(problems, [])

    def test_missing_ci_test_is_violation(self):
        bad = [e for e in self.entries if e["path"].endswith("feature_missing_link")]
        problems = csl.check(bad, self.targets)
        self.assertEqual(len(problems), 1)
        self.assertIn("requires `ci_test:`", problems[0])

    def test_bogus_ci_test_is_violation(self):
        bad = [e for e in self.entries if e["path"].endswith("feature_bogus_link")]
        problems = csl.check(bad, self.targets)
        self.assertEqual(len(problems), 1)
        self.assertIn("not an add_executable target", problems[0])

    def test_partial_status_is_exempt(self):
        partials = [e for e in self.entries if e["status"] == "partial"]
        problems = csl.check(partials, self.targets)
        self.assertEqual(problems, [])

    def test_planned_status_is_exempt(self):
        planned = [e for e in self.entries if e["status"] == "planned"]
        problems = csl.check(planned, self.targets)
        self.assertEqual(problems, [])


class ModeTests(unittest.TestCase):
    def setUp(self):
        self.matrix = _write(SYNTHETIC_MATRIX, ".yaml")
        self.cmake = _write(SYNTHETIC_CMAKE, ".txt")

    def test_report_mode_returns_zero_even_on_violations(self):
        rc = csl.main([
            "--mode=report",
            "--matrix", str(self.matrix),
            "--cmake", str(self.cmake),
        ])
        self.assertEqual(rc, 0)

    def test_block_mode_returns_one_on_violations(self):
        rc = csl.main([
            "--mode=block",
            "--matrix", str(self.matrix),
            "--cmake", str(self.cmake),
        ])
        self.assertEqual(rc, 1)


if __name__ == "__main__":
    unittest.main()
