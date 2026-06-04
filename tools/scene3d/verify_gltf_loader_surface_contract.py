#!/usr/bin/env python3
"""Verifies the glTF loader surface checker rejects meaningful drift."""

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


def add_parser_leak(header):
    marker = "struct DracoAttributeIds {\n"
    leak = "namespace fastgltf { struct Asset; }\nfastgltf::Asset* leaked_parser_symbol();\n\n"
    return replace_once(header, marker, leak + marker)


def drift_callback_signature(header):
    return remove_once(header, "    const DracoAttributeIds& attribute_ids")


def remove_pipeline_step(source):
    return remove_once(source, "    load_materials(asset, result.scene, path);\n")


def drift_diagnostic_code(source):
    return replace_once(
        source,
        '"gltf.primitive_missing_indices"',
        '"gltf.primitive_missing_indices_drift"',
    )


def remove_draco_gate(source):
    return remove_once(
        source,
        """    if (!options.allow_draco) {
        append_loader_error(scene,
                            "gltf.draco_disabled",
                            "Asset uses KHR_draco_mesh_compression, but LoadOptions::allow_draco is false.",
                            path);
        return false;
    }
""",
    )


def remove_parser_extension(source):
    return remove_once(
        source,
        "                            fastgltf::Extensions::KHR_draco_mesh_compression);",
    )


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"gltf_loader_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify glTF loader surface checker rejects contract drift.")
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
        header = tmpdir / "gltf_loader.hpp"
        source = tmpdir / "gltf_loader.cpp"

        def write_case(header_body=header_text, source_body=source_text):
            write_text(header, header_body)
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-loader",
            run_verifier(args.surface_verifier, header, source),
            0,
            "gltf_loader_surface_verified=true",
            errors,
        )

        write_case(header_body=add_parser_leak(header_text))
        expect_case(
            "header-parser-leak",
            run_verifier(args.surface_verifier, header, source),
            1,
            "gltf_loader.hpp leaked parser implementation symbols",
            errors,
        )

        write_case(header_body=drift_callback_signature(header_text))
        expect_case(
            "callback-signature-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "DracoDecodeCallback signature no longer matches native boundary",
            errors,
        )

        write_case(source_body=remove_pipeline_step(source_text))
        expect_case(
            "load-pipeline-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "load_gltf_scene pipeline drifted",
            errors,
        )

        write_case(source_body=drift_diagnostic_code(source_text))
        expect_case(
            "diagnostic-code-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "loader diagnostic code sequence mismatch",
            errors,
        )

        write_case(source_body=remove_draco_gate(source_text))
        expect_case(
            "draco-flow-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "Draco decode flow drifted",
            errors,
        )

        write_case(source_body=remove_parser_extension(source_text))
        expect_case(
            "parser-extension-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "parser extension missing from load_gltf_scene",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("gltf_loader_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
