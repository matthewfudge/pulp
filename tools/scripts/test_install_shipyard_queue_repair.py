#!/usr/bin/env python3
"""Regression test for install-shipyard.sh queue-truncation repair.

Issue #534 (Codex post-merge review): the queue-file reset block used to
sit *after* the "already installed" short-circuit, so the documented
recovery path — "rerun the installer after Shipyard dies with
JSONDecodeError" — did not actually fire when the pinned binary was
already installed. This test exercises the script in --status mode (the
only mode that doesn't need network access) via a stubbed PULP_HOME with
a pre-populated install, and confirms that a truncated
`queue/queue.json` under the stubbed state dir is reinitialized even
though the installer reports the binary as already present.

Run:
    python3 tools/scripts/test_install_shipyard_queue_repair.py
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent.parent
INSTALL_SH = REPO_ROOT / "tools" / "install-shipyard.sh"


def _stub_state_dir(home: Path) -> Path:
    """Return the platform-specific Shipyard state dir under a fake HOME."""
    if sys.platform == "darwin":
        return home / "Library" / "Application Support" / "shipyard"
    if sys.platform.startswith("win"):
        return home / "AppData" / "Local" / "shipyard"
    return home / ".local" / "state" / "shipyard"


class InstallShipyardQueueRepair(unittest.TestCase):
    """#534 P1: queue repair must run before the already-installed early exit."""

    def _run_install(self, env: dict[str, str]) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["bash", str(INSTALL_SH)],
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_queue_repaired_even_when_already_installed(self) -> None:
        """Truncated queue.json gets repaired on the already-installed path."""
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp)

            # Pre-populate a fake Shipyard install so the "already installed"
            # short-circuit fires. We pin PULP_HOME to our tmp so install
            # locations live under the fake HOME.
            pulp_home = home / ".pulp"
            # Read pinned version from the real shipyard.toml so the path
            # matches what install-shipyard.sh computes.
            version = ""
            for line in (REPO_ROOT / "tools" / "shipyard.toml").read_text().splitlines():
                if line.startswith("version"):
                    version = line.split('"')[1]
                    break
            self.assertTrue(version, "could not parse version from shipyard.toml")

            install_dir = pulp_home / "shipyard" / version
            bin_dir = pulp_home / "bin"
            install_dir.mkdir(parents=True)
            bin_dir.mkdir(parents=True)
            bin_name = "shipyard.exe" if sys.platform.startswith("win") else "shipyard"
            installed = install_dir / bin_name
            installed.write_text("#!/bin/sh\necho stub\n")
            installed.chmod(0o755)
            symlink = bin_dir / bin_name
            try:
                symlink.symlink_to(installed)
            except OSError:
                self.skipTest("no symlink support on this host")

            # Write a truncated (zero-byte) queue file under the stubbed
            # state dir to simulate the #528 recovery scenario.
            state_dir = _stub_state_dir(home)
            queue_file = state_dir / "queue" / "queue.json"
            queue_file.parent.mkdir(parents=True)
            queue_file.touch()
            self.assertEqual(queue_file.stat().st_size, 0)

            env = os.environ.copy()
            env["HOME"] = str(home)
            env["PULP_HOME"] = str(pulp_home)
            # Unset XDG_STATE_HOME so Linux path falls back to ~/.local/state.
            env.pop("XDG_STATE_HOME", None)

            result = self._run_install(env)
            self.assertEqual(
                result.returncode, 0,
                msg=f"installer failed: stdout={result.stdout} stderr={result.stderr}",
            )

            # The queue file must now contain the reinitialized JSON even
            # though the script took the "already installed" short-circuit.
            body = queue_file.read_text()
            self.assertIn("jobs", body,
                          msg=f"queue file was not repaired; contents: {body!r}")
            self.assertIn("→ Shipyard queue file is empty — reinitializing",
                          result.stdout)

    def test_healthy_queue_file_is_untouched(self) -> None:
        """A non-empty queue file must NOT be overwritten."""
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp)
            pulp_home = home / ".pulp"
            # Minimal pre-install so the short-circuit fires.
            version = ""
            for line in (REPO_ROOT / "tools" / "shipyard.toml").read_text().splitlines():
                if line.startswith("version"):
                    version = line.split('"')[1]
                    break
            install_dir = pulp_home / "shipyard" / version
            bin_dir = pulp_home / "bin"
            install_dir.mkdir(parents=True)
            bin_dir.mkdir(parents=True)
            bin_name = "shipyard.exe" if sys.platform.startswith("win") else "shipyard"
            installed = install_dir / bin_name
            installed.write_text("#!/bin/sh\n")
            installed.chmod(0o755)
            symlink = bin_dir / bin_name
            try:
                symlink.symlink_to(installed)
            except OSError:
                self.skipTest("no symlink support on this host")

            state_dir = _stub_state_dir(home)
            queue_file = state_dir / "queue" / "queue.json"
            queue_file.parent.mkdir(parents=True)
            original = '{"jobs": [{"id": "abc", "status": "running"}]}'
            queue_file.write_text(original)

            env = os.environ.copy()
            env["HOME"] = str(home)
            env["PULP_HOME"] = str(pulp_home)
            env.pop("XDG_STATE_HOME", None)

            result = self._run_install(env)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(queue_file.read_text(), original,
                             msg="installer clobbered a healthy queue.json")


if __name__ == "__main__":
    unittest.main()
