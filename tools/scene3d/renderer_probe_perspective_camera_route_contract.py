#!/usr/bin/env python3
"""Verifies the perspective camera probe smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_perspective_camera_route_smoke", path)
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
    print(f"renderer_probe_perspective_camera_route_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify perspective camera route smoke drift cases.")
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
            "valid-perspective-camera-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_perspective_camera_route_verified=true",
            errors,
        )

        perspective_lost = dict(base_fields)
        perspective_lost["perspective_camera_applied"] = "false"
        write_probe(probe, perspective_lost)
        expect_case(
            "perspective-camera-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "perspective_camera_applied: expected 'true', got 'false'",
            errors,
        )

        translation_lost = dict(base_fields)
        translation_lost["camera_node_translation_applied"] = "false"
        write_probe(probe, translation_lost)
        expect_case(
            "camera-translation-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "camera_node_translation_applied: expected 'true', got 'false'",
            errors,
        )

        aspect_lost = dict(base_fields)
        aspect_lost["camera_aspect_ratio_applied"] = "false"
        write_probe(probe, aspect_lost)
        expect_case(
            "camera-aspect-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "camera_aspect_ratio_applied: expected 'true', got 'false'",
            errors,
        )

        depth_lost = dict(base_fields)
        depth_lost["camera_depth_range_applied"] = "false"
        write_probe(probe, depth_lost)
        expect_case(
            "camera-depth-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "camera_depth_range_applied: expected 'true', got 'false'",
            errors,
        )

        orthographic_leak = dict(base_fields)
        orthographic_leak["orthographic_camera_applied"] = "true"
        write_probe(probe, orthographic_leak)
        expect_case(
            "orthographic-camera-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "orthographic_camera_applied: expected 'false', got 'true'",
            errors,
        )

        camera_deferred = dict(base_fields)
        camera_deferred["camera_aspect_ratio_deferred"] = "true"
        write_probe(probe, camera_deferred)
        expect_case(
            "camera-deferred-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "camera_aspect_ratio_deferred: expected 'false', got 'true'",
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

    print("renderer_probe_perspective_camera_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
