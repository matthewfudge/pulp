#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path


RUNTIME_FORBIDDEN_PATTERNS = [
    (re.compile(r"#\s*include\s*[<\"][^>\"]*pulp/view/[^>\"]*[>\"]"),
     "view module include"),
    (re.compile(r"\bpulp::view\b"),
     "view module namespace"),
    (re.compile(r"\bWidgetBridge\b|\bwidget_bridge\b"),
     "WidgetBridge runtime bridge"),
    (re.compile(r"\bweb-compat\b"),
     "web-compat runtime asset"),
    (re.compile(r"\b(GLTFLoader|TextureLoader|DRACOLoader|KTX2Loader|GLTFExporter)\b"),
     "live Three.js loader/exporter"),
    (re.compile(r"\bthree\.webgpu\b|\bthree\.core\b|\bthree\.iife\b"),
     "live Three.js runtime asset"),
    (re.compile(r"\b(v8::|quickjs|qjs_|JSContextRef|JSGlobalContextRef|JavaScriptCore)\b"),
     "JS engine surface"),
]

PARSER_PATTERNS = [
    (re.compile(r"#\s*include\s*[<\"][^>\"]*(fastgltf|cgltf|tinygltf)[^>\"]*[>\"]"),
     "glTF parser include"),
    (re.compile(r"\b(fastgltf|cgltf|tinygltf)::"),
     "glTF parser namespace"),
]

GPU_PATTERNS = [
    (re.compile(r"#\s*include\s*[<\"][^>\"]*(webgpu|dawn|wgpu)[^>\"]*[>\"]"),
     "Dawn/WebGPU include"),
    (re.compile(r"\b(wgpu::|WGPU[A-Za-z_]*|dawn::)"),
     "Dawn/WebGPU type"),
    (re.compile(r"#\s*include\s*[<\"][^>\"]*(Sk[A-Za-z_]*|skia)[^>\"]*[>\"]"),
     "Skia include"),
    (re.compile(r"\bSk[A-Z][A-Za-z_]*"),
     "Skia type"),
]

SCENE_SOURCE_ALLOWLIST = [
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
]

RENDER_SOURCE_ALLOWLIST = [
    "core/render/src/draco_decoder.cpp",
    "core/render/src/renderer3d.cpp",
    "core/render/src/renderer3d_probe.cpp",
]


def strip_noncode(text: str):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    text = re.sub(r'R"([A-Za-z_][A-Za-z0-9_]*)?\(.*?\)\1"', '""',
                  text, flags=re.DOTALL)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text, flags=re.DOTALL)
    text = re.sub(r"'(?:\\.|[^'\\])*'", "''", text, flags=re.DOTALL)
    return text


def scan_patterns(path: Path, text: str, patterns):
    errors = []
    for pattern, reason in patterns:
        match = pattern.search(text)
        if match:
            errors.append(f"{path}: forbidden {reason}: {match.group(0)!r}")
    return errors


def verify_expected_files(repo_root: Path):
    expected = {
        repo_root / relative
        for relative in SCENE_SOURCE_ALLOWLIST + RENDER_SOURCE_ALLOWLIST
    }
    discovered = set((repo_root / "core/scene/src").glob("*.cpp"))
    discovered.update([
        repo_root / relative for relative in RENDER_SOURCE_ALLOWLIST
    ])

    errors = []
    for path in sorted(expected - discovered):
        errors.append(f"expected native Scene3D source missing: {path}")
    for path in sorted(discovered - expected):
        errors.append(f"unexpected native Scene3D source: {path}")
    return errors


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Verify native Scene3D/Renderer3D implementation files do not "
            "cross into live Three.js/runtime-owned surfaces."))
    parser.add_argument("--repo-root", type=Path, default=Path("."))
    parser.add_argument("--source", action="append", type=Path,
                        help="Source to scan. Defaults to native Scene3D and Renderer3D sources.")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    sources = args.source or [
        repo_root / relative
        for relative in SCENE_SOURCE_ALLOWLIST + RENDER_SOURCE_ALLOWLIST
    ]

    errors = [] if args.source else verify_expected_files(repo_root)
    checked = []
    for source in sources:
        path = source if source.is_absolute() else repo_root / source
        try:
            text = strip_noncode(path.read_text(encoding="utf-8"))
        except OSError as exc:
            errors.append(f"{path}: {exc}")
            continue

        checked.append(path)
        relative = path.relative_to(repo_root).as_posix()
        errors.extend(scan_patterns(path, text, RUNTIME_FORBIDDEN_PATTERNS))

        if relative == "core/scene/src/gltf_loader.cpp":
            errors.extend(scan_patterns(path, text, GPU_PATTERNS))
        elif relative.startswith("core/scene/src/"):
            errors.extend(scan_patterns(path, text, PARSER_PATTERNS))
            errors.extend(scan_patterns(path, text, GPU_PATTERNS))
        elif relative.startswith("core/render/src/"):
            errors.extend(scan_patterns(path, text, PARSER_PATTERNS))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"native_source_boundary_verified={len(checked)} sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
