#!/usr/bin/env python3
"""Pins the bake preflight CLI source-level handoff surface."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_ORDERED_SNIPPETS = [
    'std::string_view(argv[1]) == "--help"',
    'std::string_view(argv[1]) == "-h"',
    "return argc == 2 ? 0 : 64;",
    'std::string_view(argv[1]) == "--require-runtime-evidence"',
    "options.require_runtime_evidence = true;",
    'std::string_view(argv[1]) == "--require-runtime-evidence-url"',
    "options.require_runtime_evidence_url = true;",
    "const auto parse_result = pulp::scene::sidecar_from_json(json);",
    "pulp::scene::analyze_bake_preflight(parse_result.sidecar, options);",
    '"bake_readiness="',
    "pulp::scene::bake_preflight_readiness(report)",
    '"export_blocked="',
    '"texture_encoding_blocked="',
    '"native_runtime_has_gaps="',
    '"has_error_diagnostics="',
    '"runtime_evidence_missing="',
    '"runtime_evidence_url_invalid="',
    '"export_blockers="',
    '" texture_encoding_blockers="',
    '" native_runtime_gaps="',
    '" diagnostics="',
    'print_feature_rows("export_blocker", report.export_blockers);',
    'print_feature_rows("texture_encoding_blocker",',
    'print_feature_rows("native_runtime_gap", report.native_runtime_gaps);',
    "return report.export_blocked ? 2 : 0;",
]


EXPECTED_FAILURE_SNIPPETS = [
    '"pulp-scene3d-bake-preflight: "',
    "return 1;",
    "if (!parse_result.success)",
    "parse_result.error",
    "return 1;",
]


EXPECTED_ROW_SNIPPETS = [
    "std::cout << label << \": \" << feature.feature;",
    'if (!feature.node_path.empty())',
    '" node="',
    'if (!feature.reason.empty())',
    '" reason="',
]

EXPECTED_USAGE_SNIPPETS = [
    '"Usage: "',
    '" [--require-runtime-evidence|--require-runtime-evidence-url] "',
    '"<scene.pulp3d.json>\\n"',
]

EXPECTED_READ_FILE_SNIPPETS = [
    "std::ifstream input(path, std::ios::binary);",
    'throw std::runtime_error("could not open file");',
]


def extract_function_body(source, signature):
    start = source.find(signature)
    if start < 0:
        raise ValueError(f"missing function {signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise ValueError(f"missing body for {signature}")
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise ValueError(f"unterminated body for {signature}")


def require_subsequence(text, snippets, label):
    position = 0
    errors = []
    for snippet in snippets:
        next_position = text.find(snippet, position)
        if next_position == -1:
            errors.append(f"{label}: missing ordered snippet {snippet!r}")
        else:
            position = next_position + len(snippet)
    return errors


def verify_no_extra_printed_report_fields(main_body):
    keys = re.findall(r'std::cout\s*<<\s*"([A-Za-z0-9_]+)=', main_body)
    expected = [
        "bake_readiness",
        "export_blocked",
        "texture_encoding_blocked",
        "native_runtime_has_gaps",
        "has_error_diagnostics",
        "runtime_evidence_missing",
        "runtime_evidence_url_invalid",
        "export_blockers",
    ]
    if keys != expected:
        return [
            f"preflight printed field order drifted: expected {expected!r}, got {keys!r}"
        ]
    return []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    source = args.source.read_text(encoding="utf-8")
    main_body = extract_function_body(source, "int main(int argc, char** argv)")
    usage_body = extract_function_body(source, "void print_usage(const char* argv0)")
    read_file_body = extract_function_body(
        source,
        "std::string read_file(const std::filesystem::path& path)",
    )
    row_body = extract_function_body(
        source,
        "void print_feature_rows(const char* label,",
    )

    errors = []
    errors.extend(require_subsequence(usage_body, EXPECTED_USAGE_SNIPPETS, "usage"))
    errors.extend(require_subsequence(read_file_body, EXPECTED_READ_FILE_SNIPPETS, "read_file"))
    errors.extend(require_subsequence(main_body, EXPECTED_ORDERED_SNIPPETS, "main"))
    errors.extend(require_subsequence(main_body, EXPECTED_FAILURE_SNIPPETS, "main"))
    errors.extend(require_subsequence(row_body, EXPECTED_ROW_SNIPPETS, "feature rows"))
    errors.extend(verify_no_extra_printed_report_fields(main_body))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("bake_preflight_cli_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
