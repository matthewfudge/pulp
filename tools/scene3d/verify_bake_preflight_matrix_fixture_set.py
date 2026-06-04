#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def copy_fixtures(source_dir, target_dir):
    for path in source_dir.glob("*.pulp3d.json"):
        shutil.copy2(path, target_dir / path.name)


def run_matrix_verifier(matrix_verifier, preflight_tool, fixture_dir):
    return subprocess.run(
        [
            sys.executable,
            str(matrix_verifier),
            "--preflight-tool",
            str(preflight_tool),
            "--fixture-dir",
            str(fixture_dir),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_failure(name, result, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != 1:
        errors.append(
            f"{name}: expected exit 1, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected output containing {expected_text!r}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake-preflight matrix rejects fixture-set drift.")
    parser.add_argument("--matrix-verifier", type=Path, required=True)
    parser.add_argument("--preflight-tool", type=Path, required=True)
    parser.add_argument("--fixture-dir", type=Path, required=True)
    args = parser.parse_args()

    if not args.matrix_verifier.exists():
        print(f"matrix_verifier_exists=false path={args.matrix_verifier}")
        return 2
    if not args.preflight_tool.exists():
        print(f"preflight_tool_exists=false path={args.preflight_tool}")
        return 2
    if not args.fixture_dir.exists():
        print(f"fixture_dir_exists=false path={args.fixture_dir}")
        return 2

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        unlisted_dir = temp_path / "unlisted"
        unlisted_dir.mkdir()
        copy_fixtures(args.fixture_dir, unlisted_dir)
        shutil.copy2(
            unlisted_dir / "clean.pulp3d.json",
            unlisted_dir / "unlisted-extra.pulp3d.json",
        )
        result = run_matrix_verifier(
            args.matrix_verifier, args.preflight_tool, unlisted_dir)
        expect_failure(
            "unlisted-fixture",
            result,
            "unlisted matrix sidecar fixture: unlisted-extra.pulp3d.json",
            errors,
        )
        print("fixture_set_drift_rejected=unlisted")

        missing_dir = temp_path / "missing"
        missing_dir.mkdir()
        copy_fixtures(args.fixture_dir, missing_dir)
        (missing_dir / "default-pbr-textures-clean.pulp3d.json").unlink()
        result = run_matrix_verifier(
            args.matrix_verifier, args.preflight_tool, missing_dir)
        expect_failure(
            "missing-fixture",
            result,
            "missing matrix sidecar fixture: default-pbr-textures-clean.pulp3d.json",
            errors,
        )
        print("fixture_set_drift_rejected=missing")

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1

    print("bake_preflight_matrix_fixture_set_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
