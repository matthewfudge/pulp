#!/usr/bin/env python3
"""Verifies the Scene3D CMake opt-in boundary checker rejects drift."""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


SCENE3D_BUILD_PATHS = (
    "core/scene",
    "core/render/CMakeFiles/pulp-renderer3d-probe.dir",
    "core/render/CMakeFiles/pulp-scene3d-inspect-native.dir",
    "test/CMakeFiles/pulp-test-scene3d.dir",
    "test/CMakeFiles/pulp-test-renderer3d.dir",
)

SCENE3D_GENERATED_LINK_FILES = (
    "core/scene/CMakeFiles/pulp-scene3d-inspect.dir/link.txt",
    "core/scene/CMakeFiles/pulp-scene3d-sidecar.dir/link.txt",
    "core/scene/CMakeFiles/pulp-scene3d-bake-preflight.dir/link.txt",
    "core/render/CMakeFiles/pulp-renderer3d-probe.dir/link.txt",
    "core/render/CMakeFiles/pulp-scene3d-inspect-native.dir/link.txt",
    "test/CMakeFiles/pulp-test-scene3d.dir/link.txt",
    "test/CMakeFiles/pulp-test-renderer3d.dir/link.txt",
)


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_cache(build_dir, enabled):
    value = "ON" if enabled else "OFF"
    write_text(build_dir / "CMakeCache.txt",
               f"PULP_ENABLE_SCENE3D:BOOL={value}\n")


def load_cache_enabled(build_dir):
    cache = build_dir / "CMakeCache.txt"
    for line in cache.read_text(encoding="utf-8").splitlines():
        if line.startswith("PULP_ENABLE_SCENE3D:") and "=" in line:
            return line.split("=", 1)[1].upper() in {"1", "ON", "TRUE", "YES"}
    return False


def populate_enabled_scene3d_outputs(build_dir):
    for relative in SCENE3D_BUILD_PATHS:
        (build_dir / relative).mkdir(parents=True, exist_ok=True)
    for relative in SCENE3D_GENERATED_LINK_FILES:
        write_text(build_dir / relative, "scene3d link\n")


def add_disabled_build_path(build_dir):
    (build_dir / "core/scene").mkdir(parents=True, exist_ok=True)


def add_disabled_link_file(build_dir):
    write_text(build_dir / SCENE3D_GENERATED_LINK_FILES[0], "scene3d link\n")


def add_disabled_cli_target(build_dir):
    write_text(build_dir / "bin/pulp-scene3d-sidecar", "target\n")


def remove_enabled_build_path(build_dir):
    shutil.rmtree(build_dir / "core/scene")


def remove_enabled_link_file(build_dir):
    (build_dir / SCENE3D_GENERATED_LINK_FILES[-1]).unlink()


def run_verifier(verifier, build_dir, expect):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--build-dir",
            str(build_dir),
            "--expect",
            expect,
        ],
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
    print(f"scene3d_cmake_boundary_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify Scene3D CMake opt-in boundary drift is rejected.")
    parser.add_argument("--boundary-verifier", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()

    if not args.boundary_verifier.exists():
        print(f"boundary_verifier_exists=false path={args.boundary_verifier}")
        return 2
    if not (args.build_dir / "CMakeCache.txt").exists():
        print(f"build_cache_exists=false path={args.build_dir / 'CMakeCache.txt'}")
        return 2

    errors = []
    current_enabled = load_cache_enabled(args.build_dir)
    current_expect = "on" if current_enabled else "off"
    cases = [(f"valid-current-{current_expect}", args.build_dir, current_expect, 0,
              f"scene3d_cmake_boundary_verified={current_expect}")]

    with tempfile.TemporaryDirectory() as tmp:
        temp_root = Path(tmp)

        mismatch = temp_root / "expect-mismatch"
        write_cache(mismatch, enabled=False)
        cases.append(("expect-mismatch", mismatch, "on", 1,
                      "PULP_ENABLE_SCENE3D expected on, cache has off"))

        enabled_missing_path = temp_root / "enabled-missing-build-path"
        write_cache(enabled_missing_path, enabled=True)
        populate_enabled_scene3d_outputs(enabled_missing_path)
        remove_enabled_build_path(enabled_missing_path)
        cases.append(("enabled-missing-build-path", enabled_missing_path, "on", 1,
                      "expected Scene3D build path missing: core/scene"))

        enabled_missing_link = temp_root / "enabled-missing-link-file"
        write_cache(enabled_missing_link, enabled=True)
        populate_enabled_scene3d_outputs(enabled_missing_link)
        remove_enabled_link_file(enabled_missing_link)
        cases.append(("enabled-missing-link-file", enabled_missing_link, "on", 1,
                      "expected Scene3D generated link file missing"))

        valid_disabled = temp_root / "valid-disabled"
        write_cache(valid_disabled, enabled=False)
        cases.append(("valid-disabled", valid_disabled, "off", 0,
                      "scene3d_cmake_boundary_verified=off"))

        disabled_path = temp_root / "disabled-build-path-present"
        write_cache(disabled_path, enabled=False)
        add_disabled_build_path(disabled_path)
        cases.append(("disabled-build-path-present", disabled_path, "off", 1,
                      "Scene3D build path present while disabled: core/scene"))

        disabled_link = temp_root / "disabled-link-file-present"
        write_cache(disabled_link, enabled=False)
        add_disabled_link_file(disabled_link)
        cases.append(("disabled-link-file-present", disabled_link, "off", 1,
                      "Scene3D generated link file present while disabled"))

        disabled_target = temp_root / "disabled-target-present"
        write_cache(disabled_target, enabled=False)
        add_disabled_cli_target(disabled_target)
        cases.append(("disabled-target-present", disabled_target, "off", 1,
                      "Scene3D target present while disabled"))

        for name, build_dir, expect, expected_code, expected_text in cases:
            expect_case(
                name,
                run_verifier(args.boundary_verifier, build_dir, expect),
                expected_code,
                expected_text,
                errors,
            )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("scene3d_cmake_boundary_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
