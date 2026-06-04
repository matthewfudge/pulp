#!/usr/bin/env python3
"""Verifies the SceneGraph surface checker rejects meaningful drift."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.write_text(text, encoding="utf-8")


def run_verifier(verifier, header, source):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--header",
            str(header),
            "--source",
            str(source),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def replace_once(text, old, new):
    if old not in text:
        raise ValueError(f"missing text to replace: {old!r}")
    return text.replace(old, new, 1)


def remove_once(text, snippet):
    return replace_once(text, snippet, "")


def drift_identity_initializer(header):
    return replace_once(
        header,
        "        0.0f, 0.0f, 0.0f, 1.0f,",
        "        0.0f, 0.0f, 0.0f, 0.0f,",
    )


def remove_renderable_mesh_field(header):
    return remove_once(header, "    uint32_t mesh = invalid_scene_index;\n")


def remove_public_transform_point(header):
    return remove_once(
        header,
        """TransformedPoint transform_point(const Mat4& transform,
                                 float x,
                                 float y,
                                 float z);

""",
    )


def drift_cycle_diagnostic(source):
    return replace_once(source, '"scene.graph_cycle"', '"scene.graph_loop"')


def drift_world_multiply_order(source):
    return replace_once(
        source,
        "const auto world = multiply(parent_transform, local_transform_for_node(node));",
        "const auto world = multiply(local_transform_for_node(node), parent_transform);",
    )


def remove_child_traversal(source):
    return remove_once(
        source,
        """    for (uint32_t child : node.children) {
        visit_node(scene,
                   child,
                   world,
                   active,
                   transformed_nodes,
                   renderables,
                   diagnostics);
    }
""",
    )


def drift_root_fallback(source):
    source = remove_once(
        source,
        """    for (uint32_t i = 0; i < scene.nodes.size(); ++i) {
        visit_node(scene,
                   i,
                   identity(),
                   active,
                   &transformed_nodes,
                   ignored_renderables,
                   diagnostics);
    }
""",
    )
    return remove_once(
        source,
        """    for (uint32_t i = 0; i < scene.nodes.size(); ++i) {
        visit_node(scene,
                   i,
                   identity(),
                   active,
                   nullptr,
                   renderables,
                   diagnostics);
    }
""",
    )


def drift_matrix_transform_copy(source):
    return replace_once(source, "out.m[i] = node.matrix[i];", "out.m[i] = 0.0f;")


def drift_quaternion_normalization(source):
    return replace_once(
        source,
        "const float qx = length > 0.0f ? x / length : 0.0f;",
        "const float qx = x;",
    )


def drift_transform_point_translation(source):
    return replace_once(
        source,
        "transform.m[0] * x + transform.m[4] * y + transform.m[8] * z + transform.m[12]",
        "transform.m[0] * x + transform.m[4] * y + transform.m[8] * z",
    )


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}"
        )
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}"
        )
    print(f"scene_graph_surface_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify SceneGraph surface checker rejects contract drift.")
    parser.add_argument("--surface-verifier", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("surface_verifier", args.surface_verifier),
            ("header", args.header),
            ("source", args.source)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    header_text = read_text(args.header)
    source_text = read_text(args.source)
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        header = tmpdir / "scene_graph.hpp"
        source = tmpdir / "scene_graph.cpp"

        def write_case(header_body=header_text, source_body=source_text):
            write_text(header, header_body)
            write_text(source, source_body)

        write_case()
        expect_case(
            "valid-current-scene-graph",
            run_verifier(args.surface_verifier, header, source),
            0,
            "scene_graph_surface_verified=true",
            errors,
        )

        write_case(header_body=drift_identity_initializer(header_text))
        expect_case(
            "identity-initializer-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "Mat4 identity initializer drifted",
            errors,
        )

        write_case(header_body=remove_renderable_mesh_field(header_text))
        expect_case(
            "renderable-field-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "RenderableNode fields mismatch",
            errors,
        )

        write_case(header_body=remove_public_transform_point(header_text))
        expect_case(
            "public-function-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "missing public scene_graph declaration",
            errors,
        )

        write_case(source_body=drift_cycle_diagnostic(source_text))
        expect_case(
            "cycle-diagnostic-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_world_multiply_order(source_text))
        expect_case(
            "world-multiply-order-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=remove_child_traversal(source_text))
        expect_case(
            "child-traversal-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_root_fallback(source_text))
        expect_case(
            "root-fallback-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_matrix_transform_copy(source_text))
        expect_case(
            "matrix-copy-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_quaternion_normalization(source_text))
        expect_case(
            "quaternion-normalization-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

        write_case(source_body=drift_transform_point_translation(source_text))
        expect_case(
            "transform-point-translation-drift",
            run_verifier(args.surface_verifier, header, source),
            1,
            "scene_graph: missing ordered snippet",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("scene_graph_surface_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
