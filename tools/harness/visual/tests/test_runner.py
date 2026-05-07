"""Tests for visual snapshot fixture discovery and coverage counts."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.harness.visual import runner  # noqa: E402
from tools.harness.visual import spec  # noqa: E402


class RunnerTests(unittest.TestCase):
    def test_resolve_fixtures_accepts_surface_prefixed_entries(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture_dir = root / "tools" / "harness" / "visual" / "fixtures" / "yoga"
            fixture_dir.mkdir(parents=True)
            fixture = fixture_dir / "box-sizing.json"
            fixture.write_text("{}", encoding="utf-8")

            self.assertEqual(
                runner.resolve_fixtures(root, "yoga", ["yoga/box-sizing"]),
                [fixture],
            )

    def test_fixture_spec_keeps_existing_yoga_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture_dir = root / "tools" / "harness" / "visual" / "fixtures" / "yoga"
            fixture_dir.mkdir(parents=True)
            fixture = fixture_dir / "box-sizing.json"
            fixture.write_text("{}", encoding="utf-8")

            parsed = spec.fixture_spec_from_file(fixture)

            self.assertEqual(parsed.id, "yoga/box-sizing")
            self.assertEqual(parsed.surface, "yoga")
            self.assertEqual(parsed.driver, "native_tree")
            self.assertEqual(parsed.capture_format, "json")
            self.assertEqual(
                spec.golden_path(root, parsed),
                root
                / "tools"
                / "harness"
                / "visual"
                / "goldens"
                / "yoga"
                / "box-sizing.json",
            )

    def test_visual_counts_use_typed_runtime_refs_and_supported_denominator(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._write_fixture_with_golden(root, "yoga", "a")
            (root / "compat.json").write_text(
                json.dumps(
                    {
                        "yoga": {
                            "yoga/a": {"status": "supported", "tests": ["semantic:yoga/a"]},
                            "yoga/b": {"status": "supported", "tests": ["unit:test_b"]},
                            "yoga/c": {"status": "missing", "tests": ["semantic:yoga/c"]},
                            "yoga/__note": {
                                "status": "supported",
                                "tests": ["semantic:yoga/note"],
                            },
                            "yoga/d": {"status": "supported", "tests": ["cannot-validate:issue-1"]},
                            "yoga/stale": {
                                "status": "supported",
                                "tests": ["semantic:yoga/stale"],
                            },
                        }
                    }
                ),
                encoding="utf-8",
            )

            visual_counts = runner.visual_pass_counts(root, ["yoga"])
            validation_counts = runner.validation_route_counts(root, ["yoga"])

            self.assertEqual(visual_counts["yoga"]["pass"], 1)
            self.assertEqual(visual_counts["yoga"]["total"], 3)
            self.assertEqual(visual_counts["yoga"]["label"], "1/3")
            self.assertEqual(
                visual_counts["yoga"]["missing_refs"],
                ["yoga/stale: semantic:yoga/stale"],
            )
            self.assertEqual(validation_counts["yoga"]["pass"], 3)
            self.assertEqual(validation_counts["yoga"]["total"], 4)
            self.assertEqual(validation_counts["yoga"]["excluded"], 1)
            self.assertEqual(validation_counts["yoga"]["unvalidated_entries"], ["yoga/stale"])

    def test_duplicate_fixture_ids_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture_dir = root / "tools" / "harness" / "visual" / "fixtures" / "yoga"
            fixture_dir.mkdir(parents=True)
            (fixture_dir / "a.json").write_text(
                json.dumps({"id": "yoga/shared"}),
                encoding="utf-8",
            )
            (fixture_dir / "b.json").write_text(
                json.dumps({"id": "yoga/shared"}),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "duplicate visual fixture id"):
                spec.fixture_ids_with_goldens(root, ["yoga"])

    def test_orphaned_golden_paths_are_reported(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self._write_fixture_with_golden(root, "yoga", "a")
            golden_dir = root / "tools" / "harness" / "visual" / "goldens" / "yoga"
            orphan = golden_dir / "orphan.json"
            orphan.write_text("{}", encoding="utf-8")

            self.assertEqual(spec.orphaned_golden_paths(root, ["yoga"]), [orphan])

    def _write_fixture_with_golden(self, root: Path, surface: str, name: str) -> None:
        fixture_dir = root / "tools" / "harness" / "visual" / "fixtures" / surface
        golden_dir = root / "tools" / "harness" / "visual" / "goldens" / surface
        fixture_dir.mkdir(parents=True, exist_ok=True)
        golden_dir.mkdir(parents=True, exist_ok=True)
        (fixture_dir / f"{name}.json").write_text(
            json.dumps({"id": f"{surface}/{name}"}),
            encoding="utf-8",
        )
        (golden_dir / f"{name}.json").write_text("{}", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
