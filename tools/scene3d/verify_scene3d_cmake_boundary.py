#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path


SCENE3D_BUILD_PATHS = [
    "core/scene",
    "core/render/CMakeFiles/pulp-renderer3d-probe.dir",
    "core/render/CMakeFiles/pulp-scene3d-inspect-native.dir",
    "test/CMakeFiles/pulp-test-scene3d.dir",
    "test/CMakeFiles/pulp-test-renderer3d.dir",
]

SCENE3D_CLI_TARGET_NAMES = [
    "pulp-scene3d-inspect",
    "pulp-scene3d-sidecar",
    "pulp-scene3d-bake-preflight",
    "pulp-renderer3d-probe",
    "pulp-scene3d-inspect-native",
]

SCENE3D_GENERATED_LINK_FILES = [
    "core/scene/CMakeFiles/pulp-scene3d-inspect.dir/link.txt",
    "core/scene/CMakeFiles/pulp-scene3d-sidecar.dir/link.txt",
    "core/scene/CMakeFiles/pulp-scene3d-bake-preflight.dir/link.txt",
    "core/render/CMakeFiles/pulp-renderer3d-probe.dir/link.txt",
    "core/render/CMakeFiles/pulp-scene3d-inspect-native.dir/link.txt",
    "test/CMakeFiles/pulp-test-scene3d.dir/link.txt",
    "test/CMakeFiles/pulp-test-renderer3d.dir/link.txt",
]


def load_cache(build_dir: Path):
    cache = build_dir / "CMakeCache.txt"
    try:
        text = cache.read_text(encoding="utf-8")
    except OSError as exc:
        raise RuntimeError(f"{cache}: {exc}") from exc
    values = {}
    for line in text.splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key_type, value = line.split("=", 1)
        key = key_type.split(":", 1)[0]
        values[key] = value
    return values


def cache_bool(values, key):
    return values.get(key, "").upper() in {"1", "ON", "TRUE", "YES"}


def path_exists(build_dir: Path, relative: str):
    return (build_dir / relative).exists()


def find_cli_target(build_dir: Path, target: str):
    matches = list(build_dir.glob(f"**/{target}"))
    return [path for path in matches if path.is_file()]


def main():
    parser = argparse.ArgumentParser(
        description="Verify the PULP_ENABLE_SCENE3D CMake opt-in boundary.")
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--expect", choices=("on", "off"), required=True)
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    try:
        cache = load_cache(build_dir)
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 2

    actual_enabled = cache_bool(cache, "PULP_ENABLE_SCENE3D")
    expected_enabled = args.expect == "on"
    errors = []
    if actual_enabled != expected_enabled:
        errors.append(
            f"PULP_ENABLE_SCENE3D expected {args.expect}, "
            f"cache has {'on' if actual_enabled else 'off'}")

    if expected_enabled:
        for relative in SCENE3D_BUILD_PATHS:
            if not path_exists(build_dir, relative):
                errors.append(f"expected Scene3D build path missing: {relative}")
        for relative in SCENE3D_GENERATED_LINK_FILES:
            if not path_exists(build_dir, relative):
                errors.append(
                    f"expected Scene3D generated link file missing: {relative}")
    else:
        for relative in SCENE3D_BUILD_PATHS:
            if path_exists(build_dir, relative):
                errors.append(f"Scene3D build path present while disabled: {relative}")
        for relative in SCENE3D_GENERATED_LINK_FILES:
            if path_exists(build_dir, relative):
                errors.append(
                    f"Scene3D generated link file present while disabled: {relative}")
        for target in SCENE3D_CLI_TARGET_NAMES:
            matches = find_cli_target(build_dir, target)
            if matches:
                joined = ", ".join(str(path.relative_to(build_dir))
                                   for path in matches)
                errors.append(
                    f"Scene3D target present while disabled: {target}: {joined}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"scene3d_cmake_boundary_verified={args.expect}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
