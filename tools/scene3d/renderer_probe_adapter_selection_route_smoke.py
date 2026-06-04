#!/usr/bin/env python3
"""Verifies renderer probe adapter-selection telemetry."""

import argparse
import subprocess
import sys
from pathlib import Path


def parse_key_values(text):
    values = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run_probe(probe, extra_args):
    result = subprocess.run(
        [str(probe), *extra_args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    sys.stdout.write(result.stdout)
    return result, parse_key_values(result.stdout)


def verify_null_backend(probe):
    result, values = run_probe(
        probe,
        [
            "--scene",
            "hardcoded",
            "--width",
            "32",
            "--height",
            "32",
            "--adapter-scope",
            "dawn_null_route",
            "--adapter-backend",
            "null",
        ],
    )
    require(result.returncode == 2,
            f"null backend: expected exit 2, got {result.returncode}")
    require(values.get("scene") == "hardcoded",
            f"null backend scene: expected 'hardcoded', got {values.get('scene')!r}")
    require(values.get("success") == "false",
            f"null backend success: expected 'false', got {values.get('success')!r}")
    require(values.get("gpu_available") == "true",
            f"null backend gpu_available: expected 'true', got {values.get('gpu_available')!r}")
    require(values.get("adapter_backend_type") == "Null",
            "null backend adapter_backend_type: expected 'Null', "
            f"got {values.get('adapter_backend_type')!r}")
    require(values.get("adapter_scope") == "dawn_null_route",
            f"null backend adapter_scope: expected 'dawn_null_route', got {values.get('adapter_scope')!r}")
    require(values.get("adapter_backend_preference") == "null",
            "null backend adapter_backend_preference: expected 'null', "
            f"got {values.get('adapter_backend_preference')!r}")
    require(values.get("fallback_adapter_requested") == "false",
            "null backend fallback_adapter_requested: expected 'false', "
            f"got {values.get('fallback_adapter_requested')!r}")
    require(values.get("null_backend_requested") == "true",
            "null backend null_backend_requested: expected 'true', "
            f"got {values.get('null_backend_requested')!r}")
    require(values.get("command_submitted") == "true",
            "null backend command_submitted: expected 'true', "
            f"got {values.get('command_submitted')!r}")
    require(values.get("pixel_output_produced") == "false",
            "null backend pixel_output_produced: expected 'false', "
            f"got {values.get('pixel_output_produced')!r}")
    require(values.get("final_software_golden_eligible") == "false",
            "null backend final_software_golden_eligible: expected 'false', "
            f"got {values.get('final_software_golden_eligible')!r}")
    print("renderer_probe_adapter_selection_route_case=null-backend")


def verify_forced_fallback(probe):
    result, values = run_probe(
        probe,
        [
            "--scene",
            "hardcoded",
            "--width",
            "32",
            "--height",
            "32",
            "--adapter-scope",
            "fallback_adapter_route",
            "--force-fallback-adapter",
        ],
    )
    require(result.returncode == 2,
            f"forced fallback: expected exit 2, got {result.returncode}")
    require(values.get("scene") == "hardcoded",
            "forced fallback scene: expected 'hardcoded', "
            f"got {values.get('scene')!r}")
    require(values.get("success") == "false",
            "forced fallback success: expected 'false', "
            f"got {values.get('success')!r}")
    require(values.get("adapter_scope") == "fallback_adapter_route",
            "forced fallback adapter_scope: expected 'fallback_adapter_route', "
            f"got {values.get('adapter_scope')!r}")
    require(values.get("adapter_backend_preference") == "default",
            "forced fallback adapter_backend_preference: expected 'default', "
            f"got {values.get('adapter_backend_preference')!r}")
    require(values.get("fallback_adapter_requested") == "true",
            "forced fallback fallback_adapter_requested: expected 'true', "
            f"got {values.get('fallback_adapter_requested')!r}")
    require(values.get("null_backend_requested") == "false",
            "forced fallback null_backend_requested: expected 'false', "
            f"got {values.get('null_backend_requested')!r}")
    require(values.get("command_submitted") == "false",
            "forced fallback command_submitted: expected 'false', "
            f"got {values.get('command_submitted')!r}")
    require(values.get("pixel_output_produced") == "false",
            "forced fallback pixel_output_produced: expected 'false', "
            f"got {values.get('pixel_output_produced')!r}")
    require(values.get("final_software_golden_eligible") == "false",
            "forced fallback final_software_golden_eligible: expected 'false', "
            f"got {values.get('final_software_golden_eligible')!r}")
    print("renderer_probe_adapter_selection_route_case=forced-fallback")


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe adapter-selection route telemetry.")
    parser.add_argument("--probe-tool", type=Path, required=True)
    args = parser.parse_args()

    if not args.probe_tool.exists():
        print(f"probe_tool_exists=false path={args.probe_tool}")
        return 2

    verify_null_backend(args.probe_tool)
    verify_forced_fallback(args.probe_tool)
    print("renderer_probe_adapter_selection_route_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
