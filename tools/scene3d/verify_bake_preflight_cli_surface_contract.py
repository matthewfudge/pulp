#!/usr/bin/env python3
"""Verifies the bake preflight CLI surface checker rejects drift."""

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


def drift_usage_option(source):
    return replace_once(
        source,
        '" [--require-runtime-evidence|--require-runtime-evidence-url] "',
        '" <scene.pulp3d.json>\\n"',
    )


def drift_read_file_failure(source):
    return replace_once(
        source,
        'throw std::runtime_error("could not open file");',
        'throw std::runtime_error("open failed");',
    )


def remove_require_runtime_evidence_flag(source):
    return replace_once(
        source,
        'std::string_view(argv[1]) == "--require-runtime-evidence"',
        'std::string_view(argv[1]) == "--require-runtime-evidence-disabled"',
    )

def remove_require_runtime_evidence_url_flag(source):
    return replace_once(
        source,
        'std::string_view(argv[1]) == "--require-runtime-evidence-url"',
        'std::string_view(argv[1]) == "--require-runtime-evidence-url-disabled"',
    )


def bypass_sidecar_parser(source):
    return replace_once(
        source,
        "const auto parse_result = pulp::scene::sidecar_from_json(json);",
        "const auto parse_result = pulp::scene::SidecarParseResult{};",
    )


def drift_readiness_key(source):
    return replace_once(source, '"bake_readiness="', '"readiness="')


def remove_runtime_evidence_field(source):
    return remove_once(
        source,
        """    std::cout << "runtime_evidence_missing="
              << bool_text(report.runtime_evidence_missing) << "\\n";
""",
    )

def remove_runtime_evidence_url_field(source):
    return remove_once(
        source,
        """    std::cout << "runtime_evidence_url_invalid="
              << bool_text(report.runtime_evidence_url_invalid) << "\\n";
""",
    )


def add_summary_field(source):
    return replace_once(
        source,
        """    std::cout << "export_blockers=" << report.export_blockers.size()
""",
        """    std::cout << "readiness_code=0\\n";
    std::cout << "export_blockers=" << report.export_blockers.size()
""",
    )


def remove_feature_reason(source):
    return remove_once(
        source,
        """        if (!feature.reason.empty()) {
            std::cout << " reason=" << feature.reason;
        }
""",
    )


def drift_exit_code(source):
    return replace_once(
        source,
        "return report.export_blocked ? 2 : 0;",
        "return report.export_blocked ? 1 : 0;",
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
    print(f"bake_preflight_cli_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake preflight CLI source surface drift is rejected.")
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
        source = Path(tmp) / "scene3d_bake_preflight.cpp"

        def write_case(source_body=source_text):
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-cli",
            run_verifier(args.surface_verifier, source),
            0,
            "bake_preflight_cli_surface_verified=true",
            errors,
        )

        write_case(source_body=drift_usage_option(source_text))
        expect_case(
            "usage-option-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "usage: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_read_file_failure(source_text))
        expect_case(
            "read-file-error-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "read_file: missing ordered snippet",
            errors,
        )

        write_case(source_body=remove_require_runtime_evidence_flag(source_text))
        expect_case(
            "runtime-evidence-flag-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=remove_require_runtime_evidence_url_flag(source_text))
        expect_case(
            "runtime-evidence-url-flag-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=bypass_sidecar_parser(source_text))
        expect_case(
            "sidecar-parser-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_readiness_key(source_text))
        expect_case(
            "readiness-key-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=remove_runtime_evidence_field(source_text))
        expect_case(
            "missing-report-field",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=remove_runtime_evidence_url_field(source_text))
        expect_case(
            "missing-url-report-field",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

        write_case(source_body=add_summary_field(source_text))
        expect_case(
            "extra-report-field",
            run_verifier(args.surface_verifier, source),
            1,
            "preflight printed field order drifted",
            errors,
        )

        write_case(source_body=remove_feature_reason(source_text))
        expect_case(
            "feature-row-reason-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "feature rows: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_exit_code(source_text))
        expect_case(
            "blocked-exit-code-drift",
            run_verifier(args.surface_verifier, source),
            1,
            "main: missing ordered snippet",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("bake_preflight_cli_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
