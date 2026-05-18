#!/usr/bin/env python3
"""format_baseline_diff.py — companion-track U-3.

Diff the current validator output (auval/pluginval/clap-validator) on a
representative Pulp plugin against the committed baseline in
test/fixtures/format-baseline/. Fail if anything outside the
normalized-noise allowlist has changed.

Designed to run in CI as the gate for PRs that touch core/format/ or
core/host/plugin_slot_*. Behaviour:

1. Re-run format_baseline_capture.sh in a temp output directory.
2. For each captured file, compare line-by-line to the committed
   baseline.
3. Report the first N diff lines per file with a clear remediation
   message ("If this change is intentional, run
   tools/scripts/format_baseline_capture.sh and commit the updated
   fixtures.").

If no validators are available on the host, the script exits with code
2 (skipped) rather than 0 — CI workflows should require the gate to
PASS, not just NOT-FAIL, so a skipped run is treated as a missing
signal that the CI configuration must address.

Usage:
    tools/scripts/format_baseline_diff.py [--plugin <name>]
                                         [--baseline-dir <dir>]
                                         [--max-diff-lines <n>]
"""
from __future__ import annotations

import argparse
import difflib
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--plugin", default="PulpEffect")
    p.add_argument("--baseline-dir", default="test/fixtures/format-baseline")
    p.add_argument("--max-diff-lines", type=int, default=40,
                   help="show at most N diff lines per validator")
    args = p.parse_args(argv)

    root = Path(subprocess.check_output(
        ["git", "rev-parse", "--show-toplevel"]).decode().strip())
    baseline_dir = root / args.baseline_dir
    if not baseline_dir.is_dir():
        sys.stderr.write(
            f"[format-baseline-diff] No baseline directory at "
            f"{baseline_dir} — run "
            f"tools/scripts/format_baseline_capture.sh first.\n"
        )
        return 1

    capture_script = root / "tools/scripts/format_baseline_capture.sh"
    if not capture_script.is_file():
        sys.stderr.write(
            f"[format-baseline-diff] Capture script missing at "
            f"{capture_script}.\n"
        )
        return 1

    with tempfile.TemporaryDirectory(prefix="pulp-baseline-diff-") as tmpdir:
        rc = subprocess.run(
            [str(capture_script), "--plugin", args.plugin,
             "--output", tmpdir],
            cwd=root,
        ).returncode
        if rc == 2:
            sys.stderr.write(
                "[format-baseline-diff] No validators available on this "
                "host — gate skipped. CI configuration must ensure at "
                "least one validator is installed.\n"
            )
            return 2
        if rc != 0:
            sys.stderr.write(
                f"[format-baseline-diff] Capture script exited {rc}. "
                "Cannot perform diff.\n"
            )
            return rc

        new_files = sorted(Path(tmpdir).iterdir())
        if not new_files:
            sys.stderr.write(
                "[format-baseline-diff] Capture produced no files. "
                "Plugin not installed or validators all skipped.\n"
            )
            return 2

        diffs_found = 0
        missing_baseline = 0
        for new in new_files:
            base = baseline_dir / new.name
            if not base.exists():
                sys.stderr.write(
                    f"[format-baseline-diff] No committed baseline for "
                    f"{new.name}. Run "
                    f"tools/scripts/format_baseline_capture.sh and commit.\n"
                )
                missing_baseline += 1
                continue
            new_lines = new.read_text(errors="replace").splitlines(keepends=False)
            base_lines = base.read_text(errors="replace").splitlines(keepends=False)
            if new_lines == base_lines:
                continue
            diffs_found += 1
            sys.stderr.write(
                f"\n[format-baseline-diff] DIFF in {new.name}:\n"
            )
            diff = list(difflib.unified_diff(
                base_lines, new_lines,
                fromfile=f"baseline/{new.name}",
                tofile=f"current/{new.name}",
                lineterm="",
            ))
            for i, line in enumerate(diff[: args.max_diff_lines]):
                sys.stderr.write(line + "\n")
            if len(diff) > args.max_diff_lines:
                sys.stderr.write(
                    f"  ... {len(diff) - args.max_diff_lines} more diff "
                    "lines truncated.\n"
                )

        if missing_baseline or diffs_found:
            sys.stderr.write(
                f"\n[format-baseline-diff] BLOCKED: "
                f"{diffs_found} diff(s), {missing_baseline} missing "
                f"baseline(s).\n"
                "If this change is intentional, run:\n"
                "  tools/scripts/format_baseline_capture.sh\n"
                "and commit the updated test/fixtures/format-baseline/ "
                "fixtures in the same PR.\n"
            )
            return 1

    sys.stderr.write(
        f"[format-baseline-diff] OK — all "
        f"{len(new_files)} validator output(s) match the committed "
        "baseline.\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
