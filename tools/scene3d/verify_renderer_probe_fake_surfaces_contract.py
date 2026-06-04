#!/usr/bin/env python3
"""Verifies fake Renderer3D probe surface coverage rejects drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, probe_verifier, probe_contract, final_contract):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--probe-verifier",
            str(probe_verifier),
            "--probe-contract",
            str(probe_contract),
            "--final-eligibility-contract",
            str(final_contract),
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


def remove_probe_contract_line(text):
    return replace_once(text, '    "texture_uploaded=true",\n', "")


def remove_final_true_line(text):
    return replace_once(text, '        "texture_uploaded=true",\n', "")


def remove_final_eligibility_line(text):
    return replace_once(
        text,
        '        f"final_software_golden_eligible={final_value}",\n',
        "",
    )


def add_probe_field(text):
    return replace_once(
        text,
        """    "final_software_golden_eligible",
}
""",
        """    "final_software_golden_eligible",
    "future_probe_field",
}
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
    print(f"renderer_probe_fake_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify fake Renderer3D probe surface coverage drift is rejected.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--probe-contract", type=Path, required=True)
    parser.add_argument("--final-eligibility-contract", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("probe_verifier", args.probe_verifier),
            ("probe_contract", args.probe_contract),
            ("final_eligibility_contract", args.final_eligibility_contract)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    probe_text = read_text(args.probe_verifier)
    probe_contract_text = read_text(args.probe_contract)
    final_text = read_text(args.final_eligibility_contract)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        probe = tmpdir / "verify_renderer_probe.py"
        probe_contract = tmpdir / "verify_renderer_probe_contract.py"
        final_contract = tmpdir / "verify_renderer_probe_final_eligibility.py"

        def write_case(probe_body=probe_text,
                       probe_contract_body=probe_contract_text,
                       final_body=final_text):
            write_text(probe, probe_body)
            write_text(probe_contract, probe_contract_body)
            write_text(final_contract, final_body)

        write_case()
        expect_case(
            "valid-current-fake-surfaces",
            run_verifier(args.surface_verifier, probe, probe_contract, final_contract),
            0,
            "renderer_probe_fake_surfaces_verified=true",
            errors,
        )

        write_case(probe_contract_body=remove_probe_contract_line(probe_contract_text))
        expect_case(
            "probe-contract-missing-field",
            run_verifier(args.surface_verifier, probe, probe_contract, final_contract),
            1,
            "probe-contract-lines: expected fields",
            errors,
        )

        write_case(final_body=remove_final_true_line(final_text))
        expect_case(
            "final-eligibility-missing-resource-field",
            run_verifier(args.surface_verifier, probe, probe_contract, final_contract),
            1,
            "final-eligibility-true: expected fields",
            errors,
        )

        write_case(final_body=remove_final_eligibility_line(final_text))
        expect_case(
            "final-eligibility-missing-final-field",
            run_verifier(args.surface_verifier, probe, probe_contract, final_contract),
            1,
            "final-eligibility-true: expected fields",
            errors,
        )

        write_case(probe_body=add_probe_field(probe_text))
        expect_case(
            "probe-field-added-without-fakes",
            run_verifier(args.surface_verifier, probe, probe_contract, final_contract),
            1,
            "probe-contract-lines: expected fields",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_fake_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
