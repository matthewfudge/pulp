#!/usr/bin/env python3
"""Verifies the SceneStats surface checker rejects meaningful drift."""

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


def remove_texture_bytes_field(header):
    return remove_once(header, "    size_t texture_bytes = 0;\n")


def reorder_stats_fields(header):
    return replace_once(
        header,
        """    size_t vertices = 0;
    size_t indices = 0;
""",
        """    size_t indices = 0;
    size_t vertices = 0;
""",
    )


def drift_text_key(source):
    return replace_once(source, '" roots="', '" root_nodes="')


def remove_primitive_count(source):
    return remove_once(source, "        stats.primitives += mesh.primitives.size();\n")


def remove_vertex_count(source):
    return remove_once(source, "            stats.vertices += primitive.positions.size() / 3u;\n")


def remove_texture_byte_count(source):
    return remove_once(source, "        stats.texture_bytes += texture.encoded_bytes.size();\n")


def remove_advanced_extension_count(source):
    return remove_once(
        source,
        """        stats.advanced_material_extensions +=
            material.advanced_material_extensions.size();
""",
    )


def remove_error_diagnostic_count(source):
    return remove_once(source, "            ++stats.error_diagnostics;\n")


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
    print(f"scene_stats_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify SceneStats surface checker rejects contract drift.")
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
        header = tmpdir / "scene_stats.hpp"
        source = tmpdir / "scene_stats.cpp"

        def write_case(header_body=header_text, source_body=source_text):
            write_text(header, header_body)
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-stats",
            run_verifier(args.surface_verifier, header, source),
            0,
            "scene_stats_surface_verified=true",
            errors,
        )

        write_case(header_body=remove_texture_bytes_field(header_text))
        expect_case(
            "missing-field",
            run_verifier(args.surface_verifier, header, source),
            1,
            "SceneStats fields mismatch",
            errors,
        )

        write_case(header_body=reorder_stats_fields(header_text))
        expect_case(
            "field-order-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "SceneStats fields mismatch",
            errors,
        )

        write_case(source_body=drift_text_key(source_text))
        expect_case(
            "text-key-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_stats_to_text keys mismatch",
            errors,
        )

        write_case(source_body=remove_primitive_count(source_text))
        expect_case(
            "primitive-count-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing ordered SceneStats snippet",
            errors,
        )

        write_case(source_body=remove_vertex_count(source_text))
        expect_case(
            "vertex-count-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing ordered SceneStats snippet",
            errors,
        )

        write_case(source_body=remove_texture_byte_count(source_text))
        expect_case(
            "texture-byte-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing ordered SceneStats snippet",
            errors,
        )

        write_case(source_body=remove_advanced_extension_count(source_text))
        expect_case(
            "advanced-extension-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing ordered SceneStats snippet",
            errors,
        )

        write_case(source_body=remove_error_diagnostic_count(source_text))
        expect_case(
            "error-diagnostic-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing ordered SceneStats snippet",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("scene_stats_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
