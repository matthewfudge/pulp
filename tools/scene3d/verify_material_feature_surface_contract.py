#!/usr/bin/env python3
"""Verifies the MaterialFeature surface checker rejects meaningful drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, header, source):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--header",
            str(header),
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


def drift_bit_assignment(header):
    return replace_once(header, "normal_scale = 1u << 19u", "normal_scale = 1u << 18u")


def remove_feature(header):
    return remove_once(header, "    color0 = 1u << 15u,\n")


def reorder_features(header):
    return replace_once(
        header,
        """    tangents = 1u << 13u,
    texcoord1 = 1u << 14u,
""",
        """    texcoord1 = 1u << 14u,
    tangents = 1u << 13u,
""",
    )


def drift_feature_name(source):
    return replace_once(source, '"base_color_texture_transform"', '"baseColorTextureTransform"')


def remove_feature_name(source):
    return remove_once(
        source,
        """    append_feature_name(key, MaterialFeature::texcoord1, "texcoord1", names);
""",
    )


def duplicate_feature_name(source):
    marker = '    append_feature_name(key, MaterialFeature::color0, "color0", names);\n'
    return replace_once(source, marker, marker + marker)


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
    print(f"material_feature_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify MaterialFeature surface checker rejects contract drift.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("header", args.header),
            ("source", args.source)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    header_text = read_text(args.header)
    source_text = read_text(args.source)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        header = tmpdir / "material_key.hpp"
        source = tmpdir / "material_key.cpp"

        def write_case(header_body=header_text, source_body=source_text):
            write_text(header, header_body)
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-material-feature",
            run_verifier(args.surface_verifier, header, source),
            0,
            "material_feature_surface_verified=true",
            errors,
        )

        write_case(header_body=drift_bit_assignment(header_text))
        expect_case(
            "bit-assignment-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "MaterialFeature enum mismatch",
            errors,
        )

        write_case(header_body=remove_feature(header_text))
        expect_case(
            "missing-feature",
            run_verifier(args.surface_verifier, header, source),
            1,
            "MaterialFeature enum mismatch",
            errors,
        )

        write_case(header_body=reorder_features(header_text))
        expect_case(
            "feature-order-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "MaterialFeature enum mismatch",
            errors,
        )

        write_case(source_body=drift_feature_name(source_text))
        expect_case(
            "feature-name-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "material_feature_names mismatch",
            errors,
        )

        write_case(source_body=remove_feature_name(source_text))
        expect_case(
            "missing-feature-name",
            run_verifier(args.surface_verifier, header, source),
            1,
            "material_feature_names mismatch",
            errors,
        )

        write_case(source_body=duplicate_feature_name(source_text))
        expect_case(
            "duplicate-feature-name",
            run_verifier(args.surface_verifier, header, source),
            1,
            "material_feature_names mismatch",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("material_feature_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
