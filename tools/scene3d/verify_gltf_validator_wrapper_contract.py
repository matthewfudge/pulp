#!/usr/bin/env python3
"""Verifies the glTF validator wrapper contract rejects wrapper drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, wrapper):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--validator-wrapper",
            str(wrapper),
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


def drift_missing_validator_skip_code(text):
    return replace_once(text, "SKIP_MISSING_VALIDATOR = 77", "SKIP_MISSING_VALIDATOR = 0")


def drift_require_validator_exit(text):
    return replace_once(
        text,
        "return 2 if args.require_validator else SKIP_MISSING_VALIDATOR",
        "return SKIP_MISSING_VALIDATOR",
    )


def drift_cli_output_flag(text):
    return replace_once(
        text,
        """        [*command, "-o", str(asset)],
""",
        """        [*command, str(asset)],
""",
    )


def drift_success_summary_label(text):
    return replace_once(
        text,
        """        f"{asset}: glTF-Validator errors={num_errors} "
""",
        """        f"{asset}: validator errors={num_errors} "
""",
    )


def drift_error_report_exit(text):
    return replace_once(
        text,
        """    if result.returncode != 0 or num_errors != 0:
        return result.returncode if result.returncode != 0 else 1
""",
        """    if result.returncode != 0:
        return result.returncode
""",
    )


def drift_invalid_json_exit(text):
    return replace_once(
        text,
        """    except json.JSONDecodeError as exc:
        print(f"validator did not write a JSON report to stdout: {exc}", file=sys.stderr)
        if result.stdout.strip():
            print(result.stdout.strip(), file=sys.stderr)
        return 2
""",
        """    except json.JSONDecodeError as exc:
        print(f"validator did not write a JSON report to stdout: {exc}", file=sys.stderr)
        if result.stdout.strip():
            print(result.stdout.strip(), file=sys.stderr)
        return 0
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
    print(f"gltf_validator_wrapper_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify validate_gltf.py wrapper behavior drift is rejected.")
    parser.add_argument("--wrapper-verifier", type=Path, required=True)
    parser.add_argument("--validator-wrapper", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("wrapper_verifier", args.wrapper_verifier),
            ("validator_wrapper", args.validator_wrapper)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    wrapper_text = read_text(args.validator_wrapper)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        wrapper = Path(tmp) / "validate_gltf.py"

        def write_case(body=wrapper_text):
            write_text(wrapper, body)

        write_case()
        expect_case(
            "valid-current-wrapper",
            run_verifier(args.wrapper_verifier, wrapper),
            0,
            "gltf_validator_wrapper_verified=true",
            errors,
        )

        write_case(drift_missing_validator_skip_code(wrapper_text))
        expect_case(
            "missing-validator-skip-code-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "missing validator default returned 0",
            errors,
        )

        write_case(drift_require_validator_exit(wrapper_text))
        expect_case(
            "require-validator-exit-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "missing validator required returned 77",
            errors,
        )

        write_case(drift_cli_output_flag(wrapper_text))
        expect_case(
            "cli-output-flag-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "configured validator success returned 2",
            errors,
        )

        write_case(drift_success_summary_label(wrapper_text))
        expect_case(
            "success-summary-label-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "configured validator success: missing",
            errors,
        )

        write_case(drift_error_report_exit(wrapper_text))
        expect_case(
            "error-report-exit-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "validator error report returned 0",
            errors,
        )

        write_case(drift_invalid_json_exit(wrapper_text))
        expect_case(
            "invalid-json-exit-drift",
            run_verifier(args.wrapper_verifier, wrapper),
            1,
            "invalid JSON returned 0",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("gltf_validator_wrapper_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
