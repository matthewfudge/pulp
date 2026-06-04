#!/usr/bin/env python3
"""Verifies the bake unsupported-feature surface checker rejects drift."""

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


def remove_enum_value(header):
    return remove_once(header, "    physics,\n")


def reorder_enum_values(header):
    return replace_once(
        header,
        """    physics,
    event_handler,
""",
        """    event_handler,
    physics,
""",
    )


def drift_descriptor_feature(source):
    return replace_once(source, '"ShaderMaterial"', '"Shader"')


def drift_descriptor_code(source):
    return replace_once(
        source,
        '"bake.unsupported_shader_material"',
        '"bake.shader_material"',
    )


def drift_descriptor_reason(source):
    return replace_once(source, "THREE.ShaderMaterial", "ShaderMaterial")


def remove_descriptor_case(source):
    return remove_once(
        source,
        """        case BakeUnsupportedFeature::raw_shader_material:
            return BakeUnsupportedDescriptor{
                "RawShaderMaterial",
                "bake.unsupported_raw_shader_material",
                "THREE.RawShaderMaterial has no portable glTF representation; convert to a supported glTF material or keep this scene Live-only.",
            };
""",
    )


def drift_unsupported_feature_reason(source):
    return replace_once(
        source,
        """        descriptor.reason,
        node_path,
""",
        """        "",
        node_path,
""",
    )


def drift_diagnostic_severity(source):
    return replace_once(
        source,
        "Diagnostic::Severity::warning,",
        "Diagnostic::Severity::error,",
    )


def drift_diagnostic_code(source):
    return replace_once(
        source,
        """                      descriptor.code,
                      descriptor.reason,
""",
        """                      "bake.unsupported",
                      descriptor.reason,
""",
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
    print(f"bake_unsupported_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake unsupported-feature surface drift is rejected.")
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
        header = tmpdir / "sidecar.hpp"
        source = tmpdir / "sidecar.cpp"

        def write_case(header_body=header_text, source_body=source_text):
            write_text(header, header_body)
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-bake-unsupported",
            run_verifier(args.surface_verifier, header, source),
            0,
            "bake_unsupported_surface_verified=true",
            errors,
        )

        write_case(header_body=remove_enum_value(header_text))
        expect_case(
            "missing-enum-value",
            run_verifier(args.surface_verifier, header, source),
            1,
            "BakeUnsupportedFeature enum drifted",
            errors,
        )

        write_case(header_body=reorder_enum_values(header_text))
        expect_case(
            "enum-order-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "BakeUnsupportedFeature enum drifted",
            errors,
        )

        write_case(source_body=drift_descriptor_feature(source_text))
        expect_case(
            "descriptor-feature-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "bake_descriptor cases drifted",
            errors,
        )

        write_case(source_body=drift_descriptor_code(source_text))
        expect_case(
            "descriptor-code-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "bake_descriptor cases drifted",
            errors,
        )

        write_case(source_body=drift_descriptor_reason(source_text))
        expect_case(
            "descriptor-reason-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "bake_descriptor shader_material reason lost",
            errors,
        )

        write_case(source_body=remove_descriptor_case(source_text))
        expect_case(
            "missing-descriptor-case",
            run_verifier(args.surface_verifier, header, source),
            1,
            "bake_descriptor cases drifted",
            errors,
        )

        write_case(source_body=drift_unsupported_feature_reason(source_text))
        expect_case(
            "unsupported-feature-reason-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "append_bake_unsupported_feature: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_diagnostic_severity(source_text))
        expect_case(
            "diagnostic-severity-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "append_bake_unsupported_feature: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_diagnostic_code(source_text))
        expect_case(
            "diagnostic-code-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "append_bake_unsupported_feature: missing ordered snippet",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("bake_unsupported_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
