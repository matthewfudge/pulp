#!/usr/bin/env python3
"""Verifies the bake preflight classification checker rejects bucket drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, source):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--source",
            str(source),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def replace_once(text, old, new):
    if old not in text:
        raise ValueError(f"missing text to replace: {old!r}")
    return text.replace(old, new, 1)


def remove_once(text, snippet):
    return replace_once(text, snippet, "")


def remove_export_blocker(source):
    return remove_once(source, '           feature == "RawShaderMaterial" ||\n')


def add_export_blocker(source):
    return replace_once(
        source,
        '           feature == "EventHandler";',
        '           feature == "EventHandler" ||\n'
        '           feature == "OrbitControls";',
    )


def move_texture_encoding_to_runtime_gap(source):
    source = replace_once(
        source,
        '    return feature == "TextureEncoding";',
        '    return false;',
    )
    return replace_once(
        source,
        '    return feature == "TexturePayload" ||',
        '    return feature == "TexturePayload" ||\n'
        '           feature == "TextureEncoding" ||',
    )


def remove_native_gap(source):
    return remove_once(source, '           feature == "GpuInstancing";\n')


def add_native_gap(source):
    return replace_once(
        source,
        '           feature == "GpuInstancing";',
        '           feature == "GpuInstancing" ||\n'
        '           feature == "EnvironmentMap";',
    )


def remove_material_extension_prefix(source):
    return remove_once(source, '           has_prefix(feature, "MaterialExtension:") ||\n')


def add_export_prefix(source):
    return replace_once(
        source,
        '           feature == "EventHandler";',
        '           feature == "EventHandler" ||\n'
        '           has_prefix(feature, "Shader:");',
    )


def rename_camera_prefix(source):
    return replace_once(source, '"Camera:"', '"CameraNode:"')


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
    print(f"bake_preflight_classification_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake preflight classification surface drift is rejected.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("source", args.source)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    source_text = read_text(args.source)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "bake_preflight.cpp"

        def write_case(source_body=source_text):
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-classification",
            run_verifier(args.surface_verifier, source),
            0,
            "bake_preflight_classification_surface_verified=true",
            errors,
        )

        write_case(source_body=remove_export_blocker(source_text))
        expect_case(
            "missing-export-blocker",
            run_verifier(args.surface_verifier, source),
            1,
            "is_export_blocker exact mismatch",
            errors,
        )

        write_case(source_body=add_export_blocker(source_text))
        expect_case(
            "extra-export-blocker",
            run_verifier(args.surface_verifier, source),
            1,
            "is_export_blocker exact mismatch",
            errors,
        )

        write_case(source_body=move_texture_encoding_to_runtime_gap(source_text))
        expect_case(
            "texture-encoding-bucket-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "is_texture_encoding_blocker exact mismatch",
            errors,
        )

        write_case(source_body=remove_native_gap(source_text))
        expect_case(
            "missing-native-gap",
            run_verifier(args.surface_verifier, source),
            1,
            "is_native_runtime_gap exact mismatch",
            errors,
        )

        write_case(source_body=add_native_gap(source_text))
        expect_case(
            "extra-native-gap",
            run_verifier(args.surface_verifier, source),
            1,
            "is_native_runtime_gap exact mismatch",
            errors,
        )

        write_case(source_body=remove_material_extension_prefix(source_text))
        expect_case(
            "missing-native-prefix",
            run_verifier(args.surface_verifier, source),
            1,
            "is_native_runtime_gap prefix mismatch",
            errors,
        )

        write_case(source_body=add_export_prefix(source_text))
        expect_case(
            "extra-export-prefix",
            run_verifier(args.surface_verifier, source),
            1,
            "is_export_blocker prefix mismatch",
            errors,
        )

        write_case(source_body=rename_camera_prefix(source_text))
        expect_case(
            "renamed-native-prefix",
            run_verifier(args.surface_verifier, source),
            1,
            "is_native_runtime_gap prefix mismatch",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("bake_preflight_classification_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
