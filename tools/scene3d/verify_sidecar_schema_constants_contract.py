#!/usr/bin/env python3
"""Verifies the sidecar schema-constant alignment checker rejects drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, canonical, preflight_matrix, boxtextured_sidecar):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--canonical",
            str(canonical),
            "--preflight-matrix",
            str(preflight_matrix),
            "--boxtextured-sidecar",
            str(boxtextured_sidecar),
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


def remove_runtime_hints_from_root(text):
    return remove_once(text, '    "runtime_hints",\n')


def add_asset_to_preflight_root(text):
    return replace_once(
        text,
        """    "runtime_hints",
}
""",
        """    "runtime_hints",
    "asset",
}
""",
    )


def drift_preflight_diagnostic_path(text):
    return replace_once(text, '    "path",\n', '    "source_path",\n')


def drift_boxtextured_provenance_key(text):
    return replace_once(text, '    "runtime_evidence",\n', '    "runtimeEvidence",\n')


def remove_preflight_diagnostic_keys(text):
    return remove_once(
        text,
        """DIAGNOSTIC_KEYS = {
    "severity",
    "code",
    "message",
    "path",
}

""",
    )


def remove_boxtextured_root_keys(text):
    return remove_once(
        text,
        """ROOT_KEYS = {
    "schema_version",
    "provenance",
    "diagnostics",
    "unsupported_features",
    "runtime_hints",
}

""",
    )


def remove_canonical_runtime_hint_keys(text):
    return remove_once(
        text,
        """RUNTIME_HINT_KEYS = {
    "key",
    "value",
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
    print(f"sidecar_schema_constants_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify sidecar schema-constant alignment drift is rejected.")
    parser.add_argument("--schema-verifier", type=Path, required=True)
    parser.add_argument("--canonical", type=Path, required=True)
    parser.add_argument("--preflight-matrix", type=Path, required=True)
    parser.add_argument("--boxtextured-sidecar", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("schema_verifier", args.schema_verifier),
            ("canonical", args.canonical),
            ("preflight_matrix", args.preflight_matrix),
            ("boxtextured_sidecar", args.boxtextured_sidecar)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    canonical_text = read_text(args.canonical)
    preflight_text = read_text(args.preflight_matrix)
    boxtextured_text = read_text(args.boxtextured_sidecar)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        canonical = tmpdir / "verify_bake_artifact.py"
        preflight = tmpdir / "verify_bake_preflight_matrix.py"
        boxtextured = tmpdir / "verify_scene3d_sidecar.py"

        def write_case(canonical_body=canonical_text,
                       preflight_body=preflight_text,
                       boxtextured_body=boxtextured_text):
            write_text(canonical, canonical_body)
            write_text(preflight, preflight_body)
            write_text(boxtextured, boxtextured_body)

        write_case()
        expect_case(
            "valid-current-schema-constants",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            0,
            "sidecar_schema_constants_verified=true",
            errors,
        )

        write_case(canonical_body=remove_runtime_hints_from_root(canonical_text))
        expect_case(
            "canonical-root-key-drift",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "ROOT_KEYS drift",
            errors,
        )

        write_case(canonical_body=remove_canonical_runtime_hint_keys(canonical_text))
        expect_case(
            "canonical-missing-constant",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "canonical missing RUNTIME_HINT_KEYS",
            errors,
        )

        write_case(preflight_body=add_asset_to_preflight_root(preflight_text))
        expect_case(
            "preflight-root-extra-key",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "ROOT_KEYS drift",
            errors,
        )

        write_case(preflight_body=drift_preflight_diagnostic_path(preflight_text))
        expect_case(
            "preflight-diagnostic-key-drift",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "DIAGNOSTIC_KEYS drift",
            errors,
        )

        write_case(preflight_body=remove_preflight_diagnostic_keys(preflight_text))
        expect_case(
            "preflight-missing-constant",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "preflight matrix missing DIAGNOSTIC_KEYS",
            errors,
        )

        write_case(boxtextured_body=drift_boxtextured_provenance_key(boxtextured_text))
        expect_case(
            "boxtextured-provenance-key-drift",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "PROVENANCE_KEYS drift",
            errors,
        )

        write_case(boxtextured_body=remove_boxtextured_root_keys(boxtextured_text))
        expect_case(
            "boxtextured-missing-constant",
            run_verifier(args.schema_verifier, canonical, preflight, boxtextured),
            1,
            "BoxTextured sidecar verifier missing ROOT_KEYS",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("sidecar_schema_constants_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
