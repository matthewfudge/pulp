#!/usr/bin/env python3
import argparse
import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def load_manifest(path):
    return json.loads(path.read_text(encoding="utf-8"))


def malformed_cases(manifest):
    missing_root_key = copy.deepcopy(manifest)
    missing_root_key.pop("golden_kind")

    root_extra = copy.deepcopy(manifest)
    root_extra["unexpected"] = "drift"

    invalid_status = copy.deepcopy(manifest)
    invalid_status["status"] = "default_adapter"

    duplicate_entry_id = copy.deepcopy(manifest)
    duplicate_entry_id["entries"][1]["id"] = duplicate_entry_id["entries"][0]["id"]

    missing_entry_key = copy.deepcopy(manifest)
    missing_entry_key["entries"][0].pop("fingerprint")

    entry_extra = copy.deepcopy(manifest)
    entry_extra["entries"][0]["unexpected"] = "drift"

    cpp_constant_missing = copy.deepcopy(manifest)
    cpp_constant_missing["entries"][0]["cpp_constant"] = "kMissingRendererFingerprint"

    cpp_constant_mismatch = copy.deepcopy(manifest)
    cpp_constant_mismatch["entries"][0]["fingerprint"] = "1"

    interim_scope_drift = copy.deepcopy(manifest)
    interim_scope_drift["entries"][0]["adapter_scope"] = "software_swiftshader"

    interim_backend_drift = copy.deepcopy(manifest)
    interim_backend_drift["entries"][0]["adapter_backend_type"] = "Null"

    missing_software_key = copy.deepcopy(manifest)
    missing_software_key["software_adapter"].pop("notes")

    software_extra = copy.deepcopy(manifest)
    software_extra["software_adapter"]["unexpected"] = "drift"

    interim_software_claim = copy.deepcopy(manifest)
    interim_software_claim["software_adapter"]["pixel_producing"] = True

    hardcoded_scene_data = copy.deepcopy(manifest)
    hardcoded_scene_data["entries"][0]["scene_data_consumed"] = True

    hardcoded_primitives = copy.deepcopy(manifest)
    hardcoded_primitives["entries"][0]["primitive_count"] = 1

    scene_data_not_consumed = copy.deepcopy(manifest)
    scene_data_not_consumed["entries"][1]["scene_data_consumed"] = False

    scene_data_no_pipeline = copy.deepcopy(manifest)
    scene_data_no_pipeline["entries"][1]["pipeline_cache_entry_count"] = 0

    scene_data_cache_entries_too_high = copy.deepcopy(manifest)
    scene_data_cache_entries_too_high["entries"][1]["pipeline_cache_entry_count"] = 2

    scene_data_cache_accounting_drift = copy.deepcopy(manifest)
    scene_data_cache_accounting_drift["entries"][1]["pipeline_cache_hit_count"] = 1

    renderer_source_mismatch = copy.deepcopy(manifest)
    renderer_source_mismatch["entries"][1]["source"] = "hardcoded"

    return [
        (
            "missing-root-key",
            missing_root_key,
            "manifest root keys must be exactly",
        ),
        (
            "extra-root-key",
            root_extra,
            "manifest root keys must be exactly",
        ),
        (
            "invalid-status",
            invalid_status,
            "status must be one of",
        ),
        (
            "duplicate-entry-id",
            duplicate_entry_id,
            "entries[1].id duplicates",
        ),
        (
            "missing-entry-key",
            missing_entry_key,
            "entries[0] missing required keys",
        ),
        (
            "extra-entry-key",
            entry_extra,
            "entries[0] has unexpected keys",
        ),
        (
            "cpp-constant-missing",
            cpp_constant_missing,
            "entries[0].cpp_constant kMissingRendererFingerprint not found in C++ test",
        ),
        (
            "cpp-constant-mismatch",
            cpp_constant_mismatch,
            "entries[0].fingerprint 1 does not match",
        ),
        (
            "interim-scope-drift",
            interim_scope_drift,
            "entries[0].adapter_scope must be macos_default_metal",
        ),
        (
            "interim-backend-drift",
            interim_backend_drift,
            "entries[0].adapter_backend_type must be Metal",
        ),
        (
            "missing-software-adapter-key",
            missing_software_key,
            "software_adapter keys must be exactly",
        ),
        (
            "extra-software-adapter-key",
            software_extra,
            "software_adapter keys must be exactly",
        ),
        (
            "interim-software-pixel-claim",
            interim_software_claim,
            "interim_default_adapter manifest must not claim a "
            "pixel-producing software adapter",
        ),
        (
            "hardcoded-scene-data-consumed",
            hardcoded_scene_data,
            "entries[0].scene_data_consumed must be false for "
            "Renderer3D::render_hardcoded_textured_cube",
        ),
        (
            "hardcoded-primitive-count",
            hardcoded_primitives,
            "entries[0].primitive_count must be 0 for "
            "Renderer3D::render_hardcoded_textured_cube",
        ),
        (
            "scene-data-not-consumed",
            scene_data_not_consumed,
            "entries[1].scene_data_consumed must be true for "
            "Renderer3D::render_scene_data",
        ),
        (
            "scene-data-empty-pipeline-cache",
            scene_data_no_pipeline,
            "entries[1].pipeline_cache_entry_count must be positive for "
            "Renderer3D::render_scene_data",
        ),
        (
            "scene-data-cache-entry-overflow",
            scene_data_cache_entries_too_high,
            "entries[1].pipeline_cache_entry_count must not exceed "
            "primitive_count",
        ),
        (
            "scene-data-cache-accounting-drift",
            scene_data_cache_accounting_drift,
            "entries[1].pipeline_cache_entry_count plus "
            "pipeline_cache_hit_count must equal primitive_count for "
            "Renderer3D::render_scene_data",
        ),
        (
            "scene-data-source-hardcoded",
            renderer_source_mismatch,
            "entries[1].source must name a scene asset for "
            "Renderer3D::render_scene_data",
        ),
    ]


def main():
    parser = argparse.ArgumentParser(
        description="Verify Renderer3D golden manifest rejects schema drift.")
    parser.add_argument("--validator", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--cpp-test", type=Path, required=True)
    args = parser.parse_args()

    if not args.validator.exists():
        print(f"validator_exists=false path={args.validator}")
        return 2
    if not args.manifest.exists():
        print(f"manifest_exists=false path={args.manifest}")
        return 2
    if not args.cpp_test.exists():
        print(f"cpp_test_exists=false path={args.cpp_test}")
        return 2

    manifest = load_manifest(args.manifest)
    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        for name, mutated, expected in malformed_cases(manifest):
            path = temp_path / f"{name}.json"
            path.write_text(json.dumps(mutated), encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(args.validator),
                    str(path),
                    "--cpp-test",
                    str(args.cpp_test),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            sys.stdout.write(result.stdout)
            if result.returncode != 1:
                errors.append(
                    f"{name}: expected exit 1, got {result.returncode}")
            if expected not in result.stdout:
                errors.append(
                    f"{name}: expected diagnostic containing {expected!r}")
            print(f"renderer_golden_manifest_malformed_rejected={name}")

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1
    print("renderer_golden_manifest_malformed_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
