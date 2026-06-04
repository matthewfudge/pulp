#!/usr/bin/env python3
"""Verifies the alpha-blend sort probe smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_alpha_blend_sort_route_smoke", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_probe(path, fields):
    output = "".join(f"{key}={value}\n" for key, value in fields.items())
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "import sys",
            "print(" + repr(output) + ", end='')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(path.stat().st_mode | 0o111)


def run_smoke(smoke, probe):
    return subprocess.run(
        [
            sys.executable,
            str(smoke),
            "--probe-tool",
            str(probe),
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
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_alpha_blend_sort_route_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify alpha-blend sort route smoke drift cases.")
    parser.add_argument("--route-smoke", type=Path, required=True)
    args = parser.parse_args()

    if not args.route_smoke.exists():
        print(f"route_smoke_exists=false path={args.route_smoke}")
        return 2

    smoke_module = load_route_smoke(args.route_smoke)
    base_fields = {
        "scene": "boxtextured",
        "primitive_count": "2",
        "non_transparent_pixel_count": "2400",
    }
    for key in smoke_module.EXPECTED_TRUE_FIELDS:
        base_fields[key] = "true"
    for key in smoke_module.EXPECTED_FALSE_FIELDS:
        base_fields[key] = "false"

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        probe = Path(temp_dir) / "fake-renderer3d-probe.py"

        write_probe(probe, base_fields)
        expect_case(
            "valid-alpha-blend-sort-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_alpha_blend_sort_route_verified=true",
            errors,
        )

        primitive_count_drift = dict(base_fields)
        primitive_count_drift["primitive_count"] = "1"
        write_probe(probe, primitive_count_drift)
        expect_case(
            "primitive-count-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "primitive_count: expected '2', got '1'",
            errors,
        )

        blend_lost = dict(base_fields)
        blend_lost["alpha_blend_applied"] = "false"
        write_probe(probe, blend_lost)
        expect_case(
            "alpha-blend-route-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "alpha_blend_applied: expected 'true', got 'false'",
            errors,
        )

        depth_write_lost = dict(base_fields)
        depth_write_lost["alpha_blend_depth_write_disabled"] = "false"
        write_probe(probe, depth_write_lost)
        expect_case(
            "alpha-depth-write-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "alpha_blend_depth_write_disabled: expected 'true', got 'false'",
            errors,
        )

        sort_lost = dict(base_fields)
        sort_lost["alpha_blend_sorted"] = "false"
        write_probe(probe, sort_lost)
        expect_case(
            "alpha-sort-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "alpha_blend_sorted: expected 'true', got 'false'",
            errors,
        )

        alpha_mask_leak = dict(base_fields)
        alpha_mask_leak["alpha_mask_applied"] = "true"
        write_probe(probe, alpha_mask_leak)
        expect_case(
            "alpha-mask-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "alpha_mask_applied: expected 'false', got 'true'",
            errors,
        )

        no_pixels = dict(base_fields)
        no_pixels["non_transparent_pixel_count"] = "0"
        write_probe(probe, no_pixels)
        expect_case(
            "pixel-output-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "non_transparent_pixel_count must be positive",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_alpha_blend_sort_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
