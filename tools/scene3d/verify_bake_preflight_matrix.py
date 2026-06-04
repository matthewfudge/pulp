#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path


CASES = [
    {
        "fixture": "clean.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 0,
        "fields": {
            "bake_readiness": "clean",
            "export_blocked": "false",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "0",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "0",
            "diagnostics": "0",
        },
        "diagnostic_codes": [],
        "rows": [],
        "runtime_hints": [],
    },
    {
        "fixture": "default-pbr-textures-clean.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 0,
        "fields": {
            "bake_readiness": "clean",
            "export_blocked": "false",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "0",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "0",
            "diagnostics": "1",
        },
        "diagnostic_codes": ["scene3d.default_pbr_textures_clean"],
        "rows": [],
        "runtime_hints": [("materialTextureFloor", "default-pbr-textures")],
    },
    {
        "fixture": "missing-runtime-evidence.pulp3d.json",
        "require_runtime": True,
        "require_runtime_url": False,
        "exit_code": 2,
        "fields": {
            "bake_readiness": "blocked",
            "export_blocked": "true",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "true",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "0",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "0",
            "diagnostics": "0",
        },
        "diagnostic_codes": [],
        "rows": [],
        "runtime_hints": [],
    },
    {
        "fixture": "shader-material-blocked.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 2,
        "fields": {
            "bake_readiness": "blocked",
            "export_blocked": "true",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "1",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "0",
            "diagnostics": "1",
        },
        "diagnostic_codes": ["bake.unsupported_shader_material"],
        "rows": [("export_blocker", "ShaderMaterial", "/Scene/CustomShader")],
        "runtime_hints": [],
    },
    {
        "fixture": "live-runtime-blocked.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 2,
        "fields": {
            "bake_readiness": "blocked",
            "export_blocked": "true",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "6",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "0",
            "diagnostics": "6",
        },
        "diagnostic_codes": [
            "bake.unsupported_raw_shader_material",
            "bake.unsupported_postprocessing",
            "bake.unsupported_render_target",
            "bake.unsupported_arbitrary_js_animation",
            "bake.unsupported_physics",
            "bake.unsupported_event_handler",
        ],
        "rows": [
            ("export_blocker", "RawShaderMaterial", "/Scene/Background"),
            ("export_blocker", "Postprocessing", "/Scene/Composer"),
            ("export_blocker", "RenderTarget", "/Scene/Reflector"),
            ("export_blocker", "ArbitraryJSAnimation", "/Scene/useFrame"),
            ("export_blocker", "Physics", "/Scene/RigidBodies"),
            ("export_blocker", "EventHandler", "/Scene/InteractiveMesh/onClick"),
        ],
        "runtime_hints": [],
    },
    {
        "fixture": "texture-encoding-blocked.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 2,
        "fields": {
            "bake_readiness": "blocked",
            "export_blocked": "true",
            "texture_encoding_blocked": "true",
            "native_runtime_has_gaps": "false",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "0",
            "texture_encoding_blockers": "1",
            "native_runtime_gaps": "0",
            "diagnostics": "1",
        },
        "diagnostic_codes": ["bake.texture_encoding_missing"],
        "rows": [("texture_encoding_blocker", "TextureEncoding", "/Scene/Textured")],
        "runtime_hints": [],
    },
    {
        "fixture": "native-gap.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": False,
        "exit_code": 0,
        "fields": {
            "bake_readiness": "native_gaps",
            "export_blocked": "false",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "true",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "false",
            "export_blockers": "0",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "1",
            "diagnostics": "1",
        },
        "diagnostic_codes": ["gltf.animation_deferred"],
        "rows": [("native_runtime_gap", "TransformAnimation", "/Scene/Animated")],
        "runtime_hints": [],
    },
    {
        "fixture": "native-gap.pulp3d.json",
        "require_runtime": False,
        "require_runtime_url": True,
        "exit_code": 2,
        "fields": {
            "bake_readiness": "blocked",
            "export_blocked": "true",
            "texture_encoding_blocked": "false",
            "native_runtime_has_gaps": "true",
            "has_error_diagnostics": "false",
            "runtime_evidence_missing": "false",
            "runtime_evidence_url_invalid": "true",
            "export_blockers": "0",
            "texture_encoding_blockers": "0",
            "native_runtime_gaps": "1",
            "diagnostics": "1",
        },
        "diagnostic_codes": ["gltf.animation_deferred"],
        "rows": [("native_runtime_gap", "TransformAnimation", "/Scene/Animated")],
        "runtime_hints": [],
    },
]

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


def parse_fields(output):
    fields = {}
    for line in output.splitlines():
        if ":" in line:
            continue
        for token in line.split():
            if "=" in token:
                key, value = token.split("=", 1)
                fields[key] = value
    return fields


def parse_rows(output):
    rows = []
    for line in output.splitlines():
        if ": " not in line:
            continue
        label, rest = line.split(": ", 1)
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


def run_case(tool, fixture_dir, case):
    command = [str(tool)]
    if case["require_runtime"]:
        command.append("--require-runtime-evidence")
    if case["require_runtime_url"]:
        command.append("--require-runtime-evidence-url")
    command.append(str(fixture_dir / case["fixture"]))
    return subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def load_sidecar(fixture_dir, case, errors):
    path = fixture_dir / case["fixture"]
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{case['fixture']}: invalid sidecar JSON: {exc}")
        return None


def verify_sidecar_json(fixture_dir, case, errors):
    sidecar = load_sidecar(fixture_dir, case, errors)
    if sidecar is None:
        return

    if not isinstance(sidecar, dict):
        errors.append(f"{case['fixture']}: sidecar root must be an object")
        return
    if set(sidecar.keys()) != ROOT_KEYS:
        errors.append(
            f"{case['fixture']}.keys: expected {sorted(ROOT_KEYS)!r}, got {sorted(sidecar.keys())!r}")

    if sidecar.get("schema_version") != 1:
        errors.append(
            f"{case['fixture']}.schema_version: expected 1, got {sidecar.get('schema_version')!r}")

    provenance = sidecar.get("provenance")
    if not isinstance(provenance, dict):
        errors.append(f"{case['fixture']}.provenance: expected object")
    else:
        if set(provenance.keys()) != PROVENANCE_KEYS:
            errors.append(
                f"{case['fixture']}.provenance.keys: expected {sorted(PROVENANCE_KEYS)!r}, got {sorted(provenance.keys())!r}")
        for key in ("source", "exporter", "exported_at"):
            if not provenance.get(key):
                errors.append(f"{case['fixture']}.provenance.{key}: expected non-empty value")
        runtime_evidence = str(provenance.get("runtime_evidence", ""))
        expected_missing = case["fields"]["runtime_evidence_missing"] == "true"
        if bool(runtime_evidence) == expected_missing:
            expected = "empty" if expected_missing else "non-empty"
            errors.append(
                f"{case['fixture']}.provenance.runtime_evidence: expected {expected} value")

    diagnostics = sidecar.get("diagnostics")
    unsupported_features = sidecar.get("unsupported_features")
    runtime_hints = sidecar.get("runtime_hints")
    if not isinstance(diagnostics, list):
        errors.append(f"{case['fixture']}.diagnostics: expected array")
        diagnostics = []
    if not isinstance(unsupported_features, list):
        errors.append(f"{case['fixture']}.unsupported_features: expected array")
        unsupported_features = []
    if not isinstance(runtime_hints, list):
        errors.append(f"{case['fixture']}.runtime_hints: expected array")
        runtime_hints = []

    expected_codes = case["diagnostic_codes"]
    actual_codes = [str(item.get("code", "")) for item in diagnostics if isinstance(item, dict)]
    if actual_codes != expected_codes:
        errors.append(
            f"{case['fixture']}.diagnostic_codes: expected {expected_codes!r}, got {actual_codes!r}")
    for index, item in enumerate(diagnostics):
        if not isinstance(item, dict):
            errors.append(f"{case['fixture']}.diagnostics[{index}]: expected object")
            continue
        if set(item.keys()) != DIAGNOSTIC_KEYS:
            errors.append(
                f"{case['fixture']}.diagnostics[{index}].keys: expected {sorted(DIAGNOSTIC_KEYS)!r}, got {sorted(item.keys())!r}")
        if item.get("severity") != "warning":
            errors.append(
                f"{case['fixture']}.diagnostics[{index}].severity: expected 'warning', got {item.get('severity')!r}")
        for key in ("message", "path"):
            if not item.get(key):
                errors.append(f"{case['fixture']}.diagnostics[{index}].{key}: expected non-empty value")

    expected_features = [
        (feature, node)
        for _, feature, node in case["rows"]
    ]
    actual_features = []
    for index, item in enumerate(unsupported_features):
        if not isinstance(item, dict):
            errors.append(f"{case['fixture']}.unsupported_features[{index}]: expected object")
            continue
        if set(item.keys()) != UNSUPPORTED_FEATURE_KEYS:
            errors.append(
                f"{case['fixture']}.unsupported_features[{index}].keys: expected {sorted(UNSUPPORTED_FEATURE_KEYS)!r}, got {sorted(item.keys())!r}")
        feature = str(item.get("feature", ""))
        node_path = str(item.get("node_path", ""))
        actual_features.append((feature, node_path))
        if not item.get("reason"):
            errors.append(
                f"{case['fixture']}.unsupported_features[{index}].reason: expected non-empty value")
    if actual_features != expected_features:
        errors.append(
            f"{case['fixture']}.unsupported_features: expected {expected_features!r}, got {actual_features!r}")

    expected_hints = case["runtime_hints"]
    actual_hints = []
    for index, item in enumerate(runtime_hints):
        if not isinstance(item, dict):
            errors.append(f"{case['fixture']}.runtime_hints[{index}]: expected object")
            continue
        if set(item.keys()) != RUNTIME_HINT_KEYS:
            errors.append(
                f"{case['fixture']}.runtime_hints[{index}].keys: expected {sorted(RUNTIME_HINT_KEYS)!r}, got {sorted(item.keys())!r}")
        actual_hints.append((str(item.get("key", "")), str(item.get("value", ""))))
    if actual_hints != expected_hints:
        errors.append(
            f"{case['fixture']}.runtime_hints: expected {expected_hints!r}, got {actual_hints!r}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake-preflight classification across sidecar fixtures.")
    parser.add_argument("--preflight-tool", type=Path, required=True)
    parser.add_argument("--fixture-dir", type=Path, required=True)
    args = parser.parse_args()

    if not args.preflight_tool.exists():
        print(f"preflight_tool_exists=false path={args.preflight_tool}")
        return 2
    if not args.fixture_dir.exists():
        print(f"fixture_dir_exists=false path={args.fixture_dir}")
        return 2

    errors = []
    expected_fixtures = {case["fixture"] for case in CASES}
    discovered_fixtures = {
        path.name for path in args.fixture_dir.glob("*.pulp3d.json")
    }
    for fixture in sorted(expected_fixtures - discovered_fixtures):
        errors.append(f"missing matrix sidecar fixture: {fixture}")
    for fixture in sorted(discovered_fixtures - expected_fixtures):
        errors.append(f"unlisted matrix sidecar fixture: {fixture}")

    for case in CASES:
        verify_sidecar_json(args.fixture_dir, case, errors)
        result = run_case(args.preflight_tool, args.fixture_dir, case)
        print(
            f"case={case['fixture']} "
            f"require_runtime={str(case['require_runtime']).lower()} "
            f"require_runtime_url={str(case['require_runtime_url']).lower()}")
        sys.stdout.write(result.stdout)
        if result.returncode != case["exit_code"]:
            errors.append(
                f"{case['fixture']}: expected exit {case['exit_code']}, got {result.returncode}")
            continue

        fields = parse_fields(result.stdout)
        for key, expected in case["fields"].items():
            actual = fields.get(key)
            if actual != expected:
                errors.append(
                    f"{case['fixture']}.{key}: expected {expected!r}, got {actual!r}")

        rows = parse_rows(result.stdout)
        if rows != case["rows"]:
            errors.append(
                f"{case['fixture']}.rows: expected {case['rows']!r}, got {rows!r}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("bake_preflight_matrix_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
