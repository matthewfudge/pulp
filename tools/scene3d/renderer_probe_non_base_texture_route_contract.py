#!/usr/bin/env python3
"""Verifies the non-base texture route probe smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_non_base_texture_route_smoke", path)
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
    print(f"renderer_probe_non_base_texture_route_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify non-base texture route smoke drift cases.")
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
            "valid-current-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_non_base_texture_route_verified=true",
            errors,
        )

        missing_transform = dict(base_fields)
        del missing_transform["non_base_color_texture_transform_applied"]
        write_probe(probe, missing_transform)
        expect_case(
            "missing-transform-applied-field",
            run_smoke(args.route_smoke, probe),
            1,
            "non_base_color_texture_transform_applied: expected 'true', got None",
            errors,
        )

        metal_route_lost = dict(base_fields)
        metal_route_lost["metallic_roughness_texture_applied"] = "false"
        write_probe(probe, metal_route_lost)
        expect_case(
            "metallic-roughness-route-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "metallic_roughness_texture_applied: expected 'true', got 'false'",
            errors,
        )

        occlusion_strength_lost = dict(base_fields)
        occlusion_strength_lost["occlusion_strength_applied"] = "false"
        write_probe(probe, occlusion_strength_lost)
        expect_case(
            "occlusion-strength-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "occlusion_strength_applied: expected 'true', got 'false'",
            errors,
        )

        transform_deferred = dict(base_fields)
        transform_deferred["non_base_color_texture_transform_deferred"] = "true"
        write_probe(probe, transform_deferred)
        expect_case(
            "transform-deferral-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "non_base_color_texture_transform_deferred: expected 'false', got 'true'",
            errors,
        )

        texcoord_deferred = dict(base_fields)
        texcoord_deferred["non_base_color_texcoord1_deferred"] = "true"
        write_probe(probe, texcoord_deferred)
        expect_case(
            "texcoord1-deferral-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "non_base_color_texcoord1_deferred: expected 'false', got 'true'",
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

    print("renderer_probe_non_base_texture_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
