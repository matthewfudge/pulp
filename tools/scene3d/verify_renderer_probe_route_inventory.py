#!/usr/bin/env python3
"""Verify renderer probe route telemetry has script and CTest coverage."""

import argparse
import ast
import sys
from pathlib import Path


GENERIC_PROBE_FIELDS = {
    "scene",
    "width",
    "height",
    "success",
    "gpu_available",
    "adapter_info_available",
    "adapter_backend_type",
    "adapter_backend",
    "adapter_name",
    "adapter_vendor",
    "adapter_architecture",
    "adapter_scope",
    "adapter_backend_preference",
    "fallback_adapter_requested",
    "null_backend_requested",
    "primitive_count",
    "pipeline_cache_entry_count",
    "pipeline_cache_hit_count",
    "distinct_color_count",
    "non_transparent_pixel_count",
    "rgba_fingerprint",
    "pixel_output_produced",
    "final_software_golden_eligible",
}


def extract_probe_fields(path):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == "PROBE_FIELDS":
                value = ast.literal_eval(node.value)
                if not isinstance(value, set):
                    raise ValueError("PROBE_FIELDS must be a set literal")
                return set(value)
    raise ValueError("missing PROBE_FIELDS")


def route_files(tools_dir):
    return sorted(
        list(tools_dir.glob("renderer_probe_*_route_smoke.py")) +
        list(tools_dir.glob("renderer_probe_*_route_contract.py")))


def route_stems(tools_dir, suffix):
    return {
        path.name.removesuffix(suffix)
        for path in tools_dir.glob(f"renderer_probe_*_route{suffix}")
    }


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe route telemetry coverage.")
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--tools-dir", type=Path, required=True)
    parser.add_argument("--ctest-file", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("probe_verifier", args.probe_verifier),
            ("tools_dir", args.tools_dir),
            ("ctest_file", args.ctest_file)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    errors = []
    fields = extract_probe_fields(args.probe_verifier)
    route_paths = route_files(args.tools_dir)
    smoke_stems = route_stems(args.tools_dir, "_smoke.py")
    contract_stems = route_stems(args.tools_dir, "_contract.py")
    route_text = "\n".join(
        f"{path.name}\n{path.read_text(encoding='utf-8')}"
        for path in route_paths)
    ctest_text = args.ctest_file.read_text(encoding="utf-8")

    for stem in sorted(smoke_stems - contract_stems):
        errors.append(f"missing route negative contract for: {stem}")
    for stem in sorted(contract_stems - smoke_stems):
        errors.append(f"missing route positive smoke for: {stem}")

    for field in sorted(fields - GENERIC_PROBE_FIELDS):
        if field not in route_text:
            errors.append(f"missing route mention for probe field: {field}")

    for path in route_paths:
        if path.name not in ctest_text:
            errors.append(f"missing CTest route registration: {path.name}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(
        "renderer_probe_route_inventory_verified="
        f"{len(fields - GENERIC_PROBE_FIELDS)} fields "
        f"{len(route_paths)} route files "
        f"{len(smoke_stems & contract_stems)} route pairs")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
