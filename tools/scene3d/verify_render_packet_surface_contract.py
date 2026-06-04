#!/usr/bin/env python3
"""Verifies the RenderPacket surface checker rejects meaningful drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, header, source, inspect_source):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--header",
            str(header),
            "--source",
            str(source),
            "--inspect-source",
            str(inspect_source),
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


def remove_primitive_material_key(header):
    return remove_once(header, "    MaterialKey material_key;\n")


def reorder_packet_fields(header):
    return replace_once(
        header,
        """    std::vector<RenderPrimitive> primitives;
    std::vector<Diagnostic> diagnostics;
""",
        """    std::vector<Diagnostic> diagnostics;
    std::vector<RenderPrimitive> primitives;
""",
    )


def drift_has_errors(header):
    return replace_once(
        header,
        "return has_error_diagnostics(diagnostics);",
        "return !diagnostics.empty();",
    )


def remove_validation_return(source):
    return remove_once(
        source,
        """    if (has_error_diagnostics(packet.diagnostics)) {
        return packet;
    }

""",
    )


def drift_empty_packet_code(source):
    return replace_once(
        source,
        '"scene.render_packet_empty"',
        '"scene.empty_render_packet"',
    )


def drift_cli_feature_mask(inspect_source):
    return replace_once(inspect_source, '" feature_mask="', '" mask="')


def drift_cli_feature_names(inspect_source):
    return replace_once(
        inspect_source,
        "join_feature_names(primitive.material_key)",
        '"none"',
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
    print(f"render_packet_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify RenderPacket surface checker rejects contract drift.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inspect-source", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("header", args.header),
            ("source", args.source),
            ("inspect_source", args.inspect_source)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    header_text = read_text(args.header)
    source_text = read_text(args.source)
    inspect_source_text = read_text(args.inspect_source)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        header = tmpdir / "render_packet.hpp"
        source = tmpdir / "render_packet.cpp"
        inspect_source = tmpdir / "scene3d_inspect.cpp"

        def write_case(header_body=header_text,
                       source_body=source_text,
                       inspect_source_body=inspect_source_text):
            write_text(header, header_body)
            write_text(source, source_body)
            write_text(inspect_source, inspect_source_body)

        write_case()
        expect_case(
            "valid-current-render-packet",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            0,
            "render_packet_surface_verified=true",
            errors,
        )

        write_case(header_body=remove_primitive_material_key(header_text))
        expect_case(
            "missing-primitive-field",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "RenderPrimitive fields mismatch",
            errors,
        )

        write_case(header_body=reorder_packet_fields(header_text))
        expect_case(
            "packet-field-order-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "RenderPacket fields mismatch",
            errors,
        )

        write_case(header_body=drift_has_errors(header_text))
        expect_case(
            "has-errors-delegation-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "RenderPacket::has_errors no longer delegates to diagnostics",
            errors,
        )

        write_case(source_body=remove_validation_return(source_text))
        expect_case(
            "validation-flow-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "build_render_packet: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_empty_packet_code(source_text))
        expect_case(
            "empty-packet-diagnostic-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "build_render_packet: missing ordered snippet",
            errors,
        )

        write_case(inspect_source_body=drift_cli_feature_mask(inspect_source_text))
        expect_case(
            "cli-feature-mask-key-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "scene3d_inspect render-packet CLI: missing ordered snippet",
            errors,
        )

        write_case(inspect_source_body=drift_cli_feature_names(inspect_source_text))
        expect_case(
            "cli-feature-name-handoff-drift",
            run_verifier(args.surface_verifier, header, source, inspect_source),
            1,
            "scene3d_inspect render-packet CLI: missing ordered snippet",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("render_packet_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
