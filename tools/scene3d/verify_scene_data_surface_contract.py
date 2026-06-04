#!/usr/bin/env python3
"""Verifies the SceneData surface checker rejects meaningful drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, header):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--header",
            str(header),
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


def remove_primitive_tangents(header):
    return remove_once(header, "    std::vector<float> tangents;\n")


def reorder_scene_tables(header):
    return replace_once(
        header,
        """    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
""",
        """    std::vector<MaterialData> materials;
    std::vector<MeshData> meshes;
""",
    )


def drift_validation_code(header):
    return replace_once(
        header,
        '"scene.primitive_missing_indices"',
        '"scene.primitive_missing_indices_drift"',
    )


def loosen_empty_contract(header):
    return replace_once(
        header,
        "return nodes.empty() && meshes.empty();",
        "return nodes.empty();",
    )


def loosen_index_contract(header):
    return replace_once(
        header,
        "return index != invalid_scene_index && static_cast<size_t>(index) < count;",
        "return static_cast<size_t>(index) < count;",
    )


def drop_error_severity_name(header):
    return remove_once(
        header,
        '        case Diagnostic::Severity::error: return "error";\n',
    )


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
    print(f"scene_data_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify SceneData surface checker rejects contract drift.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("header", args.header)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    header_text = read_text(args.header)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        header = Path(tmp) / "scene_data.hpp"

        def write_case(header_body=header_text):
            write_text(header, header_body)

        write_case()
        expect_case(
            "valid-current-scene-data",
            run_verifier(args.surface_verifier, header),
            0,
            "scene_data_surface_verified=true",
            errors,
        )

        write_case(remove_primitive_tangents(header_text))
        expect_case(
            "primitive-field-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "PrimitiveData fields mismatch",
            errors,
        )

        write_case(reorder_scene_tables(header_text))
        expect_case(
            "scene-table-order-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "SceneData fields mismatch",
            errors,
        )

        write_case(drift_validation_code(header_text))
        expect_case(
            "validation-code-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "validate_scene_data diagnostic codes mismatch",
            errors,
        )

        write_case(loosen_empty_contract(header_text))
        expect_case(
            "empty-contract-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "SceneData::empty no longer reflects nodes+meshes emptiness",
            errors,
        )

        write_case(loosen_index_contract(header_text))
        expect_case(
            "index-contract-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "is_valid_scene_index contract drifted",
            errors,
        )

        write_case(drop_error_severity_name(header_text))
        expect_case(
            "severity-name-drift",
            run_verifier(args.surface_verifier, header),
            1,
            "diagnostic_severity_name no longer emits error",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("scene_data_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
