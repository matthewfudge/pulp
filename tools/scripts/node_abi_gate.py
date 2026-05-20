#!/usr/bin/env python3
"""Node ABI virtual-order gate.

Processor and PluginSlot are SDK-facing polymorphic surfaces. Within a major
node ABI version, new virtual methods must be appended at the end of the class
vtable. Inserting, removing, or reordering existing virtuals changes already
compiled consumer binaries.

The gate compares the current working tree against a git base and requires the
base virtual-method order to remain a prefix of the current order.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


SURFACES = (
    (
        "Processor",
        "core/format/include/pulp/format/processor.hpp",
    ),
    (
        "PluginSlot",
        "core/host/include/pulp/host/plugin_slot.hpp",
    ),
)


def repo_root() -> Path | None:
    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return Path(result.stdout.strip())


def git_show(base: str, rel_path: str) -> str | None:
    result = subprocess.run(
        ["git", "show", f"{base}:{rel_path}"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def class_body(text: str, class_name: str) -> str:
    match = re.search(rf"\bclass\s+{re.escape(class_name)}\b[^{{]*{{", text)
    if not match:
        raise ValueError(f"class {class_name} not found")

    start = match.end()
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    if depth != 0:
        raise ValueError(f"class {class_name} body is unbalanced")
    return text[start:i - 1]


def virtual_order(text: str, class_name: str) -> list[str]:
    body = strip_comments(class_body(text, class_name))
    names: list[str] = []
    for match in re.finditer(r"\bvirtual\b(?P<decl>[^;{]*?)\(", body, re.DOTALL):
        before_paren = match.group("decl").strip()
        name_match = re.search(r"([~A-Za-z_]\w*)\s*$", before_paren)
        if not name_match:
            continue
        names.append(name_match.group(1))
    return names


def render_mismatch(surface: str, old: list[str], new: list[str]) -> str:
    lines = [f"{surface}: virtual order is not additive-only"]
    common = min(len(old), len(new))
    first_bad = next((i for i in range(common) if old[i] != new[i]), common)
    if first_bad < len(old):
        lines.append(f"  first mismatch at index {first_bad}:")
        lines.append(f"    base:    {old[first_bad]}")
        lines.append(
            f"    current: {new[first_bad] if first_bad < len(new) else '<missing>'}"
        )
    if len(new) < len(old):
        lines.append("  current order removed virtual method(s)")
    lines.append("  allowed change: append new virtual methods after all existing ones")
    return "\n".join(lines)


def check_surface(root: Path, base: str, class_name: str, rel_path: str) -> str | None:
    old_text = git_show(base, rel_path)
    if old_text is None:
        return f"{class_name}: could not read {rel_path} at {base}"

    current_path = root / rel_path
    if not current_path.exists():
        return f"{class_name}: current file missing: {rel_path}"

    try:
        old = virtual_order(old_text, class_name)
        new = virtual_order(current_path.read_text(), class_name)
    except ValueError as exc:
        return f"{class_name}: {exc}"

    if new[:len(old)] != old:
        return render_mismatch(class_name, old, new)
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--mode", choices=("hint", "report"), default="report")
    args = parser.parse_args(argv)

    root = repo_root()
    if root is None:
        print("node_abi_gate: not in a git working tree", file=sys.stderr)
        return 0 if args.mode == "hint" else 2

    findings = [
        finding
        for class_name, rel_path in SURFACES
        if (finding := check_surface(root, args.base, class_name, rel_path))
    ]

    if not findings:
        print("node_abi_gate: Processor and PluginSlot virtual order is additive-only")
        return 0

    print("\n\n".join(findings), file=sys.stderr)
    return 0 if args.mode == "hint" else 1


if __name__ == "__main__":
    raise SystemExit(main())
