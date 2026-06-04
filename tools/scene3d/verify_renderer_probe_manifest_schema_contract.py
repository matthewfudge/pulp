#!/usr/bin/env python3
"""Verifies renderer probe manifest-schema alignment rejects drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, probe_verifier, manifest_validator):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--probe-verifier",
            str(probe_verifier),
            "--manifest-validator",
            str(manifest_validator),
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


def remove_probe_manifest_key(text):
    return remove_once(text, '    "software_adapter",\n')


def add_probe_entry_key(text):
    return replace_once(
        text,
        """    "cpp_constant",
}
""",
        """    "cpp_constant",
    "unexpected_probe_field",
}
""",
    )


def remove_probe_entry_constant(text):
    return remove_once(
        text,
        """REQUIRED_ENTRY_KEYS = {
    "id",
    "renderer",
    "source",
    "width",
    "height",
    "adapter_backend_type",
    "adapter_scope",
    "scene_data_consumed",
    "primitive_count",
    "depth_target_allocated",
    "color_target_allocated",
    "vertex_buffer_uploaded",
    "index_buffer_uploaded",
    "uniform_buffer_uploaded",
    "texture_uploaded",
    "pipeline_cache_entry_count",
    "pipeline_cache_hit_count",
    "command_submitted",
    "readback_completed",
    "pixel_output_produced",
    "min_distinct_color_count",
    "min_non_transparent_pixel_count",
    "fingerprint",
    "cpp_constant",
}

""",
    )


def drift_validator_software_key(text):
    return replace_once(text, '    "golden_entry_ids",\n', '    "software_golden_entry_ids",\n')


def remove_validator_manifest_constant(text):
    return remove_once(
        text,
        """REQUIRED_MANIFEST_KEYS = {
    "schema_version",
    "golden_kind",
    "fingerprint_algorithm",
    "status",
    "entries",
    "software_adapter",
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
    print(f"renderer_probe_manifest_schema_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe manifest schema drift is rejected.")
    parser.add_argument("--schema-verifier", type=Path, required=True)
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--manifest-validator", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("schema_verifier", args.schema_verifier),
            ("probe_verifier", args.probe_verifier),
            ("manifest_validator", args.manifest_validator)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    probe_text = read_text(args.probe_verifier)
    validator_text = read_text(args.manifest_validator)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        probe = tmpdir / "verify_renderer_probe.py"
        validator = tmpdir / "validate_renderer_golden_manifest.py"

        def write_case(probe_body=probe_text, validator_body=validator_text):
            write_text(probe, probe_body)
            write_text(validator, validator_body)

        write_case()
        expect_case(
            "valid-current-manifest-schema",
            run_verifier(args.schema_verifier, probe, validator),
            0,
            "renderer_probe_manifest_schema_verified=true",
            errors,
        )

        write_case(probe_body=remove_probe_manifest_key(probe_text))
        expect_case(
            "probe-manifest-key-drift",
            run_verifier(args.schema_verifier, probe, validator),
            1,
            "REQUIRED_MANIFEST_KEYS drift",
            errors,
        )

        write_case(probe_body=add_probe_entry_key(probe_text))
        expect_case(
            "probe-entry-extra-key",
            run_verifier(args.schema_verifier, probe, validator),
            1,
            "REQUIRED_ENTRY_KEYS drift",
            errors,
        )

        write_case(probe_body=remove_probe_entry_constant(probe_text))
        expect_case(
            "probe-missing-entry-constant",
            run_verifier(args.schema_verifier, probe, validator),
            1,
            "missing constants: REQUIRED_ENTRY_KEYS",
            errors,
        )

        write_case(validator_body=drift_validator_software_key(validator_text))
        expect_case(
            "validator-software-key-drift",
            run_verifier(args.schema_verifier, probe, validator),
            1,
            "REQUIRED_SOFTWARE_ADAPTER_KEYS drift",
            errors,
        )

        write_case(validator_body=remove_validator_manifest_constant(validator_text))
        expect_case(
            "validator-missing-manifest-constant",
            run_verifier(args.schema_verifier, probe, validator),
            1,
            "missing constants: REQUIRED_MANIFEST_KEYS",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_manifest_schema_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
