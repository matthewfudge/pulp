#!/usr/bin/env python3
"""Unit tests for run_reaper_hostbench_smoke.py."""

from __future__ import annotations

import importlib.util
import pathlib
import sys
import tempfile
import time
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parent / "run_reaper_hostbench_smoke.py"
spec = importlib.util.spec_from_file_location("run_reaper_hostbench_smoke", SCRIPT)
assert spec and spec.loader
runner = importlib.util.module_from_spec(spec)
sys.modules["run_reaper_hostbench_smoke"] = runner
spec.loader.exec_module(runner)


class ReaperHostBenchSmokeTests(unittest.TestCase):
    def test_generated_lua_uses_format_specific_fx_name(self) -> None:
        vst3 = runner.build_lua_script(runner.FORMAT_CONFIGS["vst3"])
        clap = runner.build_lua_script(runner.FORMAT_CONFIGS["clap"])

        self.assertIn("VST3: PulpHostBench (Pulp)", vst3)
        self.assertIn("CLAP: PulpHostBench (Pulp)", clap)
        self.assertIn("TrackFX_AddByName", vst3)
        self.assertIn("PULP_REAPER_HOSTBENCH_STATUS", clap)
        self.assertIn("Transport: Play", vst3)

    def test_newest_added_log_ignores_existing_logs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            old = root / "REAPER-VST3-old.log"
            old.write_text("old", encoding="utf-8")
            before = runner.snapshot_logs(root, "REAPER-VST3-*.log")

            time.sleep(0.01)
            new = root / "REAPER-VST3-new.log"
            new.write_text("new", encoding="utf-8")

            self.assertEqual(
                runner.newest_added_log(before, root, "REAPER-VST3-*.log"),
                new.resolve(),
            )

    def test_status_success_requires_fx_and_done(self) -> None:
        self.assertTrue(runner.status_has_success("start version=7.74\nfx=0 name=x\ndone ticks=24\n"))
        self.assertFalse(runner.status_has_success("start version=7.74\nfx=-1 name=\n"))
        self.assertFalse(runner.status_has_success("start version=7.74\nerror=no_track\n"))
        self.assertFalse(runner.status_has_success("start version=7.74\nfx=0 name=x\n"))

    def test_log_event_counts_and_required_events(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "REAPER-VST3.log"
            path.write_text(
                "2026-06-13T00:00:00Z\tsession_start\tn=1\n"
                "2026-06-13T00:00:00Z\tprocessor_construct\tn=2\n"
                "2026-06-13T00:00:00Z\tdefine_parameters\tn=3\n"
                "2026-06-13T00:00:00Z\tprepare\tn=4\n"
                "2026-06-13T00:00:00Z\tprepare\tn=5\n",
                encoding="utf-8",
            )
            counts = runner.log_event_counts(path)
            self.assertEqual(counts["prepare"], 2)
            self.assertEqual(runner.missing_required_events(counts), [])
            del counts["prepare"]
            self.assertEqual(runner.missing_required_events(counts), ["prepare"])

    def test_print_lua_mode_does_not_require_reaper_binary(self) -> None:
        self.assertEqual(runner.main([
            "--format", "vst3",
            "--reaper", "/definitely/missing/REAPER",
            "--print-lua",
        ]), 0)


if __name__ == "__main__":
    unittest.main()
