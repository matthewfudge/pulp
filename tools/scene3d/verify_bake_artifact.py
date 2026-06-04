#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path
from urllib.parse import urlparse


SKIP_MISSING_VALIDATOR = 77

ROOT_KEYS = {
    "schema_version",
    "provenance",
    "diagnostics",
    "unsupported_features",
    "runtime_hints",
}

PROVENANCE_KEYS = {
    "source",
    "exporter",
    "exported_at",
    "runtime_evidence",
}

DIAGNOSTIC_KEYS = {
    "severity",
    "code",
    "message",
    "path",
}

UNSUPPORTED_FEATURE_KEYS = {
    "feature",
    "reason",
    "node_path",
}

RUNTIME_HINT_KEYS = {
    "key",
    "value",
}

PREFLIGHT_FIELDS = {
    "bake_readiness",
    "export_blocked",
    "texture_encoding_blocked",
    "native_runtime_has_gaps",
    "has_error_diagnostics",
    "runtime_evidence_missing",
    "runtime_evidence_url_invalid",
    "export_blockers",
    "texture_encoding_blockers",
    "native_runtime_gaps",
    "diagnostics",
}


def run_command(command):
    return subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def parse_preflight(stdout):
    fields = {}
    for line in stdout.splitlines():
        if "=" not in line or ": " in line:
            continue
        for token in line.split():
            if "=" in token:
                key, value = token.split("=", 1)
                fields[key] = value
    return fields


def parse_preflight_rows(stdout):
    rows = []
    for line in stdout.splitlines():
        if ": " not in line:
            continue
        label, rest = line.split(": ", 1)
        if label not in {
                "export_blocker",
                "texture_encoding_blocker",
                "native_runtime_gap"}:
            continue
        parts = rest.split()
        if not parts:
            continue
        feature = parts[0]
        node = ""
        for part in parts[1:]:
            if part.startswith("node="):
                node = part.removeprefix("node=")
                break
        rows.append((label, feature, node))
    return rows


def print_prefixed(prefix, text):
    for line in text.splitlines():
        if line:
            print(f"{prefix}{line}")


def is_supported_asset_extension(path: Path):
    return path.suffix.lower() in {".glb", ".gltf"}


def is_supported_sidecar_extension(path: Path):
    suffixes = [suffix.lower() for suffix in path.suffixes]
    return len(suffixes) >= 2 and suffixes[-2:] == [".pulp3d", ".json"]


def expect_key_set(name, value, expected_keys):
    if set(value.keys()) == expected_keys:
        print(f"{name}_keys_valid=true")
        return True
    print(f"{name}_keys_valid=false")
    print(
        f"error={name} keys expected {sorted(expected_keys)!r}, "
        f"got {sorted(value.keys())!r}")
    return False


def is_string(value):
    return isinstance(value, str)


def validate_string_field(name, value, *, allow_empty):
    if not is_string(value):
        print(f"{name}_valid=false")
        print(f"error={name} must be a string")
        return False
    if not allow_empty and not value:
        print(f"{name}_valid=false")
        print(f"error={name} must be a non-empty string")
        return False
    return True


def is_runtime_evidence_url(value):
    parsed = urlparse(value)
    return (
        parsed.scheme in {"http", "https"} and
        bool(parsed.netloc) and
        bool(parsed.path)
    )


def validate_provenance(provenance):
    for key in ("source", "exporter", "exported_at"):
        if not validate_string_field(f"sidecar_provenance_{key}",
                                     provenance.get(key),
                                     allow_empty=False):
            return False
    if not validate_string_field("sidecar_provenance_runtime_evidence",
                                 provenance.get("runtime_evidence"),
                                 allow_empty=True):
        return False
    print("sidecar_provenance_values_valid=true")
    return True


def validate_object_array(sidecar, key, expected_keys):
    value = sidecar.get(key)
    if not isinstance(value, list):
        print(f"sidecar_{key}_valid=false")
        print(f"error=sidecar {key} must be an array")
        return False
    for index, item in enumerate(value):
        if not isinstance(item, dict):
            print(f"sidecar_{key}_valid=false")
            print(f"error=sidecar {key}[{index}] must be an object")
            return False
        if set(item.keys()) != expected_keys:
            print(f"sidecar_{key}_valid=false")
            print(
                f"error=sidecar {key}[{index}] keys expected "
                f"{sorted(expected_keys)!r}, got {sorted(item.keys())!r}")
            return False
        for item_key in expected_keys:
            if not validate_string_field(f"sidecar_{key}_{index}_{item_key}",
                                         item.get(item_key),
                                         allow_empty=True):
                print(f"sidecar_{key}_valid=false")
                return False
        if key == "diagnostics" and item.get("severity") not in {
                "info", "warning", "error"}:
            print(f"sidecar_{key}_valid=false")
            print(
                f"error=sidecar {key}[{index}].severity must be info, warning, or error")
            return False
    print(f"sidecar_{key}_valid=true")
    return True


def validate_preflight_fields(fields):
    if set(fields.keys()) == PREFLIGHT_FIELDS:
        print("preflight_fields_valid=true")
        return True
    print("preflight_fields_valid=false")
    print(
        f"error=preflight fields expected {sorted(PREFLIGHT_FIELDS)!r}, "
        f"got {sorted(fields.keys())!r}")
    return False


def validate_unsupported_feature_classification(sidecar, preflight_stdout):
    unsupported_features = sidecar.get("unsupported_features")
    if not unsupported_features:
        print("unsupported_features_classified=true")
        return True

    classified = parse_preflight_rows(preflight_stdout)
    classified_features = {
        (feature, node)
        for _, feature, node in classified
    }
    missing = []
    for item in unsupported_features:
        feature = str(item.get("feature", ""))
        node_path = str(item.get("node_path", ""))
        if (feature, node_path) not in classified_features:
            missing.append((feature, node_path))

    if not missing:
        print("unsupported_features_classified=true")
        return True

    print("unsupported_features_classified=false")
    for feature, node_path in missing:
        print(
            "error=unsupported feature was not classified by preflight: "
            f"feature={feature} node_path={node_path}")
    return False


def validate_preflight_row_counts(fields, preflight_stdout):
    rows = parse_preflight_rows(preflight_stdout)
    try:
        expected = {
            "export_blocker": int(fields.get("export_blockers", "-1")),
            "texture_encoding_blocker": int(
                fields.get("texture_encoding_blockers", "-1")),
            "native_runtime_gap": int(fields.get("native_runtime_gaps", "-1")),
        }
    except ValueError:
        print("preflight_row_counts_valid=false")
        print("error=preflight row counts must be integers")
        return False
    actual = {label: 0 for label in expected}
    for label, _, _ in rows:
        if label in actual:
            actual[label] += 1

    if actual == expected:
        print("preflight_row_counts_valid=true")
        return True

    print("preflight_row_counts_valid=false")
    print(f"error=preflight row counts expected {expected!r}, got {actual!r}")
    return False


def validate_preflight_sidecar_counts(sidecar, fields):
    try:
        expected_diagnostics = int(fields.get("diagnostics", "-1"))
        expected_unsupported = (
            int(fields.get("export_blockers", "-1")) +
            int(fields.get("texture_encoding_blockers", "-1")) +
            int(fields.get("native_runtime_gaps", "-1")))
    except ValueError:
        print("preflight_sidecar_counts_valid=false")
        print("error=preflight sidecar counts must be integers")
        return False

    actual_diagnostics = len(sidecar.get("diagnostics", []))
    actual_unsupported = len(sidecar.get("unsupported_features", []))
    if (actual_diagnostics == expected_diagnostics and
            actual_unsupported == expected_unsupported):
        print("preflight_sidecar_counts_valid=true")
        return True

    print("preflight_sidecar_counts_valid=false")
    print(
        "error=preflight summary counts do not match sidecar arrays: "
        f"diagnostics expected {expected_diagnostics}, got {actual_diagnostics}; "
        f"unsupported_features expected {expected_unsupported}, got {actual_unsupported}")
    return False


def validate_preflight_readiness_consistency(fields):
    readiness = fields.get("bake_readiness", "missing")
    export_blocked = fields.get("export_blocked", "missing")
    native_runtime_has_gaps = fields.get("native_runtime_has_gaps", "missing")
    if export_blocked == "true":
        expected = "blocked"
    elif native_runtime_has_gaps == "true":
        expected = "native_gaps"
    else:
        expected = "clean"

    if readiness == expected:
        print("preflight_readiness_consistent=true")
        return True

    print("preflight_readiness_consistent=false")
    print(
        "error=preflight readiness inconsistent with summary flags: "
        f"expected {expected}, got {readiness}")
    return False


def validate_preflight_exit_code(fields, returncode):
    export_blocked = fields.get("export_blocked", "missing")
    expected = 2 if export_blocked == "true" else 0
    if returncode == expected:
        print("preflight_exit_code_consistent=true")
        return True

    print("preflight_exit_code_consistent=false")
    print(
        "error=preflight exit code inconsistent with export_blocked: "
        f"expected {expected}, got {returncode}")
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Verify a native 3D bake artifact pair: GLB/GLTF + sidecar.")
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--sidecar", type=Path, required=True)
    parser.add_argument("--preflight-tool", type=Path, required=True)
    parser.add_argument(
        "--validator-tool",
        type=Path,
        default=Path(__file__).with_name("validate_gltf.py"))
    parser.add_argument("--require-runtime-evidence", action="store_true")
    parser.add_argument(
        "--require-runtime-evidence-url",
        action="store_true",
        help="Require runtime_evidence to be an http(s) URL, not only a non-empty token.")
    parser.add_argument("--require-gltf-validator", action="store_true")
    parser.add_argument(
        "--require-extensions",
        action="store_true",
        help="Require asset to be .glb/.gltf and sidecar to be .pulp3d.json.")
    args = parser.parse_args()

    if not args.asset.exists():
        print(f"asset_exists=false")
        print(f"error=asset not found: {args.asset}")
        return 2
    if not args.sidecar.exists():
        print("sidecar_exists=false")
        print(f"error=sidecar not found: {args.sidecar}")
        return 2
    if not args.preflight_tool.exists():
        print("preflight_tool_exists=false")
        print(f"error=preflight tool not found: {args.preflight_tool}")
        return 2

    print("asset_exists=true")
    print("sidecar_exists=true")
    asset_extension_supported = is_supported_asset_extension(args.asset)
    sidecar_extension_supported = is_supported_sidecar_extension(args.sidecar)
    print(
        "asset_extension_supported="
        f"{'true' if asset_extension_supported else 'false'}")
    print(
        "sidecar_extension_supported="
        f"{'true' if sidecar_extension_supported else 'false'}")
    if args.require_extensions:
        if not asset_extension_supported:
            print("error=asset must have .glb or .gltf extension")
            return 2
        if not sidecar_extension_supported:
            print("error=sidecar must have .pulp3d.json extension")
            return 2

    try:
        sidecar = json.loads(args.sidecar.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print("sidecar_json_valid=false")
        print(f"error={exc}")
        return 2

    print("sidecar_json_valid=true")
    if not isinstance(sidecar, dict):
        print("sidecar_root_valid=false")
        print("error=sidecar root must be an object")
        return 2
    print("sidecar_root_valid=true")
    if not expect_key_set("sidecar_root", sidecar, ROOT_KEYS):
        return 2

    schema_version = sidecar.get("schema_version", "missing")
    print(f"sidecar_schema_version={schema_version}")
    if schema_version != 1:
        print("sidecar_schema_supported=false")
        print("error=sidecar schema_version must be 1")
        return 2
    print("sidecar_schema_supported=true")

    provenance = sidecar.get("provenance")
    if not isinstance(provenance, dict):
        print("sidecar_provenance_valid=false")
        print("error=sidecar provenance must be an object")
        return 2
    print("sidecar_provenance_valid=true")
    if not expect_key_set("sidecar_provenance", provenance, PROVENANCE_KEYS):
        return 2
    if not validate_provenance(provenance):
        return 2

    if not validate_object_array(sidecar, "diagnostics", DIAGNOSTIC_KEYS):
        return 2
    if not validate_object_array(
            sidecar, "unsupported_features", UNSUPPORTED_FEATURE_KEYS):
        return 2
    if not validate_object_array(sidecar, "runtime_hints", RUNTIME_HINT_KEYS):
        return 2

    source = provenance.get("source", "")
    exporter = provenance.get("exporter", "")
    exported_at = provenance.get("exported_at", "")
    runtime_evidence = provenance.get("runtime_evidence", "")
    print(f"sidecar_source_present={'true' if source else 'false'}")
    print(f"sidecar_exporter_present={'true' if exporter else 'false'}")
    print(f"sidecar_exported_at_present={'true' if exported_at else 'false'}")
    if not source or not exporter or not exported_at:
        print("error=sidecar provenance requires source, exporter, and exported_at")
        return 2
    print(f"runtime_evidence_present={'true' if runtime_evidence else 'false'}")
    runtime_evidence_url_valid = is_runtime_evidence_url(runtime_evidence)
    print(
        "runtime_evidence_url_valid="
        f"{'true' if runtime_evidence_url_valid else 'false'}")
    if args.require_runtime_evidence_url and not runtime_evidence_url_valid:
        print("error=runtime_evidence must be an http(s) URL")
        return 2

    validator_command = [
        sys.executable,
        str(args.validator_tool),
        str(args.asset),
    ]
    if args.require_gltf_validator:
        validator_command.append("--require-validator")
    validator = run_command(validator_command)
    if validator.returncode == SKIP_MISSING_VALIDATOR:
        print("gltf_validator=skipped")
        print_prefixed("gltf_validator_message=", validator.stdout)
        if args.require_gltf_validator:
            return 2
    elif validator.returncode == 0:
        print("gltf_validator=passed")
        print_prefixed("gltf_validator_output=", validator.stdout)
    else:
        print("gltf_validator=failed")
        print_prefixed("gltf_validator_output=", validator.stdout)
        return validator.returncode

    preflight_command = [str(args.preflight_tool)]
    if args.require_runtime_evidence_url:
        preflight_command.append("--require-runtime-evidence-url")
    elif args.require_runtime_evidence:
        preflight_command.append("--require-runtime-evidence")
    preflight_command.append(str(args.sidecar))
    preflight = run_command(preflight_command)
    print_prefixed("preflight_output=", preflight.stdout)

    fields = parse_preflight(preflight.stdout)
    if not validate_preflight_fields(fields):
        print("bake_artifact_verified=false")
        return 2
    if not validate_preflight_row_counts(fields, preflight.stdout):
        print("bake_artifact_verified=false")
        return 2
    if not validate_preflight_sidecar_counts(sidecar, fields):
        print("bake_artifact_verified=false")
        return 2
    if not validate_preflight_readiness_consistency(fields):
        print("bake_artifact_verified=false")
        return 2
    if not validate_preflight_exit_code(fields, preflight.returncode):
        print("bake_artifact_verified=false")
        return 2
    if not validate_unsupported_feature_classification(sidecar, preflight.stdout):
        print("bake_artifact_verified=false")
        return 2

    readiness = fields.get("bake_readiness", "missing")
    export_blocked = fields.get("export_blocked", "missing")
    runtime_missing = fields.get("runtime_evidence_missing", "missing")
    runtime_url_invalid = fields.get("runtime_evidence_url_invalid", "missing")
    print(f"bake_readiness={readiness}")
    print(f"export_blocked={export_blocked}")
    print(f"runtime_evidence_missing={runtime_missing}")
    print(f"runtime_evidence_url_invalid={runtime_url_invalid}")

    if preflight.returncode != 0:
        print("bake_artifact_verified=false")
        return preflight.returncode

    if readiness != "clean":
        print("bake_artifact_verified=false")
        return 1

    print("bake_artifact_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
