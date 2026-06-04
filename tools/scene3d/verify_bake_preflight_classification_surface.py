#!/usr/bin/env python3
"""Pins bake_preflight.cpp's public unsupported-feature classification buckets."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_EXACT = {
    "is_export_blocker": {
        "ShaderMaterial",
        "RawShaderMaterial",
        "Postprocessing",
        "RenderTarget",
        "ArbitraryJSAnimation",
        "Physics",
        "EventHandler",
    },
    "is_texture_encoding_blocker": {
        "TextureEncoding",
    },
    "is_native_runtime_gap": {
        "TexturePayload",
        "TransformAnimation",
        "MaterialTexture:normalTangents",
        "MaterialTexture:normalScale",
        "MaterialTexture:occlusionStrength",
        "MaterialTextureTransform:nonBaseColor",
        "MaterialTexcoord:nonBaseColor",
        "MorphWeights",
        "MorphTargets",
        "Skinning",
        "GpuInstancing",
    },
}

EXPECTED_PREFIXES = {
    "is_export_blocker": set(),
    "is_texture_encoding_blocker": set(),
    "is_native_runtime_gap": {
        "TextureFormat:",
        "AnimationPath:",
        "MaterialExtension:",
        "PrimitiveMode:",
        "PunctualLight:",
        "Camera:",
    },
}


def extract_function_body(source, name):
    match = re.search(
        rf"bool\s+{re.escape(name)}\s*\([^)]*\)\s*\{{",
        source,
    )
    if not match:
        raise ValueError(f"missing function: {name}")

    start = match.end()
    depth = 1
    index = start
    while index < len(source):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index]
        index += 1

    raise ValueError(f"unterminated function: {name}")


def collect_surface(body):
    exact = set(re.findall(r'feature\s*==\s*"([^"]+)"', body))
    prefixes = set(re.findall(r'has_prefix\s*\(\s*feature\s*,\s*"([^"]+)"\s*\)', body))
    return exact, prefixes


def describe_set(values):
    if not values:
        return "[]"
    return "[" + ", ".join(sorted(values)) + "]"


def verify_function(source, name):
    body = extract_function_body(source, name)
    exact, prefixes = collect_surface(body)
    expected_exact = EXPECTED_EXACT[name]
    expected_prefixes = EXPECTED_PREFIXES[name]

    errors = []
    if exact != expected_exact:
        errors.append(
            f"{name} exact mismatch: expected {describe_set(expected_exact)}, "
            f"got {describe_set(exact)}"
        )
    if prefixes != expected_prefixes:
        errors.append(
            f"{name} prefix mismatch: expected {describe_set(expected_prefixes)}, "
            f"got {describe_set(prefixes)}"
        )
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    source = args.source.read_text(encoding="utf-8")
    errors = []
    for name in EXPECTED_EXACT:
        errors.extend(verify_function(source, name))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("bake_preflight_classification_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
