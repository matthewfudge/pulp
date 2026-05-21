#!/usr/bin/env python3
"""Fixture tests for the version_bump Surface cluster.

Config loading + version-file readers/writers. Mirrors
`version_bump_surfaces.py`. Split from `test_gates.py` (P9-NEW refactor,
2026-05); test bodies are byte-identical to their previous definitions.

Runs standalone (`python3 tools/scripts/test_version_bump_surfaces.py`)
or as part of the aggregate suite via `test_gates.py`.
"""

from __future__ import annotations

import json
import unittest

from gate_test_support import GateFixtureTestCase, VBC, _run


class VersionBumpSurfacesTests(GateFixtureTestCase):
    """version_bump_check Surface-cluster fixtures."""

    def test_version_file_helpers_cover_supported_kinds(self) -> None:
        """Direct coverage for the version-file readers/writers that
        integration fixtures only hit through the CMake + JSON paths."""
        vbc = self._import_gate_module("version_bump_check")

        cases = [
            (
                "package.json",
                '{"name": "demo", "version": "1.2.3"}\n',
                vbc.VersionFile("package.json", "json_field", "version"),
                "1.2.3",
                "2.0.0",
                '"version": "2.0.0"',
            ),
            (
                "pyproject.toml",
                '[project]\nname = "demo"\nversion = "0.4.0"\n',
                vbc.VersionFile("pyproject.toml", "pyproject_version"),
                "0.4.0",
                "0.5.0",
                'version = "0.5.0"',
            ),
            (
                "pkg/__init__.py",
                '__version__ = "3.2.1"\n',
                vbc.VersionFile("pkg/__init__.py", "python_dunder_version"),
                "3.2.1",
                "3.2.2",
                '__version__ = "3.2.2"',
            ),
            (
                "VERSION.txt",
                "tool_version = 7.8.9\n",
                vbc.VersionFile("VERSION.txt", "regex",
                                pattern=r"tool_version\s*=\s*(\d+\.\d+\.\d+)"),
                "7.8.9",
                "8.0.0",
                "tool_version = 8.0.0",
            ),
        ]

        for rel, text, vf, old, new, expected in cases:
            self.f.write(rel, text)
            self.assertEqual(vbc.read_version(self.tmp, vf), old)
            self.assertTrue(vbc.write_version(self.tmp, vf, new))
            written = (self.tmp / rel).read_text()
            self.assertIn(expected, written)

        self.f.write("bad.json", "{not json}\n")
        self.assertIsNone(
            vbc.read_version(self.tmp, vbc.VersionFile("bad.json", "json_field", "version"))
        )
        self.assertFalse(
            vbc.write_version(
                self.tmp,
                vbc.VersionFile("missing.toml", "pyproject_version"),
                "1.0.0",
            )
        )

    def test_version_config_loader_strips_metadata(self) -> None:
        vbc = self._import_gate_module("version_bump_check")

        cfg_path = self.tmp / "custom-versioning.json"
        cfg_path.write_text(json.dumps({
            "$schema": "https://example.invalid/schema.json",
            "_comment": "ignored top-level metadata",
            "generated_globs": ["build/**"],
            "trailers": {"version_bump": "Custom-Bump"},
            "surfaces": {
                "sdk": {
                    "_comment": "ignored surface metadata",
                    "label": "SDK",
                    "version_files": [{
                        "_note": "ignored version-file metadata",
                        "path": "CMakeLists.txt",
                        "kind": "cmake_project_version",
                    }],
                    "trigger_paths": ["core/**"],
                    "public_api_paths": ["core/**/include/**"],
                    "internal_only_paths": ["core/**/src/**"],
                    "changelog": "CHANGELOG.md",
                },
            },
        }, indent=2) + "\n")

        cfg = vbc.load_config(cfg_path)
        self.assertEqual(cfg.generated_globs, ["build/**"])
        self.assertEqual(cfg.trailer_version_bump, "Custom-Bump")
        self.assertEqual(len(cfg.surfaces), 1)
        self.assertEqual(cfg.surfaces[0].name, "sdk")
        self.assertEqual(cfg.surfaces[0].version_files[0].path, "CMakeLists.txt")
        self.assertEqual(cfg.surfaces[0].changelog, "CHANGELOG.md")

    def test_version_bump_missing_config_returns_usage_error(self) -> None:
        code, out = _run(
            [
                "python3", str(VBC),
                "--base", "origin/main",
                "--config", str(self.tmp / "missing-versioning.json"),
            ],
            cwd=self.tmp,
        )
        self.assertEqual(code, 2, msg=out)
        self.assertIn("config not found", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
