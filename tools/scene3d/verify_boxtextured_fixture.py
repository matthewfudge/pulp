#!/usr/bin/env python3
import argparse
import hashlib
import json
import sys
from pathlib import Path


EXPECTED_SHA256 = "b510eca2e2ef33f62f9ed57d6e7ce2d10ebb2bdebc4a8e59d347719ba81abdf4"
FIXTURE_RELATIVE = "test/fixtures/scene3d/BoxTextured/BoxTextured.glb"
README_RELATIVE = "test/fixtures/scene3d/BoxTextured/README.md"
SOURCE_TREE_URL = (
    "https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/BoxTextured")
SOURCE_FILE_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/"
    "Models/BoxTextured/glTF-Binary/BoxTextured.glb")
LICENSE_MARKERS = {
    "LicenseRef-CC-BY-TM",
    "LicenseRef-LegalMark-Cesium",
    "Cesium",
}


def sha256(path: Path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_contains(label, text, needles, errors):
    for needle in needles:
        if needle not in text:
            errors.append(f"{label}: missing {needle!r}")


def load_manifest(path: Path, errors):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"{path}: {exc}")
        return []
    dependencies = data.get("dependencies")
    if not isinstance(dependencies, list):
        errors.append(f"{path}: dependencies must be an array")
        return []
    return dependencies


def find_manifest_entry(dependencies):
    for entry in dependencies:
        if isinstance(entry, dict) and entry.get("name") == "Khronos Box Textured fixture":
            return entry
    return None


def verify_manifest(entry, errors):
    if entry is None:
        errors.append("manifest: missing Khronos Box Textured fixture entry")
        return
    expected = {
        "category": "test-fixture",
        "license": "LicenseRef-CC-BY-TM + LicenseRef-LegalMark-Cesium",
        "version": EXPECTED_SHA256,
        "source_kind": "url-snapshot",
        "repository": SOURCE_TREE_URL,
    }
    for key, expected_value in expected.items():
        actual = entry.get(key)
        if actual != expected_value:
            errors.append(
                f"manifest.{key}: expected {expected_value!r}, got {actual!r}")
    upstream = entry.get("upstream")
    if not isinstance(upstream, dict):
        errors.append("manifest.upstream: expected object")
    else:
        if upstream.get("kind") != "url-snapshot":
            errors.append(
                f"manifest.upstream.kind: expected 'url-snapshot', got {upstream.get('kind')!r}")
        if upstream.get("ref") != SOURCE_FILE_URL:
            errors.append(
                f"manifest.upstream.ref: expected {SOURCE_FILE_URL!r}, got {upstream.get('ref')!r}")
    source_files = entry.get("source_files")
    if not isinstance(source_files, list):
        errors.append("manifest.source_files: expected array")
    else:
        for required in (
                FIXTURE_RELATIVE,
                README_RELATIVE,
                "DEPENDENCIES.md",
                "NOTICE.md",
                "docs/reference/licensing.md"):
            if required not in source_files:
                errors.append(f"manifest.source_files: missing {required!r}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify the checked-in Khronos BoxTextured GLB fixture contract.")
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    fixture = repo_root / FIXTURE_RELATIVE
    readme = repo_root / README_RELATIVE
    dependencies_md = repo_root / "DEPENDENCIES.md"
    notice_md = repo_root / "NOTICE.md"
    licensing_md = repo_root / "docs/reference/licensing.md"
    manifest_json = repo_root / "tools/deps/manifest.json"

    errors = []
    for path in (fixture, readme, dependencies_md, notice_md, licensing_md, manifest_json):
        if not path.exists():
            errors.append(f"missing required file: {path}")

    if not errors:
        actual_hash = sha256(fixture)
        print(f"boxtextured_fixture_sha256={actual_hash}")
        if actual_hash != EXPECTED_SHA256:
            errors.append(
                f"{fixture}: expected sha256 {EXPECTED_SHA256}, got {actual_hash}")

        expected_markers = {
            FIXTURE_RELATIVE,
            "Khronos",
            "Box Textured",
            SOURCE_TREE_URL,
            SOURCE_FILE_URL,
            EXPECTED_SHA256,
            *LICENSE_MARKERS,
        }
        require_contains("README.md",
                         readme.read_text(encoding="utf-8"),
                         expected_markers,
                         errors)
        require_contains("DEPENDENCIES.md",
                         dependencies_md.read_text(encoding="utf-8"),
                         {"Khronos Box Textured fixture", EXPECTED_SHA256,
                          "LicenseRef-CC-BY-TM + LicenseRef-LegalMark-Cesium"},
                         errors)
        require_contains("NOTICE.md",
                         notice_md.read_text(encoding="utf-8"),
                         expected_markers,
                         errors)
        require_contains("licensing.md",
                         licensing_md.read_text(encoding="utf-8"),
                         {"Khronos Box Textured fixture",
                          "LicenseRef-CC-BY-TM + LicenseRef-LegalMark-Cesium",
                          FIXTURE_RELATIVE,
                          SOURCE_TREE_URL},
                         errors)

        manifest = load_manifest(manifest_json, errors)
        verify_manifest(find_manifest_entry(manifest), errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("boxtextured_fixture_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
