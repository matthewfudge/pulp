#!/usr/bin/env python3
"""Verify renderer probe route-inventory checker rejects drift."""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def run_verifier(verifier, probe_verifier, tools_dir, ctest_file):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--probe-verifier",
            str(probe_verifier),
            "--tools-dir",
            str(tools_dir),
            "--ctest-file",
            str(ctest_file),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def replace_once(text, old, new):
    if old not in text:
        raise ValueError(f"missing text to replace: {old!r}")
    return text.replace(old, new, 1)


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_route_inventory_contract_case={name}")


def copy_route_files(source_dir, target_dir):
    for path in source_dir.glob("renderer_probe_*_route_*.py"):
        shutil.copy2(path, target_dir / path.name)


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe route inventory drift is rejected.")
    parser.add_argument("--inventory-verifier", type=Path, required=True)
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--tools-dir", type=Path, required=True)
    parser.add_argument("--ctest-file", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("inventory_verifier", args.inventory_verifier),
            ("probe_verifier", args.probe_verifier),
            ("tools_dir", args.tools_dir),
            ("ctest_file", args.ctest_file)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    probe_text = read_text(args.probe_verifier)
    ctest_text = read_text(args.ctest_file)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        tools_dir = tmpdir / "tools"
        tools_dir.mkdir()
        probe = tmpdir / "verify_renderer_probe.py"
        ctest = tmpdir / "CMakeLists.txt"

        def write_case(probe_body=probe_text, ctest_body=ctest_text):
            write_text(probe, probe_body)
            write_text(ctest, ctest_body)

        def reset_tools_dir():
            shutil.rmtree(tools_dir)
            tools_dir.mkdir()
            copy_route_files(args.tools_dir, tools_dir)

        reset_tools_dir()
        write_case()
        expect_case(
            "valid-current-route-inventory",
            run_verifier(args.inventory_verifier, probe, tools_dir, ctest),
            0,
            "renderer_probe_route_inventory_verified=",
            errors,
        )

        uncovered_probe = replace_once(
            probe_text,
            '    "texture_uploaded",\n',
            '    "texture_uploaded",\n    "uncovered_route_field",\n',
        )
        write_case(probe_body=uncovered_probe)
        expect_case(
            "missing-route-field-mention",
            run_verifier(args.inventory_verifier, probe, tools_dir, ctest),
            1,
            "missing route mention for probe field: uncovered_route_field",
            errors,
        )

        missing_ctest = ctest_text.replace(
            "renderer_probe_unlit_route_smoke.py",
            "renderer_probe_unlit_route_smoke_disabled.py",
        )
        write_case(ctest_body=missing_ctest)
        expect_case(
            "missing-route-ctest-registration",
            run_verifier(args.inventory_verifier, probe, tools_dir, ctest),
            1,
            "missing CTest route registration: "
            "renderer_probe_unlit_route_smoke.py",
            errors,
        )

        reset_tools_dir()
        (tools_dir / "renderer_probe_unlit_route_contract.py").unlink()
        write_case()
        expect_case(
            "missing-route-negative-contract",
            run_verifier(args.inventory_verifier, probe, tools_dir, ctest),
            1,
            "missing route negative contract for: renderer_probe_unlit_route",
            errors,
        )

        reset_tools_dir()
        (tools_dir / "renderer_probe_unlit_route_smoke.py").unlink()
        write_case()
        expect_case(
            "missing-route-positive-smoke",
            run_verifier(args.inventory_verifier, probe, tools_dir, ctest),
            1,
            "missing route positive smoke for: renderer_probe_unlit_route",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1

    print("renderer_probe_route_inventory_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
