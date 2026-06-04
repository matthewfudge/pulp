#!/usr/bin/env python3
"""Pins SceneStats fields, aggregation inputs, and text output order."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_FIELDS = [
    "nodes",
    "root_nodes",
    "meshes",
    "primitives",
    "indexed_primitives",
    "vertices",
    "indices",
    "materials",
    "textures",
    "texture_samplers",
    "texture_bytes",
    "advanced_material_extensions",
    "cameras",
    "lights",
    "animations",
    "unsupported_features",
    "diagnostics",
    "error_diagnostics",
]

EXPECTED_TEXT_KEYS = [
    "nodes",
    "roots",
    "meshes",
    "primitives",
    "indexed_primitives",
    "vertices",
    "indices",
    "materials",
    "textures",
    "texture_samplers",
    "texture_bytes",
    "advanced_material_extensions",
    "cameras",
    "lights",
    "animations",
    "unsupported_features",
    "diagnostics",
    "error_diagnostics",
]

EXPECTED_ASSIGNMENTS = [
    "stats.nodes = scene.nodes.size();",
    "stats.root_nodes = scene.root_nodes.size();",
    "stats.meshes = scene.meshes.size();",
    "stats.materials = scene.materials.size();",
    "stats.textures = scene.textures.size();",
    "stats.texture_samplers = scene.texture_samplers.size();",
    "stats.cameras = scene.cameras.size();",
    "stats.lights = scene.lights.size();",
    "stats.animations = scene.animations.size();",
    "stats.unsupported_features = scene.unsupported_features.size();",
    "stats.diagnostics = scene.diagnostics.size();",
    "stats.primitives += mesh.primitives.size();",
    "stats.vertices += primitive.positions.size() / 3u;",
    "stats.indices += primitive.indices.size();",
    "++stats.indexed_primitives;",
    "stats.texture_bytes += texture.encoded_bytes.size();",
    "stats.advanced_material_extensions +=",
    "material.advanced_material_extensions.size();",
    "++stats.error_diagnostics;",
]


def extract_stats_fields(header):
    match = re.search(
        r"struct\s+SceneStats\s*\{(?P<body>.*?)\n\};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing SceneStats struct")
    return re.findall(r"size_t\s+([A-Za-z0-9_]+)\s*=\s*0;", match.group("body"))


def extract_text_keys(source):
    match = re.search(
        r"std::string\s+scene_stats_to_text\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing scene_stats_to_text body")
    return re.findall(r'<<\s*" ?([A-Za-z0-9_]+)=', match.group("body"))


def require_ordered_snippets(source, snippets):
    errors = []
    position = 0
    for snippet in snippets:
        found = source.find(snippet, position)
        if found == -1:
            errors.append(f"missing ordered SceneStats snippet {snippet!r}")
        else:
            position = found + len(snippet)
    return errors


def verify(header, source):
    errors = []
    fields = extract_stats_fields(header)
    text_keys = extract_text_keys(source)

    if fields != EXPECTED_FIELDS:
        errors.append(f"SceneStats fields mismatch: expected {EXPECTED_FIELDS!r}, got {fields!r}")
    if text_keys != EXPECTED_TEXT_KEYS:
        errors.append(f"scene_stats_to_text keys mismatch: expected {EXPECTED_TEXT_KEYS!r}, got {text_keys!r}")
    errors.extend(require_ordered_snippets(source, EXPECTED_ASSIGNMENTS))
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    errors = verify(
        args.header.read_text(encoding="utf-8"),
        args.source.read_text(encoding="utf-8"),
    )
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("scene_stats_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
