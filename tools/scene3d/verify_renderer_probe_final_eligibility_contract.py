#!/usr/bin/env python3
"""Verifies Renderer3D final software eligibility gate rejects drift."""

import argparse
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def load_final_contract(path):
    spec = importlib.util.spec_from_file_location(
        "verify_renderer_probe_final_eligibility", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_json(path, data):
    path.write_text(json.dumps(data), encoding="utf-8")


def write_probe(path, final_contract, final_eligible):
    path.write_text(
        final_contract.fake_probe_source(final_eligible),
        encoding="utf-8")


def run_verifier(verifier, manifest, probe):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--manifest",
            str(manifest),
            "--entry-id",
            "hardcoded_textured_cube",
            "--",
            sys.executable,
            str(probe),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_final_eligibility_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify Renderer3D final software eligibility gate cases.")
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--final-eligibility-contract", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("probe_verifier", args.probe_verifier),
            ("final_eligibility_contract", args.final_eligibility_contract)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    final_contract = load_final_contract(args.final_eligibility_contract)
    base_manifest = final_contract.final_manifest()
    errors = []

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        manifest_path = temp_path / "renderer3d-final-gate.json"
        probe_path = temp_path / "renderer3d-final-probe.py"

        write_json(manifest_path, base_manifest)
        write_probe(probe_path, final_contract, True)
        expect_case(
            "valid-final-software-entry",
            run_verifier(args.probe_verifier, manifest_path, probe_path),
            0,
            "renderer_probe_verified=hardcoded_textured_cube",
            errors,
        )

        status_manifest = dict(base_manifest)
        status_manifest["status"] = "interim_default_adapter"
        write_json(manifest_path, status_manifest)
        write_probe(probe_path, final_contract, True)
        expect_case(
            "interim-status-rejects-final-true",
            run_verifier(args.probe_verifier, manifest_path, probe_path),
            1,
            "final_software_golden_eligible: expected 'false', got 'true'",
            errors,
        )

        pixel_manifest = dict(base_manifest)
        pixel_manifest["software_adapter"] = dict(
            base_manifest["software_adapter"])
        pixel_manifest["software_adapter"]["pixel_producing"] = False
        write_json(manifest_path, pixel_manifest)
        write_probe(probe_path, final_contract, True)
        expect_case(
            "non-pixel-software-rejects-final-true",
            run_verifier(args.probe_verifier, manifest_path, probe_path),
            1,
            "final_software_golden_eligible: expected 'false', got 'true'",
            errors,
        )

        ids_manifest = dict(base_manifest)
        ids_manifest["software_adapter"] = dict(
            base_manifest["software_adapter"])
        ids_manifest["software_adapter"]["golden_entry_ids"] = []
        write_json(manifest_path, ids_manifest)
        write_probe(probe_path, final_contract, True)
        expect_case(
            "missing-entry-id-rejects-final-true",
            run_verifier(args.probe_verifier, manifest_path, probe_path),
            1,
            "final_software_golden_eligible: expected 'false', got 'true'",
            errors,
        )

        write_json(manifest_path, base_manifest)
        write_probe(probe_path, final_contract, False)
        expect_case(
            "final-entry-rejects-final-false",
            run_verifier(args.probe_verifier, manifest_path, probe_path),
            1,
            "final_software_golden_eligible: expected 'true', got 'false'",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_final_eligibility_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
