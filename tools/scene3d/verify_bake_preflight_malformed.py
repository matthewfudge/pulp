#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def valid_sidecar():
    return {
        "schema_version": 1,
        "provenance": {
            "source": "malformed-contract-fixture",
            "exporter": "scene3d-test",
            "exported_at": "2026-06-03T00:00:00Z",
            "runtime_evidence": "native-preflight-fixture",
        },
        "diagnostics": [],
        "unsupported_features": [],
        "runtime_hints": [],
    }


def malformed_cases():
    string_schema_version = valid_sidecar()
    string_schema_version["schema_version"] = "1"

    unsupported_schema_version = valid_sidecar()
    unsupported_schema_version["schema_version"] = 2

    numeric_provenance = valid_sidecar()
    numeric_provenance["provenance"]["source"] = 42

    empty_provenance_source = valid_sidecar()
    empty_provenance_source["provenance"]["source"] = ""

    empty_provenance_exporter = valid_sidecar()
    empty_provenance_exporter["provenance"]["exporter"] = ""

    empty_provenance_exported_at = valid_sidecar()
    empty_provenance_exported_at["provenance"]["exported_at"] = ""

    invalid_severity = valid_sidecar()
    invalid_severity["diagnostics"].append({
        "severity": "notice",
        "code": "scene.notice",
        "message": "Invalid severity should not coerce to info.",
        "path": "fixture.glb",
    })

    extra_diagnostic_key = valid_sidecar()
    extra_diagnostic_key["diagnostics"].append({
        "severity": "warning",
        "code": "scene.warning",
        "message": "Unexpected key should be rejected.",
        "path": "fixture.glb",
        "extra": "drift",
    })

    missing_root_key = valid_sidecar()
    del missing_root_key["runtime_hints"]

    nonstring_feature = valid_sidecar()
    nonstring_feature["unsupported_features"].append({
        "feature": "ShaderMaterial",
        "reason": ["not", "a", "string"],
        "node_path": "/Scene/CustomShader",
    })

    nonstring_hint = valid_sidecar()
    nonstring_hint["runtime_hints"].append({
        "key": "preferredCamera",
        "value": 7,
    })

    return [
        ("string-schema-version", string_schema_version,
         "Unsupported or missing sidecar schema_version"),
        ("unsupported-schema-version", unsupported_schema_version,
         "Unsupported or missing sidecar schema_version"),
        ("numeric-provenance", numeric_provenance, "invalid field shape"),
        ("empty-provenance-source", empty_provenance_source,
         "invalid field shape"),
        ("empty-provenance-exporter", empty_provenance_exporter,
         "invalid field shape"),
        ("empty-provenance-exported-at", empty_provenance_exported_at,
         "invalid field shape"),
        ("invalid-diagnostic-severity", invalid_severity, "invalid field shape"),
        ("extra-diagnostic-key", extra_diagnostic_key, "invalid field shape"),
        ("missing-root-key", missing_root_key, "invalid field shape"),
        ("nonstring-unsupported-feature", nonstring_feature,
         "invalid field shape"),
        ("nonstring-runtime-hint", nonstring_hint, "invalid field shape"),
    ]


def run_preflight(preflight_tool, path):
    return subprocess.run(
        [str(preflight_tool), str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake-preflight rejects malformed sidecar JSON.")
    parser.add_argument("--preflight-tool", type=Path, required=True)
    args = parser.parse_args()

    if not args.preflight_tool.exists():
        print(f"preflight_tool_exists=false path={args.preflight_tool}")
        return 2

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        for name, sidecar, expected in malformed_cases():
            path = temp_path / f"{name}.pulp3d.json"
            path.write_text(json.dumps(sidecar), encoding="utf-8")
            result = run_preflight(args.preflight_tool, path)
            sys.stdout.write(result.stdout)
            if result.returncode != 1:
                errors.append(
                    f"{name}: expected exit 1, got {result.returncode}")
            if expected not in result.stdout:
                errors.append(f"{name}: expected diagnostic containing {expected!r}")
            print(f"malformed_sidecar_rejected={name}")

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1
    print("bake_preflight_malformed_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
