#!/usr/bin/env python3
"""Pins the public SceneData surface and validation diagnostic contract."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_PRIMITIVE_FIELDS = [
    ("std::vector<float>", "positions"),
    ("std::vector<float>", "normals"),
    ("std::vector<float>", "tangents"),
    ("std::vector<float>", "texcoord0"),
    ("std::vector<float>", "texcoord1"),
    ("std::vector<float>", "color0"),
    ("std::vector<uint32_t>", "indices"),
    ("uint32_t", "material"),
]

EXPECTED_SCENE_FIELDS = [
    ("std::vector<NodeData>", "nodes"),
    ("std::vector<uint32_t>", "root_nodes"),
    ("std::vector<MeshData>", "meshes"),
    ("std::vector<MaterialData>", "materials"),
    ("std::vector<TextureData>", "textures"),
    ("std::vector<TextureSamplerData>", "texture_samplers"),
    ("std::vector<CameraData>", "cameras"),
    ("std::vector<LightData>", "lights"),
    ("std::vector<AnimationData>", "animations"),
    ("std::vector<UnsupportedFeatureData>", "unsupported_features"),
    ("std::vector<Diagnostic>", "diagnostics"),
]

EXPECTED_VALIDATION_CODES = [
    "scene.root_node_out_of_range",
    "scene.node_mesh_out_of_range",
    "scene.node_camera_out_of_range",
    "scene.node_light_out_of_range",
    "scene.node_child_out_of_range",
    "scene.material_texture_out_of_range",
    "scene.material_texture_out_of_range",
    "scene.material_texture_out_of_range",
    "scene.material_texture_out_of_range",
    "scene.material_texture_out_of_range",
    "scene.material_sampler_out_of_range",
    "scene.material_sampler_out_of_range",
    "scene.material_sampler_out_of_range",
    "scene.material_sampler_out_of_range",
    "scene.material_sampler_out_of_range",
    "scene.animation_node_out_of_range",
    "scene.animation_sampler_out_of_range",
    "scene.animation_sampler_missing_input",
    "scene.animation_sampler_invalid_output",
    "scene.animation_sampler_count_mismatch",
    "scene.mesh_without_primitives",
    "scene.primitive_invalid_positions",
    "scene.primitive_normal_count_mismatch",
    "scene.primitive_tangent_count_mismatch",
    "scene.primitive_texcoord_count_mismatch",
    "scene.primitive_texcoord_count_mismatch",
    "scene.primitive_color_count_mismatch",
    "scene.primitive_missing_indices",
    "scene.primitive_index_out_of_range",
    "scene.primitive_material_out_of_range",
]


def extract_struct_body(source, struct_name):
    match = re.search(
        rf"struct\s+{re.escape(struct_name)}\s*\{{(?P<body>.*?)\n\}};",
        source,
        re.DOTALL,
    )
    if not match:
        raise ValueError(f"missing struct {struct_name}")
    return match.group("body")


def extract_fields(source, struct_name):
    body = extract_struct_body(source, struct_name)
    fields = []
    for line in body.splitlines():
        stripped = line.strip()
        if not stripped or "(" in stripped or "{" in stripped:
            continue
        field_match = re.match(
            r"(?P<type>[A-Za-z0-9_:<>]+(?:\s+[A-Za-z0-9_:<>]+)*)\s+"
            r"(?P<name>[A-Za-z0-9_]+)(?:\s*=\s*[^;]+)?;",
            stripped,
        )
        if field_match:
            fields.append((field_match.group("type"), field_match.group("name")))
    return fields


def extract_validation_codes(source):
    match = re.search(
        r"validate_scene_data\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}",
        source,
        re.DOTALL,
    )
    if not match:
        raise ValueError("missing validate_scene_data body")
    return re.findall(r'"(scene\.[^"]+)"', match.group("body"))


def verify(source):
    errors = []
    primitive_fields = extract_fields(source, "PrimitiveData")
    scene_fields = extract_fields(source, "SceneData")
    validation_codes = extract_validation_codes(source)

    if primitive_fields != EXPECTED_PRIMITIVE_FIELDS:
        errors.append(
            f"PrimitiveData fields mismatch: expected "
            f"{EXPECTED_PRIMITIVE_FIELDS!r}, got {primitive_fields!r}"
        )
    if scene_fields != EXPECTED_SCENE_FIELDS:
        errors.append(
            f"SceneData fields mismatch: expected "
            f"{EXPECTED_SCENE_FIELDS!r}, got {scene_fields!r}"
        )
    if validation_codes != EXPECTED_VALIDATION_CODES:
        errors.append(
            f"validate_scene_data diagnostic codes mismatch: expected "
            f"{EXPECTED_VALIDATION_CODES!r}, got {validation_codes!r}"
        )
    if "return nodes.empty() && meshes.empty();" not in source:
        errors.append("SceneData::empty no longer reflects nodes+meshes emptiness")
    if "return index != invalid_scene_index && static_cast<size_t>(index) < count;" not in source:
        errors.append("is_valid_scene_index contract drifted")
    if "Diagnostic::Severity::error: return \"error\";" not in source:
        errors.append("diagnostic_severity_name no longer emits error")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    args = parser.parse_args()

    source = args.header.read_text(encoding="utf-8")
    errors = verify(source)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("scene_data_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
