#!/usr/bin/env python3
"""Verifies the Renderer3D probe CLI handoff surface."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def run_command(args):
    return subprocess.run(
        [str(arg) for arg in args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_case(name, result, expected_code, expected_fragments, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    for fragment in expected_fragments:
        if fragment not in result.stdout:
            errors.append(
                f"{name}: expected output containing {fragment!r}")
    print(f"renderer_probe_cli_surface_case={name}")


def expect_png(path, name, errors):
    if not path.exists():
        errors.append(f"{name}: expected PNG output file to exist")
        return
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        errors.append(f"{name}: expected PNG output file to have PNG magic")


def main():
    parser = argparse.ArgumentParser(
        description="Verify Renderer3D probe CLI usage, gates, and exits.")
    parser.add_argument("--probe-tool", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()

    if not args.probe_tool.exists():
        print(f"probe_tool_exists=false path={args.probe_tool}")
        return 2
    if not args.fixture.exists():
        print(f"fixture_exists=false path={args.fixture}")
        return 2

    probe = args.probe_tool
    errors = []

    expect_case(
        "help",
        run_command([probe, "--help"]),
        0,
        [
            "usage:",
            "--scene hardcoded|boxtextured",
            "--fixture path",
            "--output-png path",
            "--adapter-backend default|null",
            "--require-final-software-adapter",
        ],
        errors,
    )

    expect_case(
        "invalid-scene",
        run_command([probe, "--scene", "live-gltf"]),
        64,
        ["usage:", "--scene hardcoded|boxtextured"],
        errors,
    )

    expect_case(
        "invalid-width",
        run_command([probe, "--scene", "hardcoded", "--width", "0"]),
        64,
        ["usage:", "--width n"],
        errors,
    )

    expect_case(
        "boxtextured-requires-fixture",
        run_command([probe, "--scene", "boxtextured"]),
        64,
        ["pulp-renderer3d-probe: --fixture is required for --scene boxtextured"],
        errors,
    )

    expect_case(
        "null-backend",
        run_command([
            probe,
            "--scene",
            "hardcoded",
            "--width",
            "32",
            "--height",
            "32",
            "--adapter-scope",
            "dawn_null_api",
            "--adapter-backend",
            "null",
        ]),
        2,
        [
            "adapter_backend_type=Null",
            "adapter_backend_preference=null",
            "null_backend_requested=true",
            "pixel_output_produced=false",
            "final_software_golden_eligible=false",
        ],
        errors,
    )

    expect_case(
        "final-software-gate",
        run_command([
            probe,
            "--scene",
            "hardcoded",
            "--width",
            "128",
            "--height",
            "128",
            "--adapter-scope",
            "macos_default_metal",
            "--require-final-software-adapter",
        ]),
        3,
        [
            "scene=hardcoded",
            "success=true",
            "pixel_output_produced=true",
            "final_software_golden_eligible=false",
        ],
        errors,
    )

    with tempfile.TemporaryDirectory(prefix="pulp-renderer3d-probe-") as tmp:
        output_png = Path(tmp) / "hardcoded.png"
        expect_case(
            "hardcoded-output-png",
            run_command([
                probe,
                "--scene",
                "hardcoded",
                "--width",
                "64",
                "--height",
                "64",
                "--adapter-scope",
                "macos_default_metal",
                "--output-png",
                output_png,
            ]),
            0,
            [
                "scene=hardcoded",
                "success=true",
                "pixel_output_produced=true",
            ],
            errors,
        )
        expect_png(output_png, "hardcoded-output-png", errors)

    expect_case(
        "boxtextured-handoff",
        run_command([
            probe,
            "--scene",
            "boxtextured",
            "--fixture",
            args.fixture,
            "--width",
            "128",
            "--height",
            "128",
            "--adapter-scope",
            "macos_default_metal",
        ]),
        0,
        [
            "scene=boxtextured",
            "success=true",
            "scene_data_consumed=true",
            "primitive_count=1",
            "pixel_output_produced=true",
        ],
        errors,
    )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_cli_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
