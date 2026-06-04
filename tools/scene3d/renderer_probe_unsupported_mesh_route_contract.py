#!/usr/bin/env python3
"""Verifies the unsupported-mesh route smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_unsupported_mesh_route_smoke", path)
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
    print(f"renderer_probe_unsupported_mesh_route_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify unsupported-mesh route smoke drift cases.")
    parser.add_argument("--route-smoke", type=Path, required=True)
    args = parser.parse_args()

    if not args.route_smoke.exists():
        print(f"route_smoke_exists=false path={args.route_smoke}")
        return 2

    smoke_module = load_route_smoke(args.route_smoke)
    base_fields = {
        "scene": "boxtextured",
        "primitive_count": "1",
        "non_transparent_pixel_count": "1200",
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
            "valid-unsupported-mesh-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_unsupported_mesh_route_verified=true",
            errors,
        )

        skinning_lost = dict(base_fields)
        skinning_lost["skinning_deferred"] = "false"
        write_probe(probe, skinning_lost)
        expect_case(
            "skinning-deferral-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "skinning_deferred: expected 'true', got 'false'",
            errors,
        )

        morph_lost = dict(base_fields)
        morph_lost["morph_target_deferred"] = "false"
        write_probe(probe, morph_lost)
        expect_case(
            "morph-deferral-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "morph_target_deferred: expected 'true', got 'false'",
            errors,
        )

        instancing_lost = dict(base_fields)
        instancing_lost["gpu_instancing_deferred"] = "false"
        write_probe(probe, instancing_lost)
        expect_case(
            "gpu-instancing-deferral-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "gpu_instancing_deferred: expected 'true', got 'false'",
            errors,
        )

        animation_leak = dict(base_fields)
        animation_leak["transform_animation_deferred"] = "true"
        write_probe(probe, animation_leak)
        expect_case(
            "animation-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "transform_animation_deferred: expected 'false', got 'true'",
            errors,
        )

        advanced_leak = dict(base_fields)
        advanced_leak["advanced_material_extension_deferred"] = "true"
        write_probe(probe, advanced_leak)
        expect_case(
            "advanced-material-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "advanced_material_extension_deferred: expected 'false', got 'true'",
            errors,
        )

        primitive_drift = dict(base_fields)
        primitive_drift["primitive_count"] = "2"
        write_probe(probe, primitive_drift)
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

    print("renderer_probe_unsupported_mesh_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
