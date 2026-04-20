#!/usr/bin/env python3
"""Dependency audit and upstream drift checker for Pulp."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "tools" / "deps" / "manifest.json"
DEPENDENCIES_MD = ROOT / "DEPENDENCIES.md"
NOTICE_MD = ROOT / "NOTICE.md"
LICENSING_MD = ROOT / "docs" / "reference" / "licensing.md"


def load_manifest() -> list[dict]:
    return json.loads(MANIFEST.read_text())["dependencies"]


def parse_dependencies_md() -> set[str]:
    text = DEPENDENCIES_MD.read_text()
    names: set[str] = set()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells:
            continue
        first = cells[0]
        if first in {"Name", "SDK", "------", "-----"}:
            continue
        if set(first) == {"-"}:
            continue
        names.add(first)
    return names


def parse_notice_md() -> set[str]:
    text = NOTICE_MD.read_text()
    names: set[str] = set()
    for line in text.splitlines():
        if line.startswith("## "):
            names.add(line[3:].strip())
    return names


def parse_licensing_md() -> set[str]:
    """Extract dependency names from docs/reference/licensing.md tables.

    Table rows that attribute a dependency look like:
        | **Highway** | Apache-2.0 | ... | [link] |
    We pull the first-column bolded name so the check mirrors DEPENDENCIES.md.
    """
    text = LICENSING_MD.read_text()
    names: set[str] = set()
    bold_re = re.compile(r"\*\*([^*]+)\*\*")
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if not cells:
            continue
        first = cells[0]
        match = bold_re.search(first)
        if match:
            names.add(match.group(1).strip())
    return names


def run_git_ls_remote(repo: str, *refs: str) -> str:
    cmd = ["git", "ls-remote", repo, *refs]
    try:
        result = subprocess.run(
            cmd,
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
            timeout=5,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return ""


SEMVER_RE = re.compile(r"(\d+)\.(\d+)\.(\d+)")


def semver_key(value: str):
    match = SEMVER_RE.search(value)
    if not match:
        return None
    return tuple(int(part) for part in match.groups())


def latest_semver_tag(repo: str) -> str | None:
    output = run_git_ls_remote(repo, "--tags", "--refs")
    candidates: list[tuple[tuple[int, int, int], str]] = []
    for line in output.splitlines():
        if not line:
            continue
        ref = line.split("\t", 1)[1]
        tag = ref.removeprefix("refs/tags/")
        key = semver_key(tag)
        if key is not None:
            candidates.append((key, tag))
    if not candidates:
        return None
    candidates.sort()
    return candidates[-1][1]


def upstream_status(dep: dict) -> str:
    kind = dep["upstream"]["kind"]
    repo = dep["repository"]
    if kind == "none":
        return "manual"
    if kind == "git-head":
        output = run_git_ls_remote(repo, "HEAD")
        return output.split()[0][:12] if output else "missing"
    if kind == "git-branch":
        ref = dep["upstream"]["ref"]
        output = run_git_ls_remote(repo, f"refs/heads/{ref}")
        sha = output.split()[0][:12] if output else "missing"
        return f"{ref} @ {sha}"
    if kind == "git-tag":
        ref = dep["upstream"]["ref"]
        output = run_git_ls_remote(repo, f"refs/tags/{ref}")
        exact = "present" if output else "missing"
        latest = latest_semver_tag(repo)
        if latest and latest != ref:
            return f"{exact}; latest={latest}"
        return exact
    return "unknown"


def render_markdown(
    rows: list[dict],
    missing_deps: list[str],
    missing_notice: list[str],
    missing_licensing: list[str],
) -> str:
    lines = [
        "# Dependency Audit",
        "",
        "| Name | Version | License | Source | Upstream | DEPENDENCIES.md | NOTICE.md | licensing.md |",
        "|------|---------|---------|--------|----------|------------------|-----------|--------------|",
    ]
    for row in rows:
        lines.append(
            f"| {row['name']} | {row['version']} | {row['license']} | {row['source_kind']} | "
            f"{row['upstream']} | {row['dependencies_md']} | {row['notice_md']} | {row['licensing_md']} |"
        )
    if missing_deps:
        lines.extend(["", "## Missing from DEPENDENCIES.md", ""])
        lines.extend(f"- {name}" for name in missing_deps)
    if missing_notice:
        lines.extend(["", "## Missing from NOTICE.md", ""])
        lines.extend(f"- {name}" for name in missing_notice)
    if missing_licensing:
        lines.extend(["", "## Missing from docs/reference/licensing.md", ""])
        lines.extend(f"- {name}" for name in missing_licensing)
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit dependency inventory and drift")
    parser.add_argument("--check-upstream", action="store_true", help="Query upstream repos")
    parser.add_argument("--format", choices=["text", "markdown"], default="text")
    parser.add_argument("--strict", action="store_true", help="Fail if docs/notices are incomplete")
    args = parser.parse_args()

    manifest = load_manifest()
    deps_md_names = parse_dependencies_md()
    notice_names = parse_notice_md()
    licensing_names = parse_licensing_md()

    rows = []
    missing_deps: list[str] = []
    missing_notice: list[str] = []
    missing_licensing: list[str] = []

    for dep in manifest:
        in_deps = dep["name"] in deps_md_names
        in_notice = dep["name"] in notice_names
        # licensing.md uses the presentation name inside bold markers.
        # Accept either the manifest name directly or a loose match without
        # trailing " SDK" (e.g. "VST3 SDK" is listed as "VST3 SDK" already).
        in_licensing = dep["name"] in licensing_names or (
            dep["name"].replace("-", " ") in licensing_names
        )
        if dep["documented_in_dependencies_md"] and not in_deps:
            missing_deps.append(dep["name"])
        if dep["documented_in_notice_md"] and not in_notice:
            missing_notice.append(dep["name"])
        # AAX/ASIO and other developer-supplied SDKs are exempt from the
        # public licensing.md table (they live in the "Optional Vendor SDK"
        # section instead), so gate on documented_in_notice_md which already
        # marks them false.
        if dep["documented_in_notice_md"] and not in_licensing:
            missing_licensing.append(dep["name"])
        rows.append({
            "name": dep["name"],
            "version": dep["version"],
            "license": dep["license"],
            "source_kind": dep["source_kind"],
            "upstream": upstream_status(dep) if args.check_upstream else "skipped",
            "dependencies_md": "yes" if in_deps else "no",
            "notice_md": "yes" if in_notice else "no",
            "licensing_md": "yes" if in_licensing else "no",
        })

    if args.format == "markdown":
        output = render_markdown(rows, missing_deps, missing_notice, missing_licensing)
        sys.stdout.write(output)
    else:
        for row in rows:
            print(
                f"{row['name']}: version={row['version']} license={row['license']} "
                f"source={row['source_kind']} upstream={row['upstream']} "
                f"DEPENDENCIES.md={row['dependencies_md']} NOTICE.md={row['notice_md']} "
                f"licensing.md={row['licensing_md']}"
            )
        if missing_deps:
            print("\nMissing from DEPENDENCIES.md:")
            for name in missing_deps:
                print(f"  - {name}")
        if missing_notice:
            print("\nMissing from NOTICE.md:")
            for name in missing_notice:
                print(f"  - {name}")
        if missing_licensing:
            print("\nMissing from docs/reference/licensing.md:")
            for name in missing_licensing:
                print(f"  - {name}")

    if args.strict and (missing_deps or missing_notice or missing_licensing):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
