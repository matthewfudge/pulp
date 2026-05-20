#!/usr/bin/env python3
"""Tests for tools/ci/bootstrap-macos-host.sh.

The bootstrap script provisions a Mac as a Pulp self-hosted CI runner
host. It is the turnkey artifact the M5 setup reuses verbatim, so it is
safety-critical: a broken, non-idempotent, or silently-passing bootstrap
mis-provisions a CI host. These tests pin the script's contract —
argument handling, syntax, and the `--check` dry run (which must report
every provisioning phase and mutate nothing).

Cross-platform tests run everywhere; the `--check` dry-run tests are
macOS-only (the script hard-exits at preflight off macOS by design).

Run:  python3 tools/ci/test_bootstrap_macos_host.py
"""
from __future__ import annotations

import os
import platform
import subprocess
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("bootstrap-macos-host.sh")
BREWFILE = Path(__file__).with_name("Brewfile")
IS_MACOS = platform.system() == "Darwin"


def _run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["bash", str(SCRIPT), *args],
        capture_output=True, text=True, check=False,
    )


class ScriptContractTests(unittest.TestCase):
    """Cross-platform — syntax + argument handling."""

    def test_script_exists_and_is_executable(self) -> None:
        self.assertTrue(SCRIPT.is_file(), SCRIPT)
        self.assertTrue(os.access(SCRIPT, os.X_OK), f"{SCRIPT} not executable")

    def test_syntax_is_valid(self) -> None:
        r = subprocess.run(["bash", "-n", str(SCRIPT)],
                           capture_output=True, text=True, check=False)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_help_exits_zero_with_usage(self) -> None:
        r = _run("--help")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("Usage", r.stdout + r.stderr)

    def test_unknown_arg_exits_2(self) -> None:
        # An unrecognized flag must fail loudly, not be silently ignored.
        r = _run("--definitely-not-a-real-flag")
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)

    def test_runner_env_enables_ccache_depend_mode(self) -> None:
        # The per-runner .env enables ccache depend mode + a conservative
        # sloppiness set — the one build-speed lever validated as worth
        # it (the other proposed optimizations were no-ops on review).
        body = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("CCACHE_DEPEND=true", body)
        self.assertIn("CCACHE_SLOPPINESS=time_macros,pch_defines", body)


class BrewfileTests(unittest.TestCase):
    """The Brewfile the bootstrap consumes."""

    def test_brewfile_present(self) -> None:
        self.assertTrue(BREWFILE.is_file(), BREWFILE)

    def test_brewfile_lists_core_toolchain(self) -> None:
        body = BREWFILE.read_text(encoding="utf-8")
        for dep in ("cmake", "ninja", "ccache", "gh", "git-lfs"):
            self.assertIn(f'"{dep}"', body, f"Brewfile missing {dep}")


@unittest.skipUnless(IS_MACOS, "bootstrap-macos-host.sh runs only on macOS")
class CheckModeTests(unittest.TestCase):
    """macOS-only — the `--check` dry run."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.result = _run("--check")

    def test_check_mode_exits_zero(self) -> None:
        self.assertEqual(self.result.returncode, 0, self.result.stderr)

    def test_check_mode_reports_every_phase(self) -> None:
        out = self.result.stdout
        for phase in ("Preflight", "Xcode", "Homebrew dependencies",
                      "Cache layout", "ccache tuning", "Skia",
                      "Shipyard", "Verify build dependencies", "Done"):
            self.assertIn(phase, out, f"phase '{phase}' missing from --check")

    def test_check_mode_is_a_dry_run(self) -> None:
        # Every mutating command must be reported as "would run:", never
        # executed — that is the whole guarantee of --check.
        self.assertIn("would run:", self.result.stdout)

    def test_check_mode_confirms_provisioned(self) -> None:
        self.assertIn("host provisioned", self.result.stdout)

    def test_check_mode_runner_phase_skipped_without_flag(self) -> None:
        # Without --with-runners, registration must not happen.
        self.assertIn("runner registration skipped", self.result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
