#!/usr/bin/env python3
"""Verifies the unlit-material probe smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_unlit_route_smoke", path)
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
    print(f"renderer_probe_unlit_route_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify unlit route smoke drift cases.")
    parser.add_argument("--route-smoke", type=Path, required=True)
    args = parser.parse_args()

    if not args.route_smoke.exists():
        print(f"route_smoke_exists=false path={args.route_smoke}")
        return 2

    smoke_module = load_route_smoke(args.route_smoke)
    base_fields = {
        "scene": "boxtextured",
        "primitive_count": "1",
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
            "valid-unlit-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_unlit_route_verified=true",
            errors,
        )

        unlit_lost = dict(base_fields)
        unlit_lost["unlit_material_applied"] = "false"
        write_probe(probe, unlit_lost)
        expect_case(
            "unlit-route-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "unlit_material_applied: expected 'true', got 'false'",
            errors,
        )

        fallback_lost = dict(base_fields)
        fallback_lost["fallback_texture_used"] = "false"
        write_probe(probe, fallback_lost)
        expect_case(
            "fallback-texture-route-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "fallback_texture_used: expected 'true', got 'false'",
            errors,
        )

        texture_decode_leak = dict(base_fields)
        texture_decode_leak["texture_decoded"] = "true"
        write_probe(probe, texture_decode_leak)
        expect_case(
            "texture-decode-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "texture_decoded: expected 'false', got 'true'",
            errors,
        )

        double_sided_leak = dict(base_fields)
        double_sided_leak["double_sided_material_applied"] = "true"
        write_probe(probe, double_sided_leak)
        expect_case(
            "double-sided-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "double_sided_material_applied: expected 'false', got 'true'",
            errors,
        )

        alpha_blend_leak = dict(base_fields)
        alpha_blend_leak["alpha_blend_applied"] = "true"
        write_probe(probe, alpha_blend_leak)
        expect_case(
            "alpha-blend-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "alpha_blend_applied: expected 'false', got 'true'",
            errors,
        )

        primitive_count_drift = dict(base_fields)
        primitive_count_drift["primitive_count"] = "2"
        write_probe(probe, primitive_count_drift)
        expect_case(
            "primitive-count-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "primitive_count: expected '1', got '2'",
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

    print("renderer_probe_unlit_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
