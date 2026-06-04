#!/usr/bin/env python3
"""Pins the CPU scene graph transform/traversal surface."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_STRUCT_FIELDS = {
    "RenderableNode": [
        ("uint32_t", "node"),
        ("uint32_t", "mesh"),
        ("Mat4", "world_transform"),
    ],
    "TransformedNode": [
        ("uint32_t", "node"),
        ("Mat4", "world_transform"),
    ],
}

EXPECTED_FUNCTIONS = [
    "Mat4 local_transform_for_node(const NodeData& node);",
    "Mat4 multiply(const Mat4& a, const Mat4& b);",
    "TransformedPoint transform_point(const Mat4& transform,",
    "std::vector<TransformedNode> collect_node_world_transforms(",
    "std::vector<RenderableNode> collect_renderable_nodes(",
]

EXPECTED_SOURCE_SNIPPETS = [
    "if (!is_valid_scene_index(node_index, scene.nodes.size()))",
    '"scene.graph_node_out_of_range"',
    "if (active[node_index] != 0u)",
    '"scene.graph_cycle"',
    "active[node_index] = 1u;",
    "const auto world = multiply(parent_transform, local_transform_for_node(node));",
    "TransformedNode{\n            node_index,\n            world,",
    "RenderableNode{\n            node_index,\n            node.mesh,\n            world,",
    "for (uint32_t child : node.children)",
    "active[node_index] = 0u;",
    "if (node.has_matrix_transform)",
    "out.m[i] = node.matrix[i];",
    "const float length = std::sqrt(x * x + y * y + z * z + w * w);",
    "const float qx = length > 0.0f ? x / length : 0.0f;",
    "const float qw = length > 0.0f ? w / length : 1.0f;",
    "out.m[12] = node.translation[0];",
    "out.m[15] = 1.0f;",
    "out.m[static_cast<size_t>(column) * 4u + static_cast<size_t>(row)] =",
    "transform.m[0] * x + transform.m[4] * y + transform.m[8] * z + transform.m[12]",
    "if (!scene.root_nodes.empty())",
    "for (uint32_t root : scene.root_nodes)",
    "for (uint32_t i = 0; i < scene.nodes.size(); ++i)",
]


def extract_struct_body(header, struct_name):
    match = re.search(
        rf"struct\s+{re.escape(struct_name)}\s*\{{(?P<body>.*?)\n\}};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError(f"missing struct {struct_name}")
    return match.group("body")


def extract_fields(header, struct_name):
    fields = []
    for line in extract_struct_body(header, struct_name).splitlines():
        stripped = line.strip()
        if not stripped or "{" in stripped or "(" in stripped:
            continue
        match = re.match(
            r"(?P<type>[A-Za-z0-9_:<>]+(?:\s+[A-Za-z0-9_:<>]+)*)\s+"
            r"(?P<name>[A-Za-z0-9_]+)(?:\s*=\s*[^;]+)?;",
            stripped,
        )
        if match:
            fields.append((match.group("type"), match.group("name")))
    return fields


def require_ordered_snippets(text, snippets, label):
    errors = []
    position = 0
    for snippet in snippets:
        found = text.find(snippet, position)
        if found == -1:
            errors.append(f"{label}: missing ordered snippet {snippet!r}")
        else:
            position = found + len(snippet)
    return errors


def verify(header, source):
    errors = []
    for struct_name, expected_fields in EXPECTED_STRUCT_FIELDS.items():
        fields = extract_fields(header, struct_name)
        if fields != expected_fields:
            errors.append(
                f"{struct_name} fields mismatch: expected "
                f"{expected_fields!r}, got {fields!r}"
            )

    for function in EXPECTED_FUNCTIONS:
        if function not in header:
            errors.append(f"missing public scene_graph declaration {function!r}")

    mat4_body = extract_struct_body(header, "Mat4")
    if "1.0f, 0.0f, 0.0f, 0.0f" not in mat4_body or "0.0f, 0.0f, 0.0f, 1.0f" not in mat4_body:
        errors.append("Mat4 identity initializer drifted")

    errors.extend(require_ordered_snippets(source, EXPECTED_SOURCE_SNIPPETS, "scene_graph"))
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

    print("scene_graph_surface_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
