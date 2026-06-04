#!/usr/bin/env python3
"""Verifies the Renderer3D probe CLI surface checker rejects drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


FAKE_PROBE = r'''#!/usr/bin/env python3
import sys

CASE = "__CASE__"

HELP = """usage:
--scene hardcoded|boxtextured
--fixture path
--output-png path
--adapter-backend default|null
--require-final-software-adapter
--width n
"""


def has_arg(name):
    return name in sys.argv[1:]


def value_after(name, default=None):
    args = sys.argv[1:]
    if name not in args:
        return default
    index = args.index(name)
    if index + 1 >= len(args):
        return default
    return args[index + 1]


def write_output_png_if_requested():
    output_png = value_after("--output-png")
    if output_png is None:
        return
    if CASE == "output-png-missing-file-drift":
        return
    data = (
        b"not a png"
        if CASE == "output-png-magic-drift"
        else bytes([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a])
    )
    with open(output_png, "wb") as handle:
        handle.write(data)


if has_arg("--help"):
    if CASE == "help-option-drift":
        print("usage:\n--scene hardcoded|boxtextured\n--fixture path")
    else:
        print(HELP)
    raise SystemExit(0)

scene = value_after("--scene")
if scene == "live-gltf":
    print(HELP)
    raise SystemExit(0 if CASE == "invalid-scene-exit-drift" else 64)

if value_after("--width") == "0":
    if CASE == "invalid-width-usage-drift":
        print("usage:")
    else:
        print(HELP)
    raise SystemExit(64)

if scene == "boxtextured" and not has_arg("--fixture"):
    if CASE != "boxtextured-fixture-drift":
        print("pulp-renderer3d-probe: --fixture is required for --scene boxtextured")
    else:
        print("pulp-renderer3d-probe: missing asset")
    raise SystemExit(64)

if value_after("--adapter-backend") == "null":
    print("adapter_backend_type=Null")
    print("adapter_backend_preference=null")
    print("null_backend_requested=true")
    print("pixel_output_produced=true" if CASE == "null-backend-pixel-drift"
          else "pixel_output_produced=false")
    print("final_software_golden_eligible=false")
    raise SystemExit(2)

if has_arg("--require-final-software-adapter"):
    print("scene=hardcoded")
    print("success=true")
    print("pixel_output_produced=true")
    print("final_software_golden_eligible=true"
          if CASE == "final-gate-eligibility-drift"
          else "final_software_golden_eligible=false")
    raise SystemExit(0 if CASE == "final-gate-exit-drift" else 3)

if has_arg("--output-png"):
    write_output_png_if_requested()
    print("scene=hardcoded")
    print("success=true")
    print("pixel_output_produced=true")
    raise SystemExit(0)

if scene == "boxtextured":
    print("scene=boxtextured")
    print("success=true")
    if CASE != "boxtextured-consumption-drift":
        print("scene_data_consumed=true")
    print("primitive_count=1")
    print("pixel_output_produced=true")
    raise SystemExit(0)

raise SystemExit(64)
'''


def write_fake_probe(path, case):
    path.write_text(FAKE_PROBE.replace("__CASE__", case), encoding="utf-8")
    path.chmod(0o755)


def write_fixture(path):
    path.write_bytes(b"fake glb")


def run_verifier(verifier, probe, fixture):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--probe-tool",
            str(probe),
            "--fixture",
            str(fixture),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}"
        )
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}"
        )
    print(f"renderer_probe_cli_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify Renderer3D probe CLI verifier drift is rejected.")
    parser.add_argument("--cli-verifier", type=Path, required=True)
    args = parser.parse_args()

    if not args.cli_verifier.exists():
        print(f"cli_verifier_exists=false path={args.cli_verifier}")
        return 2

    cases = [
        ("valid-fake-probe", 0, "renderer_probe_cli_surface_verified=true"),
        ("help-option-drift", 1, "help: expected output containing"),
        ("invalid-scene-exit-drift", 1, "invalid-scene: expected exit 64"),
        ("invalid-width-usage-drift", 1, "invalid-width: expected output containing"),
        ("boxtextured-fixture-drift", 1,
         "boxtextured-requires-fixture: expected output containing"),
        ("null-backend-pixel-drift", 1,
         "null-backend: expected output containing 'pixel_output_produced=false'"),
        ("final-gate-exit-drift", 1, "final-software-gate: expected exit 3"),
        ("final-gate-eligibility-drift", 1,
         "final-software-gate: expected output containing"),
        ("boxtextured-consumption-drift", 1,
         "boxtextured-handoff: expected output containing 'scene_data_consumed=true'"),
        ("output-png-missing-file-drift", 1,
         "hardcoded-output-png: expected PNG output file to exist"),
        ("output-png-magic-drift", 1,
         "hardcoded-output-png: expected PNG output file to have PNG magic"),
    ]
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        temp_root = Path(tmp)
        fixture = temp_root / "fixture.glb"
        write_fixture(fixture)
        for name, expected_code, expected_text in cases:
            probe = temp_root / f"{name}.py"
            write_fake_probe(probe, name)
            expect_case(
                name,
                run_verifier(args.cli_verifier, probe, fixture),
                expected_code,
                expected_text,
                errors,
            )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_cli_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
