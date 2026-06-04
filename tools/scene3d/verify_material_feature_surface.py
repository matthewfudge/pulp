#!/usr/bin/env python3
"""Pins MaterialFeature bit assignments and exported feature-name coverage."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_FEATURES = [
    ("normals", 0, "normals"),
    ("texcoord0", 1, "texcoord0"),
    ("indexed", 2, "indexed"),
    ("base_color_texture", 3, "base_color_texture"),
    ("unlit", 4, "unlit"),
    ("double_sided", 5, "double_sided"),
    ("alpha_blend", 6, "alpha_blend"),
    ("material_fallback", 7, "material_fallback"),
    ("metallic_roughness_texture", 8, "metallic_roughness_texture"),
    ("normal_texture", 9, "normal_texture"),
    ("occlusion_texture", 10, "occlusion_texture"),
    ("emissive_texture", 11, "emissive_texture"),
    ("alpha_mask", 12, "alpha_mask"),
    ("tangents", 13, "tangents"),
    ("texcoord1", 14, "texcoord1"),
    ("color0", 15, "color0"),
    ("base_color_texture_transform", 16, "base_color_texture_transform"),
    ("non_base_color_texture_transform", 17, "non_base_color_texture_transform"),
    ("non_base_color_texcoord1", 18, "non_base_color_texcoord1"),
    ("normal_scale", 19, "normal_scale"),
    ("occlusion_strength", 20, "occlusion_strength"),
]


def extract_enum(header):
    match = re.search(
        r"enum\s+class\s+MaterialFeature\s*:\s*uint32_t\s*\{(?P<body>.*?)\};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing MaterialFeature enum")

    entries = []
    for name, shift in re.findall(
        r"([A-Za-z0-9_]+)\s*=\s*1u\s*<<\s*([0-9]+)u",
        match.group("body"),
    ):
        entries.append((name, int(shift)))
    return entries


def extract_name_surface(source):
    body_match = re.search(
        r"std::vector<const char\*>\s+material_feature_names\s*"
        r"\(\s*const\s+MaterialKey&\s+key\s*\)\s*\{(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    if not body_match:
        raise ValueError("missing material_feature_names body")

    body = body_match.group("body")
    names = []
    for feature, name in re.findall(
        r"append_feature_name\s*\(\s*key\s*,\s*MaterialFeature::([A-Za-z0-9_]+)\s*,\s*"
        r"\"([^\"]+)\"",
        body,
    ):
        names.append((feature, name))
    return names


def describe(values):
    return "[" + ", ".join(str(value) for value in values) + "]"


def verify(header_path, source_path):
    header = header_path.read_text(encoding="utf-8")
    source = source_path.read_text(encoding="utf-8")

    expected_enum = [(feature, shift) for feature, shift, _ in EXPECTED_FEATURES]
    expected_names = [(feature, name) for feature, _, name in EXPECTED_FEATURES]
    actual_enum = extract_enum(header)
    actual_names = extract_name_surface(source)

    errors = []
    if actual_enum != expected_enum:
        errors.append(
            f"MaterialFeature enum mismatch: expected {describe(expected_enum)}, "
            f"got {describe(actual_enum)}"
        )
    if actual_names != expected_names:
        errors.append(
            f"material_feature_names mismatch: expected {describe(expected_names)}, "
            f"got {describe(actual_names)}"
        )

    feature_names = [feature for feature, _ in actual_enum]
    if len(feature_names) != len(set(feature_names)):
        errors.append("MaterialFeature enum contains duplicate feature names")
    emitted_features = [feature for feature, _ in actual_names]
    if len(emitted_features) != len(set(emitted_features)):
        errors.append("material_feature_names contains duplicate feature names")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    errors = verify(args.header, args.source)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("material_feature_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
