#!/usr/bin/env python3
"""Verifies the native source-boundary checker rejects ownership drift."""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


SCENE_SOURCES = (
    "core/scene/src/bake_preflight.cpp",
    "core/scene/src/gltf_loader.cpp",
    "core/scene/src/material_key.cpp",
    "core/scene/src/render_packet.cpp",
    "core/scene/src/renderer3d_characterization.cpp",
    "core/scene/src/scene3d_bake_preflight.cpp",
    "core/scene/src/scene3d_inspect.cpp",
    "core/scene/src/scene3d_sidecar.cpp",
    "core/scene/src/scene_graph.cpp",
    "core/scene/src/scene_stats.cpp",
    "core/scene/src/sidecar.cpp",
)

RENDER_SOURCES = (
    "core/render/src/draco_decoder.cpp",
    "core/render/src/renderer3d.cpp",
    "core/render/src/renderer3d_probe.cpp",
)

ALL_SOURCES = SCENE_SOURCES + RENDER_SOURCES


def copy_sources(source_root, target_root):
    for relative in ALL_SOURCES:
        src = source_root / relative
        dst = target_root / relative
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def append_to_source(repo_root, relative, token):
    path = repo_root / relative
    write_text(path, read_text(path) + f"\n{token}\n")


def add_unexpected_scene_source(repo_root):
    write_text(repo_root / "core/scene/src/live_bridge.cpp", "int drift = 0;\n")


def remove_expected_scene_source(repo_root):
    (repo_root / "core/scene/src/sidecar.cpp").unlink()


def mutate_view_include(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/sidecar.cpp",
                     '#include <pulp/view/widget_bridge.hpp>')


def mutate_widget_bridge(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/scene_graph.cpp",
                     "WidgetBridge* drift;")


def mutate_js_engine(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/bake_preflight.cpp",
                     "v8::Isolate* drift;")


def mutate_live_loader(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/sidecar.cpp",
                     "GLTFLoader drift;")


def mutate_parser_leak_to_scene_surface(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/sidecar.cpp",
                     "fastgltf::Asset drift;")


def mutate_gpu_leak_to_scene_surface(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/render_packet.cpp",
                     "wgpu::Device drift;")


def mutate_parser_leak_to_renderer(repo_root):
    append_to_source(repo_root,
                     "core/render/src/renderer3d.cpp",
                     "fastgltf::Asset drift;")


def mutate_gpu_allowed_in_loader(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/gltf_loader.cpp",
                     "wgpu::Device allowed;")


def mutate_parser_allowed_in_loader(repo_root):
    append_to_source(repo_root,
                     "core/scene/src/gltf_loader.cpp",
                     "fastgltf::Asset allowed;")


def mutate_gpu_allowed_in_renderer(repo_root):
    append_to_source(repo_root,
                     "core/render/src/renderer3d.cpp",
                     "wgpu::Device allowed;")


def run_verifier(verifier, repo_root):
    return subprocess.run(
        [sys.executable, str(verifier), "--repo-root", str(repo_root)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
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
    print(f"native_source_boundary_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify native source-boundary drift is rejected.")
    parser.add_argument("--boundary-verifier", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()

    if not args.boundary_verifier.exists():
        print(f"boundary_verifier_exists=false path={args.boundary_verifier}")
        return 2
    for relative in ALL_SOURCES:
        if not (args.repo_root / relative).exists():
            print(f"native_boundary_source_exists=false path={args.repo_root / relative}")
            return 2

    cases = [
        ("valid-current-source-boundary", None, 0,
         "native_source_boundary_verified=14 sources"),
        ("unexpected-source", add_unexpected_scene_source, 1,
         "unexpected native Scene3D source"),
        ("missing-source", remove_expected_scene_source, 1,
         "expected native Scene3D source missing"),
        ("view-include", mutate_view_include, 1,
         "forbidden view module include"),
        ("widget-bridge", mutate_widget_bridge, 1,
         "forbidden WidgetBridge runtime bridge"),
        ("js-engine", mutate_js_engine, 1,
         "forbidden JS engine surface"),
        ("live-loader", mutate_live_loader, 1,
         "forbidden live Three.js loader/exporter"),
        ("scene-parser-leak", mutate_parser_leak_to_scene_surface, 1,
         "forbidden glTF parser namespace"),
        ("scene-gpu-leak", mutate_gpu_leak_to_scene_surface, 1,
         "forbidden Dawn/WebGPU type"),
        ("renderer-parser-leak", mutate_parser_leak_to_renderer, 1,
         "forbidden glTF parser namespace"),
        ("loader-parser-allowed", mutate_parser_allowed_in_loader, 0,
         "native_source_boundary_verified=14 sources"),
        ("loader-gpu-rejected", mutate_gpu_allowed_in_loader, 1,
         "forbidden Dawn/WebGPU type"),
        ("renderer-gpu-allowed", mutate_gpu_allowed_in_renderer, 0,
         "native_source_boundary_verified=14 sources"),
    ]
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        temp_root = Path(tmp)
        for name, mutate, expected_code, expected_text in cases:
            case_root = temp_root / name
            copy_sources(args.repo_root, case_root)
            if mutate is not None:
                mutate(case_root)
            expect_case(
                name,
                run_verifier(args.boundary_verifier, case_root),
                expected_code,
                expected_text,
                errors,
            )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("native_source_boundary_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
