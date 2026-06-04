#!/usr/bin/env python3
"""Verifies the BoxTextured fixture contract rejects fixture/doc drift."""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


FILES_TO_COPY = (
    "test/fixtures/scene3d/BoxTextured/BoxTextured.glb",
    "test/fixtures/scene3d/BoxTextured/README.md",
    "DEPENDENCIES.md",
    "NOTICE.md",
    "docs/reference/licensing.md",
    "tools/deps/manifest.json",
)


def read_text(path):
    return path.read_text(encoding="utf-8")


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def copy_fixture_repo(source_root, target_root):
    for relative in FILES_TO_COPY:
        src = source_root / relative
        dst = target_root / relative
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def run_verifier(verifier, repo_root):
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--repo-root",
            str(repo_root),
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


def mutate_fixture_hash(repo_root):
    fixture = repo_root / "test/fixtures/scene3d/BoxTextured/BoxTextured.glb"
    data = bytearray(fixture.read_bytes())
    if not data:
        raise ValueError("fixture is empty")
    data[-1] ^= 0x01
    fixture.write_bytes(bytes(data))


def mutate_readme_source_url(repo_root):
    path = repo_root / "test/fixtures/scene3d/BoxTextured/README.md"
    write_text(path, read_text(path).replace("glTF-Sample-Assets", "glTF-Sample-Assetz", 1))


def mutate_dependencies_license(repo_root):
    path = repo_root / "DEPENDENCIES.md"
    write_text(
        path,
        replace_once(
            read_text(path),
            "LicenseRef-CC-BY-TM + LicenseRef-LegalMark-Cesium",
            "LicenseRef-CC-BY-TM",
        ),
    )


def mutate_notice_marker(repo_root):
    path = repo_root / "NOTICE.md"
    text = read_text(path)
    if "Cesium" not in text:
        raise ValueError("NOTICE.md does not contain Cesium marker")
    write_text(path, text.replace("Cesium", "LegalMarkMissing"))


def mutate_licensing_path(repo_root):
    path = repo_root / "docs/reference/licensing.md"
    write_text(
        path,
        replace_once(
            read_text(path),
            "test/fixtures/scene3d/BoxTextured/BoxTextured.glb",
            "test/fixtures/scene3d/BoxTextured/Missing.glb",
        ),
    )


def mutate_manifest_version(repo_root):
    path = repo_root / "tools/deps/manifest.json"
    data = json.loads(read_text(path))
    for entry in data.get("dependencies", []):
        if entry.get("name") == "Khronos Box Textured fixture":
            entry["version"] = "not-the-boxtextured-sha"
            write_text(path, json.dumps(data, indent=2) + "\n")
            return
    raise ValueError("missing Khronos Box Textured fixture manifest entry")


def mutate_manifest_source_file(repo_root):
    path = repo_root / "tools/deps/manifest.json"
    data = json.loads(read_text(path))
    for entry in data.get("dependencies", []):
        if entry.get("name") == "Khronos Box Textured fixture":
            source_files = entry.get("source_files", [])
            if "NOTICE.md" in source_files:
                source_files.remove("NOTICE.md")
            write_text(path, json.dumps(data, indent=2) + "\n")
            return
    raise ValueError("missing Khronos Box Textured fixture manifest entry")


def mutate_missing_manifest_entry(repo_root):
    path = repo_root / "tools/deps/manifest.json"
    data = json.loads(read_text(path))
    data["dependencies"] = [
        entry
        for entry in data.get("dependencies", [])
        if entry.get("name") != "Khronos Box Textured fixture"
    ]
    write_text(path, json.dumps(data, indent=2) + "\n")


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
    print(f"boxtextured_fixture_contract_case={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify the BoxTextured fixture verifier rejects drift.")
    parser.add_argument("--fixture-verifier", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()

    source_root = args.repo_root.resolve()
    if not args.fixture_verifier.exists():
        print(f"fixture_verifier_exists=false path={args.fixture_verifier}")
        return 2
    for relative in FILES_TO_COPY:
        if not (source_root / relative).exists():
            print(f"fixture_contract_source_exists=false path={source_root / relative}")
            return 2

    cases = [
        ("valid-current-fixture", None, 0, "boxtextured_fixture_verified=true"),
        ("fixture-hash-drift", mutate_fixture_hash, 1, "expected sha256"),
        ("readme-source-url-drift", mutate_readme_source_url, 1, "README.md: missing"),
        ("dependencies-license-drift", mutate_dependencies_license, 1, "DEPENDENCIES.md: missing"),
        ("notice-marker-drift", mutate_notice_marker, 1, "NOTICE.md: missing 'Cesium'"),
        ("licensing-path-drift", mutate_licensing_path, 1, "licensing.md: missing"),
        ("manifest-version-drift", mutate_manifest_version, 1, "manifest.version: expected"),
        ("manifest-source-file-drift", mutate_manifest_source_file, 1, "manifest.source_files: missing 'NOTICE.md'"),
        ("manifest-entry-missing", mutate_missing_manifest_entry, 1, "manifest: missing Khronos Box Textured fixture entry"),
    ]
    errors = []

    with tempfile.TemporaryDirectory() as tmp:
        tmp_root = Path(tmp)
        for name, mutate, expected_code, expected_text in cases:
            case_root = tmp_root / name
            copy_fixture_repo(source_root, case_root)
            if mutate is not None:
                mutate(case_root)
            expect_case(
                name,
                run_verifier(args.fixture_verifier, case_root),
                expected_code,
                expected_text,
                errors,
            )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("boxtextured_fixture_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
