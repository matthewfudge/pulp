#!/usr/bin/env python3
"""Verifies Renderer3D material-floor probe verifier rejects drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_material_floor_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_material_floor_smoke", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_probe(path, fields):
    lines = [
        "#!/usr/bin/env python3",
        "import sys",
        "print(" + repr(
            "".join(f"{key}={value}\n" for key, value in fields.items())
        ) + ", end='')",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | 0o111)


def run_smoke(smoke, probe, fixture):
    return subprocess.run(
        [
            sys.executable,
            str(smoke),
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
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_material_floor_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify Renderer3D material-floor probe verifier drift cases.")
    parser.add_argument("--material-floor-smoke", type=Path, required=True)
    args = parser.parse_args()

    if not args.material_floor_smoke.exists():
        print(f"material_floor_smoke_exists=false path={args.material_floor_smoke}")
        return 2

    smoke_module = load_material_floor_smoke(args.material_floor_smoke)
    base_fields = dict(smoke_module.EXPECTED_BOXTEXTURED_FIELDS)
    errors = []

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        fixture = temp_path / "BoxTextured.glb"
        fixture.write_bytes(b"synthetic fixture path for verifier contract")
        probe = temp_path / "fake-renderer3d-probe.py"

        write_probe(probe, base_fields)
        expect_case(
            "valid-current-material-floor",
            run_smoke(args.material_floor_smoke, probe, fixture),
            0,
            "renderer_probe_material_floor_verified=boxtextured",
            errors,
        )

        missing_texture_fields = dict(base_fields)
        del missing_texture_fields["base_color_texture_srgb_applied"]
        write_probe(probe, missing_texture_fields)
        expect_case(
            "missing-base-color-srgb-field",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "base_color_texture_srgb_applied: expected 'true', got None",
            errors,
        )

        wrong_texture_upload = dict(base_fields)
        wrong_texture_upload["texture_uploaded"] = "false"
        write_probe(probe, wrong_texture_upload)
        expect_case(
            "texture-upload-drift",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "texture_uploaded: expected 'true', got 'false'",
            errors,
        )

        wrong_scene = dict(base_fields)
        wrong_scene["scene"] = "hardcoded"
        write_probe(probe, wrong_scene)
        expect_case(
            "scene-identity-drift",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "scene: expected 'boxtextured', got 'hardcoded'",
            errors,
        )

        missing_material_factor = dict(base_fields)
        missing_material_factor["metallic_roughness_factor_applied"] = "false"
        write_probe(probe, missing_material_factor)
        expect_case(
            "material-factor-drift",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "metallic_roughness_factor_applied: expected 'true', got 'false'",
            errors,
        )

        missing_normals = dict(base_fields)
        missing_normals["geometry_normals_applied"] = "false"
        write_probe(probe, missing_normals)
        expect_case(
            "geometry-normal-drift",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "geometry_normals_applied: expected 'true', got 'false'",
            errors,
        )

        no_pixels = dict(base_fields)
        no_pixels["pixel_output_produced"] = "false"
        write_probe(probe, no_pixels)
        expect_case(
            "pixel-output-drift",
            run_smoke(args.material_floor_smoke, probe, fixture),
            1,
            "pixel_output_produced: expected 'true', got 'false'",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_material_floor_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
