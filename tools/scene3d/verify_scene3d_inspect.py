#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


EXPECTED_STATS = {
    "nodes": "2",
    "roots": "1",
    "meshes": "1",
    "primitives": "1",
    "indexed_primitives": "1",
    "vertices": "24",
    "indices": "36",
    "materials": "1",
    "textures": "1",
    "texture_samplers": "1",
    "texture_bytes": "3750",
    "advanced_material_extensions": "0",
    "cameras": "0",
    "lights": "0",
    "animations": "0",
    "unsupported_features": "0",
    "diagnostics": "0",
    "error_diagnostics": "0",
}

EXPECTED_RENDER_PACKET = {
    "transformed_nodes": "2",
    "primitives": "1",
    "diagnostics": "0",
    "has_errors": "false",
}

EXPECTED_PRIMITIVE = {
    "node": "1",
    "mesh": "0",
    "primitive": "0",
    "material": "0",
    "feature_mask": "15",
    "world_transform": "1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1",
    "features": "normals,texcoord0,indexed,base_color_texture",
}


def parse_key_values(line):
    values = {}
    for token in line.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        values[key] = value
    return values


def require_fields(actual, expected, label, errors):
    actual_keys = set(actual.keys())
    expected_keys = set(expected.keys())
    if actual_keys != expected_keys:
        errors.append(
            f"{label}.keys: expected {sorted(expected_keys)!r}, got {sorted(actual_keys)!r}")
    for key, expected_value in expected.items():
        actual_value = actual.get(key)
        if actual_value != expected_value:
            errors.append(
                f"{label}.{key}: expected {expected_value!r}, got {actual_value!r}")


def require_single_line(lines, prefix, label, errors):
    matches = [line for line in lines if line.startswith(prefix)]
    if len(matches) != 1:
        errors.append(f"{label}.count: expected 1, got {len(matches)}")
        return {}
    return parse_key_values(matches[0])


def run_inspect(command, fixture):
    return subprocess.run(
        [str(command), "--render-packet", str(fixture)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify stable pulp-scene3d-inspect output for BoxTextured.")
    parser.add_argument("--inspect-tool", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    args = parser.parse_args()

    if not args.inspect_tool.exists():
        print(f"inspect_tool_exists=false path={args.inspect_tool}")
        return 2
    if not args.fixture.exists():
        print(f"fixture_exists=false path={args.fixture}")
        return 2

    result = run_inspect(args.inspect_tool, args.fixture)
    sys.stdout.write(result.stdout)
    if result.returncode != 0:
        print(f"inspect_exit_code={result.returncode}", file=sys.stderr)
        return result.returncode

    errors = []
    lines = result.stdout.splitlines()
    stats = require_single_line(lines, "nodes=", "stats", errors)
    render_packet = require_single_line(lines,
                                        "render_packet ",
                                        "render_packet",
                                        errors)
    primitive = require_single_line(lines, "primitive ", "primitive", errors)
    require_fields(stats, EXPECTED_STATS, "stats", errors)
    require_fields(render_packet,
                   EXPECTED_RENDER_PACKET,
                   "render_packet",
                   errors)
    require_fields(primitive, EXPECTED_PRIMITIVE, "primitive", errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("scene3d_inspect_verified=boxtextured")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
