#!/usr/bin/env python3
"""Pins the RenderPacket source and CLI handoff surface."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_RENDER_PRIMITIVE_FIELDS = [
    ("uint32_t", "node"),
    ("uint32_t", "mesh"),
    ("uint32_t", "primitive"),
    ("Mat4", "world_transform"),
    ("MaterialKey", "material_key"),
]

EXPECTED_RENDER_PACKET_FIELDS = [
    ("std::vector<TransformedNode>", "transformed_nodes"),
    ("std::vector<RenderPrimitive>", "primitives"),
    ("std::vector<Diagnostic>", "diagnostics"),
]

EXPECTED_RENDER_PACKET_KEYS = [
    "transformed_nodes",
    "primitives",
    "diagnostics",
    "has_errors",
]

EXPECTED_PRIMITIVE_KEYS = [
    "node",
    "mesh",
    "primitive",
    "material",
    "feature_mask",
    "world_transform",
    "features",
]


def extract_struct_fields(header, struct_name):
    match = re.search(
        rf"struct\s+{re.escape(struct_name)}\s*\{{(?P<body>.*?)\n\}};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError(f"missing struct {struct_name}")

    fields = []
    for line in match.group("body").splitlines():
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


def require_subsequence(text, snippets, label):
    position = 0
    errors = []
    for snippet in snippets:
        next_position = text.find(snippet, position)
        if next_position == -1:
            errors.append(f"{label}: missing ordered snippet {snippet!r}")
        else:
            position = next_position + len(snippet)
    return errors


def verify_header(header):
    errors = []
    primitive_fields = extract_struct_fields(header, "RenderPrimitive")
    packet_fields = extract_struct_fields(header, "RenderPacket")
    if primitive_fields != EXPECTED_RENDER_PRIMITIVE_FIELDS:
        errors.append(
            f"RenderPrimitive fields mismatch: expected "
            f"{EXPECTED_RENDER_PRIMITIVE_FIELDS!r}, got {primitive_fields!r}"
        )
    if packet_fields != EXPECTED_RENDER_PACKET_FIELDS:
        errors.append(
            f"RenderPacket fields mismatch: expected "
            f"{EXPECTED_RENDER_PACKET_FIELDS!r}, got {packet_fields!r}"
        )
    if "return has_error_diagnostics(diagnostics);" not in header:
        errors.append("RenderPacket::has_errors no longer delegates to diagnostics")
    return errors


def verify_render_packet_source(source):
    snippets = [
        "auto validation = validate_scene_data(scene);",
        "if (has_error_diagnostics(packet.diagnostics))",
        "return packet;",
        "collect_node_world_transforms(scene, &graph_diagnostics)",
        "if (has_error_diagnostics(packet.diagnostics))",
        "return packet;",
        "if (primitive.positions.empty() || primitive.indices.empty())",
        "continue;",
        "derive_material_key(scene, primitive)",
        '"scene.render_packet_empty"',
    ]
    return require_subsequence(source, snippets, "build_render_packet")


def verify_inspect_source(source):
    snippets = [
        '"render_packet transformed_nodes="',
        '" primitives="',
        '" diagnostics="',
        '" has_errors="',
        '"primitive node="',
        '" mesh="',
        '" primitive="',
        '" material="',
        '" feature_mask="',
        '" world_transform="',
        '" features="',
        "join_feature_names(primitive.material_key)",
    ]
    return require_subsequence(source, snippets, "scene3d_inspect render-packet CLI")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--inspect-source", required=True, type=Path)
    args = parser.parse_args()

    header = args.header.read_text(encoding="utf-8")
    source = args.source.read_text(encoding="utf-8")
    inspect_source = args.inspect_source.read_text(encoding="utf-8")

    errors = []
    errors.extend(verify_header(header))
    errors.extend(verify_render_packet_source(source))
    errors.extend(verify_inspect_source(inspect_source))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("render_packet_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
