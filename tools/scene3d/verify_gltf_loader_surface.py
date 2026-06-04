#!/usr/bin/env python3
"""Pins the native glTF loader API, private parser boundary, and load pipeline."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_DRACO_ATTRIBUTE_FIELDS = [
    ("int", "position", "-1"),
    ("int", "normal", "-1"),
    ("int", "texcoord0", "-1"),
    ("int", "texcoord1", "-1"),
    ("int", "tangent", "-1"),
    ("int", "color0", "-1"),
]

EXPECTED_DRACO_DECODED_FIELDS = [
    ("std::vector<float>", "positions", None),
    ("std::vector<float>", "normals", None),
    ("std::vector<float>", "texcoord0", None),
    ("std::vector<float>", "texcoord1", None),
    ("std::vector<float>", "tangents", None),
    ("std::vector<float>", "color0", None),
    ("std::vector<uint32_t>", "indices", None),
    ("bool", "success", "false"),
    ("bool", "decoder_available", "true"),
]

EXPECTED_LOAD_OPTIONS_FIELDS = [
    ("bool", "allow_draco", "true"),
    ("DracoDecodeCallback", "draco_decode", None),
]

EXPECTED_LOAD_RESULT_FIELDS = [
    ("SceneData", "scene", None),
    ("bool", "success", "false"),
    ("std::string", "error", None),
]

EXPECTED_DIAGNOSTIC_CODES = [
    "gltf.image_unsupported_source",
    "gltf.material_extension_deferred",
    "gltf.animation_sampler_out_of_range",
    "gltf.animation_missing_target_node",
    "gltf.animation_path_unsupported",
    "gltf.animation_accessor_unsupported",
    "gltf.primitive_missing_position",
    "gltf.primitive_missing_indices",
    "gltf.primitive_unsupported_index_type",
    "gltf.draco_disabled",
    "gltf.draco_not_wired",
    "gltf.draco_buffer_view_invalid",
    "gltf.draco_unavailable",
    "gltf.draco_decode_failed",
    "gltf.mesh_morph_weights_unsupported",
    "gltf.primitive_morph_targets_unsupported",
    "gltf.primitive_unsupported_mode",
    "gltf.node_skin_unsupported",
    "gltf.node_morph_weights_unsupported",
    "gltf.node_instancing_unsupported",
    "gltf.read_failed",
    "gltf.parse_failed",
]

EXPECTED_LOAD_PIPELINE = [
    "load_textures(asset, result.scene, path);",
    "load_texture_samplers(asset, result.scene);",
    "load_materials(asset, result.scene, path);",
    "load_cameras(asset, result.scene);",
    "load_lights(asset, result.scene);",
    "load_animations(asset, result.scene, path);",
    "load_meshes(asset, result.scene, options, path);",
    "load_nodes_and_roots(asset, result.scene, path);",
    "append_validation_diagnostics(result.scene, path);",
]

EXPECTED_FASTGLTF_EXTENSIONS = [
    "KHR_texture_transform",
    "EXT_mesh_gpu_instancing",
    "KHR_lights_punctual",
    "KHR_materials_anisotropy",
    "KHR_materials_clearcoat",
    "KHR_materials_diffuse_transmission",
    "KHR_materials_dispersion",
    "KHR_materials_emissive_strength",
    "KHR_materials_ior",
    "KHR_materials_iridescence",
    "KHR_materials_sheen",
    "KHR_materials_specular",
    "KHR_materials_transmission",
    "KHR_materials_unlit",
    "KHR_materials_volume",
    "KHR_draco_mesh_compression",
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
            r"(?P<name>[A-Za-z0-9_]+)(?:\s*=\s*(?P<default>[^;]+))?;",
            stripped,
        )
        if field_match:
            default = field_match.group("default")
            fields.append((
                field_match.group("type"),
                field_match.group("name"),
                default.strip() if default is not None else None,
            ))
    return fields


def extract_function_body(source, function_signature):
    start = source.find(function_signature)
    if start < 0:
        raise ValueError(f"missing function {function_signature}")
    brace = source.find("{", start)
    if brace < 0:
        raise ValueError(f"missing body for {function_signature}")
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise ValueError(f"unterminated body for {function_signature}")


def require_ordered_substrings(text, substrings):
    index = 0
    missing = []
    for substring in substrings:
        found = text.find(substring, index)
        if found < 0:
            missing.append(substring)
        else:
            index = found + len(substring)
    return missing


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def verify_header(header):
    errors = []
    header_code = strip_comments(header)
    if "fastgltf" in header_code or "cgltf" in header_code or "tinygltf" in header_code:
        errors.append("gltf_loader.hpp leaked parser implementation symbols")
    if "<pulp/scene/scene_data.hpp>" not in header:
        errors.append("gltf_loader.hpp no longer includes SceneData boundary header")

    expected_structs = [
        ("DracoAttributeIds", EXPECTED_DRACO_ATTRIBUTE_FIELDS),
        ("DracoDecodedMesh", EXPECTED_DRACO_DECODED_FIELDS),
        ("LoadOptions", EXPECTED_LOAD_OPTIONS_FIELDS),
        ("LoadResult", EXPECTED_LOAD_RESULT_FIELDS),
    ]
    for struct_name, expected in expected_structs:
        try:
            fields = extract_fields(header, struct_name)
        except ValueError as exc:
            errors.append(str(exc))
            continue
        if fields != expected:
            errors.append(
                f"{struct_name} fields mismatch: expected {expected!r}, got {fields!r}"
            )

    callback_pattern = re.compile(
        r"using\s+DracoDecodeCallback\s*=\s*std::function<DracoDecodedMesh\s*\(\s*"
        r"const\s+uint8_t\*\s+data\s*,\s*size_t\s+size\s*,\s*"
        r"const\s+DracoAttributeIds&\s+attribute_ids\s*\)>;",
        re.DOTALL,
    )
    if not callback_pattern.search(header):
        errors.append("DracoDecodeCallback signature no longer matches native boundary")

    load_pattern = re.compile(
        r"LoadResult\s+load_gltf_scene\s*\(\s*"
        r"const\s+std::filesystem::path&\s+path\s*,\s*"
        r"const\s+LoadOptions&\s+options\s*=\s*\{\}\s*\)\s*;",
        re.DOTALL,
    )
    if not load_pattern.search(header):
        errors.append("load_gltf_scene declaration drifted")

    return errors


def verify_source(source):
    errors = []
    for include in ["#include <fastgltf/core.hpp>", "#include <fastgltf/tools.hpp>"]:
        if include not in source:
            errors.append(f"missing private parser include: {include}")

    diagnostic_codes = re.findall(r'"(gltf\.[^"]+)"', source)
    if diagnostic_codes != EXPECTED_DIAGNOSTIC_CODES:
        errors.append(
            f"loader diagnostic code sequence mismatch: expected "
            f"{EXPECTED_DIAGNOSTIC_CODES!r}, got {diagnostic_codes!r}"
        )

    try:
        body = extract_function_body(
            source,
            "LoadResult load_gltf_scene(const std::filesystem::path& path,",
        )
    except ValueError as exc:
        errors.append(str(exc))
        body = ""

    if body:
        missing_pipeline = require_ordered_substrings(body, EXPECTED_LOAD_PIPELINE)
        if missing_pipeline:
            errors.append(f"load_gltf_scene pipeline drifted: missing {missing_pipeline!r}")
        for option in [
            "fastgltf::Options::GenerateMeshIndices",
            "fastgltf::Options::LoadExternalBuffers",
            "fastgltf::Options::LoadExternalImages",
        ]:
            if option not in body:
                errors.append(f"parser option missing from load_gltf_scene: {option}")
        for extension in EXPECTED_FASTGLTF_EXTENSIONS:
            if f"fastgltf::Extensions::{extension}" not in body:
                errors.append(f"parser extension missing from load_gltf_scene: {extension}")
        if "result.success = !has_error_diagnostics(result.scene.diagnostics);" not in body:
            errors.append("load_gltf_scene success no longer reflects diagnostics")
        if 'result.error = "Loaded glTF scene has structural diagnostics.";' not in body:
            errors.append("load_gltf_scene structural diagnostic error text drifted")

    try:
        draco_body = extract_function_body(source, "bool decode_draco_primitive(")
    except ValueError as exc:
        errors.append(str(exc))
        draco_body = ""

    if draco_body:
        missing_draco = require_ordered_substrings(
            draco_body,
            [
                "if (!gltf_primitive.dracoCompression) {",
                "if (!options.allow_draco) {",
                "if (!options.draco_decode) {",
                "copy_buffer_view_bytes(asset,",
                "auto decoded = options.draco_decode(",
                "draco_attribute_ids_from_gltf(*gltf_primitive.dracoCompression)",
                "if (!decoded.decoder_available) {",
                "if (!decoded.success) {",
                "primitive.positions = std::move(decoded.positions);",
                "primitive.indices = std::move(decoded.indices);",
            ],
        )
        if missing_draco:
            errors.append(f"Draco decode flow drifted: missing {missing_draco!r}")

    for attribute_name in [
        "POSITION",
        "NORMAL",
        "TEXCOORD_0",
        "TEXCOORD_1",
        "TANGENT",
        "COLOR_0",
    ]:
        if f'draco_attribute_id(draco, "{attribute_name}")' not in source:
            errors.append(f"Draco attribute map missing {attribute_name}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()

    errors = []
    errors.extend(verify_header(args.header.read_text(encoding="utf-8")))
    errors.extend(verify_source(args.source.read_text(encoding="utf-8")))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("gltf_loader_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
