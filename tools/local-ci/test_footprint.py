#!/usr/bin/env python3
"""Tests for the footprint size/footprint helpers.

Split out of test_local_ci.py (roadmap P11-3) so the test surface mirrors
the extracted footprint module. The harness still loads the local_ci.py
orchestrator, which re-exports the footprint symbols.
"""

import io
import importlib.util
import json
import os
import subprocess
import tempfile
import threading
import unittest
from urllib.parse import urlparse
from unittest import mock
from contextlib import redirect_stdout
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace


MODULE_PATH = Path(__file__).with_name("local_ci.py")
VALIDATE_BUILD_PATH = MODULE_PATH.parent.parent.parent / "validate-build.sh"


def load_module():
    spec = importlib.util.spec_from_file_location("pulp_local_ci", MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module



class FootprintTests(unittest.TestCase):
    def _set_target_enabled(self, name: str, enabled: bool):
        payload = json.loads(self.config_path.read_text())
        payload.setdefault("targets", {}).setdefault(name, {})["enabled"] = enabled
        self.config_path.write_text(json.dumps(payload) + "\n")

    def _write_desktop_manifest(self, config, target, action, manifest):
        bundle = self.mod.create_desktop_run_bundle(config, target, action)
        payload = dict(manifest)
        artifacts = dict(payload.get("artifacts", {}))
        artifacts.setdefault("bundle_dir", str(bundle))
        payload["artifacts"] = artifacts
        (bundle / "manifest.json").write_text(json.dumps(payload) + "\n")
        return bundle, payload

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        root = Path(self.tmpdir.name)
        self.state_dir = root / "state"
        self.config_path = root / "config.json"
        self.config_path.write_text(
            json.dumps(
                {
                    "desktop_automation": {
                        "artifact_root": str(root / "desktop-artifacts"),
                    },
                    "targets": {
                        "mac": {"type": "local", "enabled": True},
                        "ubuntu": {"type": "ssh", "enabled": True, "host": "ubuntu", "repo_path": "/tmp/pulp"},
                        "windows": {"type": "ssh", "enabled": False, "host": "win2", "repo_path": "C:\\Pulp"},
                    },
                    "github_actions": {
                        "repository": "danielraffel/pulp",
                        "defaults": {
                            "workflow": "build",
                            "provider": "github-hosted",
                            "wait_poll_secs": 5,
                            "match_timeout_secs": 30,
                        },
                        "workflows": {
                            "build": {
                                "providers": {
                                    "namespace": {
                                        "linux_runner_selector_json": "\"namespace-profile-default\"",
                                        "windows_runner_selector_json": "\"namespace-profile-default\"",
                                    }
                                }
                            },
                            "docs-check": {
                                "providers": {
                                    "namespace": {
                                        "runner_selector_json": "\"namespace-profile-default\""
                                    }
                                }
                            }
                        },
                    },
                    "defaults": {
                        "priority": "normal",
                        "targets": ["mac"],
                    },
                }
            )
            + "\n"
        )

        self.prev_home = os.environ.get("PULP_LOCAL_CI_HOME")
        self.prev_config = os.environ.get("PULP_LOCAL_CI_CONFIG")
        os.environ["PULP_LOCAL_CI_HOME"] = str(self.state_dir)
        os.environ["PULP_LOCAL_CI_CONFIG"] = str(self.config_path)
        self.mod = load_module()

    def tearDown(self):
        if self.prev_home is None:
            os.environ.pop("PULP_LOCAL_CI_HOME", None)
        else:
            os.environ["PULP_LOCAL_CI_HOME"] = self.prev_home

        if self.prev_config is None:
            os.environ.pop("PULP_LOCAL_CI_CONFIG", None)
        else:
            os.environ["PULP_LOCAL_CI_CONFIG"] = self.prev_config

        self.tmpdir.cleanup()


    def test_size_helpers_report_files_and_local_ci_state_footprint(self):
        self.assertEqual(self.mod.format_size_bytes(None), "")
        self.assertEqual(self.mod.format_size_bytes(""), "")
        self.assertEqual(self.mod.format_size_bytes(512), "512 B")
        self.assertEqual(self.mod.format_size_bytes(1536), "1.5 KB")
        self.assertEqual(self.mod.format_size_bytes(1024 * 1024), "1.0 MB")
        self.assertEqual(self.mod.format_size_bytes(1024 * 1024 * 1024), "1.0 GB")
        self.assertEqual(self.mod.format_size_bytes(1024 * 1024 * 1024 * 1024), "1.0 TB")
        self.assertEqual(self.mod.path_size_bytes(self.state_dir / "missing"), 0)

        bundle_dir = self.state_dir / "bundles"
        bundle_dir.mkdir(parents=True)
        (bundle_dir / "a.bundle").write_bytes(b"abc")
        nested = bundle_dir / "nested"
        nested.mkdir()
        (nested / "b.bundle").write_bytes(b"defg")

        self.assertEqual(self.mod.path_size_bytes(bundle_dir), 7)
        footprint = self.mod.local_ci_state_footprint()
        self.assertEqual(footprint["entries"]["bundles"]["size_bytes"], 7)
        self.assertEqual(footprint["total_bytes"], 7)
        self.assertEqual(
            self.mod.state_footprint_lines(footprint, indent="  "),
            [
                "  Local CI footprint: total=7 B",
                "    bundles: 7 B (bundles)",
                "    prepared: 0 B (prepared)",
                "    logs: 0 B (logs)",
                "    results: 0 B (results)",
                "    cloud-runs: 0 B (cloud-runs)",
            ],
        )
        self.assertEqual(self.mod.describe_path_for_cleanup(bundle_dir), "bundles")
        outside = Path(self.tmpdir.name) / "outside"
        self.assertEqual(self.mod.describe_path_for_cleanup(outside), str(outside))

    def test_path_size_handles_files_and_stat_errors(self):
        file_path = self.state_dir / "payload.bin"
        file_path.parent.mkdir(parents=True)
        file_path.write_bytes(b"abcd")
        self.assertEqual(self.mod.path_size_bytes(file_path), 4)

        with mock.patch.object(Path, "exists", side_effect=OSError("boom")):
            self.assertEqual(self.mod.path_size_bytes(file_path), 0)

        walk_root = self.state_dir / "walk"
        walk_root.mkdir()
        good = walk_root / "good.bin"
        bad = walk_root / "bad.bin"
        good.write_bytes(b"123")
        bad.write_bytes(b"4567")
        original_stat = Path.stat

        def flaky_stat(path, *args, **kwargs):
            if path == bad:
                raise OSError("stat failed")
            return original_stat(path, *args, **kwargs)

        with mock.patch.object(Path, "stat", flaky_stat):
            self.assertEqual(self.mod.path_size_bytes(walk_root), 3)

    def test_describe_path_for_cleanup_relativizes_nested_state_paths(self):
        inside = self.state_dir / "logs" / "job" / "mac.log"
        outside = Path(self.tmpdir.name) / "elsewhere.log"

        self.assertEqual(self.mod.describe_path_for_cleanup(inside), "logs/job/mac.log")
        self.assertEqual(self.mod.describe_path_for_cleanup(outside), str(outside))



if __name__ == "__main__":
    unittest.main()
