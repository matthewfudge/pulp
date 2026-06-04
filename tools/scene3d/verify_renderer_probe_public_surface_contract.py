#!/usr/bin/env python3
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, header, probe, probe_verifier):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--renderer-header",
            str(header),
            "--probe-source",
            str(probe),
            "--probe-verifier",
            str(probe_verifier),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def remove_header_bool(text, name):
    line = f"    bool {name} = false;\n"
    if line not in text:
        raise ValueError(f"missing header bool line for {name}")
    return text.replace(line, "", 1)


def remove_probe_print_bool(text, name):
    line = f'    print_bool("{name}", result.{name});\n'
    if line not in text:
        raise ValueError(f"missing probe print_bool line for {name}")
    return text.replace(line, "", 1)


def remove_verifier_field(text, name):
    line = f'    "{name}",\n'
    if line not in text:
        raise ValueError(f"missing verifier field line for {name}")
    return text.replace(line, "", 1)


def mismatch_probe_key(text, name):
    line = f'    print_bool("{name}", result.{name});\n'
    replacement = f'    print_bool("{name}_drift", result.{name});\n'
    if line not in text:
        raise ValueError(f"missing probe print_bool line for {name}")
    return text.replace(line, replacement, 1)


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_public_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify public-surface checker rejects Renderer3D probe drift.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--renderer-header", type=Path, required=True)
    parser.add_argument("--probe-source", type=Path, required=True)
    parser.add_argument("--probe-verifier", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("renderer_header", args.renderer_header),
            ("probe_source", args.probe_source),
            ("probe_verifier", args.probe_verifier)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    header_text = read_text(args.renderer_header)
    probe_text = read_text(args.probe_source)
    verifier_text = read_text(args.probe_verifier)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        header = tmpdir / "renderer3d.hpp"
        probe = tmpdir / "renderer3d_probe.cpp"
        probe_verifier = tmpdir / "verify_renderer_probe.py"

        def write_case(header_body=header_text,
                       probe_body=probe_text,
                       verifier_body=verifier_text):
            write_text(header, header_body)
            write_text(probe, probe_body)
            write_text(probe_verifier, verifier_body)

        write_case()
        expect_case(
            "valid-current-surface",
            run_verifier(args.surface_verifier, header, probe, probe_verifier),
            0,
            "renderer_probe_public_surface_verified=79 bools",
            errors,
        )

        write_case(header_body=remove_header_bool(header_text,
                                                  "texture_uploaded"))
        expect_case(
            "header-bool-removed",
            run_verifier(args.surface_verifier, header, probe, probe_verifier),
            1,
            "probe printed bool fields drift",
            errors,
        )

        write_case(probe_body=remove_probe_print_bool(probe_text,
                                                      "texture_uploaded"))
        expect_case(
            "probe-print-removed",
            run_verifier(args.surface_verifier, header, probe, probe_verifier),
            1,
            "probe printed bool fields drift",
            errors,
        )

        write_case(verifier_body=remove_verifier_field(verifier_text,
                                                       "texture_uploaded"))
        expect_case(
            "verifier-field-removed",
            run_verifier(args.surface_verifier, header, probe, probe_verifier),
            1,
            "verifier PROBE_FIELDS missing result bools",
            errors,
        )

        write_case(probe_body=mismatch_probe_key(probe_text,
                                                 "texture_uploaded"))
        expect_case(
            "probe-key-mismatch",
            run_verifier(args.surface_verifier, header, probe, probe_verifier),
            1,
            "probe print_bool key/member mismatch",
            errors,
        )

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("renderer_probe_public_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
