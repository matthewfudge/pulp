#!/usr/bin/env python3
"""Focused unit coverage for skill_sync_check.py."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import skill_sync_check as ssc


REPO_ROOT = Path(__file__).resolve().parents[2]
SKILL_PATH_MAP = REPO_ROOT / "tools" / "scripts" / "skill_path_map.json"


class SkillSyncCheckTests(unittest.TestCase):

    def test_load_config_resolves_repo_relative_paths_and_trailer_key(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            config = repo / "versioning.json"
            config.write_text(json.dumps({
                "$schema": "ignored",
                "_comment": "ignored",
                "skills": {
                    "skills_dir": ".agents/skills",
                    "path_map": "tools/scripts/skill_path_map.json",
                },
                "generated_globs": ["build/**"],
                "trailers": {"skill_update": "Skill-Gate"},
            }))

            loaded = ssc.load_config(config, repo)

        self.assertEqual(loaded.skills_dir, repo / ".agents" / "skills")
        self.assertEqual(
            loaded.path_map_file,
            repo / "tools" / "scripts" / "skill_path_map.json",
        )
        self.assertEqual(loaded.generated_globs, ["build/**"])
        self.assertEqual(loaded.trailer_skill_update, "Skill-Gate")

    def test_load_skill_map_ignores_metadata_and_empty_entries(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "skill_path_map.json"
            path.write_text(json.dumps({
                "$schema": "ignored",
                "_comment": "ignored",
                "skills": {
                    "ci": {"paths": [".github/workflows/**"]},
                    "empty": {},
                },
            }))

            loaded = ssc.load_skill_map(path)

        self.assertEqual(loaded.skills["ci"], [".github/workflows/**"])
        self.assertEqual(loaded.skills["empty"], [])

    def test_compute_findings_requires_skill_md_not_sidecar_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            skills_dir = repo / ".agents" / "skills"
            changed = [
                "ci/build.yml",
                ".agents/skills/ci/notes.md",
            ]

            findings = ssc.compute_findings(
                changed=changed,
                skill_map=ssc.SkillMap({"ci": ["ci/**"]}),
                skills_dir=skills_dir,
                repo=repo,
                bypasses={},
            )

        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].touched_paths, ["ci/build.yml"])
        self.assertFalse(findings[0].skill_md_modified)

    def test_render_report_fails_only_in_enforcing_modes(self) -> None:
        finding = ssc.Finding(
            skill="ci",
            touched_paths=["ci/build.yml"],
            skill_md_modified=False,
            bypass_reason=None,
        )

        report_text, report_code = ssc.render_report(
            [finding],
            trailer_key="Skill-Update",
            mode="report",
        )
        hint_text, hint_code = ssc.render_report(
            [finding],
            trailer_key="Skill-Update",
            mode="hint",
        )

        self.assertEqual(report_code, 1)
        self.assertIn("Skill-sync check FAILED", report_text)
        self.assertIn('Skill-Update: skip skill=ci reason="..."', report_text)
        self.assertEqual(hint_code, 0)
        self.assertIn("SKILL.md NOT updated", hint_text)


class SkillPathMapTests(unittest.TestCase):
    """Anti-drift contract for tools/scripts/skill_path_map.json.

    Mirrors the audit recipe in issue #1053. Same shape family as the
    diff-cover empty-match bug fixed in efefe144 + b258730c (#1005):
    a path pattern that matches NOTHING is a silent no-op — the skill
    never gets a sync-check trigger, and a typo or rename can quietly
    disable enforcement until a contributor stumbles on it.

    Every glob in `paths` MUST match at least one tracked file. Patterns
    that are intentionally future-facing (e.g. developer-supplied SDKs
    that ship outside the source tree) MUST be declared under
    `_doc.expected_empty` instead, with a one-line reason.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.skill_map_raw = json.loads(SKILL_PATH_MAP.read_text())
        cls.tracked_files = subprocess.check_output(
            ["git", "ls-files"],
            cwd=REPO_ROOT,
            text=True,
        ).splitlines()

    def _matches(self, pattern: str) -> list[str]:
        return [f for f in self.tracked_files if ssc._glob_match(f, pattern)]

    def test_every_paths_pattern_matches_at_least_one_tracked_file(self) -> None:
        # The core anti-drift contract — added back any empty-match
        # pattern (typo, renamed file, never-existed path) and this
        # test fails loudly with the offending skill+pattern.
        empty: list[tuple[str, str]] = []
        for skill, entry in self.skill_map_raw.get("skills", {}).items():
            for pat in entry.get("paths", []):
                if not self._matches(pat):
                    empty.append((skill, pat))
        self.assertFalse(
            empty,
            "skill_path_map.json contains paths patterns that match zero "
            "tracked files (silent no-ops). Either fix the pattern, "
            "delete it, or move it to the entry's `_doc.expected_empty` "
            "with a reason if it's intentionally future-facing:\n  "
            + "\n  ".join(f"{skill}: {pat!r}" for skill, pat in empty),
        )

    def test_expected_empty_patterns_actually_match_nothing(self) -> None:
        # Guard rail on the `_doc.expected_empty` escape hatch: anything
        # listed there must currently match zero tracked files. If a
        # listed path *does* exist in the tree, the entry should be
        # promoted into `paths` instead — keeping `_doc.expected_empty`
        # narrowly scoped to genuinely external/future-facing surfaces.
        wrong: list[tuple[str, str]] = []
        for skill, entry in self.skill_map_raw.get("skills", {}).items():
            doc = entry.get("_doc") or {}
            expected_empty = doc.get("expected_empty") or {}
            for pat in expected_empty:
                if self._matches(pat):
                    wrong.append((skill, pat))
        self.assertFalse(
            wrong,
            "skill_path_map.json `_doc.expected_empty` entries must "
            "currently match zero tracked files. Move these into "
            "`paths` instead so they actually gate the skill:\n  "
            + "\n  ".join(f"{skill}: {pat!r}" for skill, pat in wrong),
        )

    def test_expected_empty_entries_have_reason_strings(self) -> None:
        # `_doc.expected_empty` is a dict of pattern -> reason. Empty or
        # missing reason strings defeat the purpose of the annotation.
        bad: list[tuple[str, str]] = []
        for skill, entry in self.skill_map_raw.get("skills", {}).items():
            doc = entry.get("_doc") or {}
            expected_empty = doc.get("expected_empty") or {}
            if not isinstance(expected_empty, dict):
                bad.append((skill, "_doc.expected_empty must be a dict of pattern->reason"))
                continue
            for pat, reason in expected_empty.items():
                if not isinstance(reason, str) or not reason.strip():
                    bad.append((skill, f"missing reason for {pat!r}"))
        self.assertFalse(
            bad,
            "skill_path_map.json `_doc.expected_empty` entries must each "
            "carry a non-empty reason string:\n  "
            + "\n  ".join(f"{skill}: {msg}" for skill, msg in bad),
        )


if __name__ == "__main__":
    unittest.main()
