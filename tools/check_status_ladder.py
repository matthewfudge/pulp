#!/usr/bin/env python3
"""check_status_ladder.py — enforce the partial→usable promotion gate.

Reads docs/status/support-matrix.yaml. For every entry whose status is
`usable` or higher, asserts the entry has a `ci_test:` field referencing
a real CMake target in test/CMakeLists.txt.

This is sub-deliverable 8.4 of production-readiness workstream 08.
Documented in docs/guides/status-ladder.md.

Modes:
    --mode=report (default): print violations, always exit 0.
    --mode=block            : print violations, exit 1 if any.

Phase-in policy: ship in report mode. Promote to block mode after one
release of cleanup so existing entries that lack `ci_test:` get a chance
to either gain coverage or be downgraded honestly.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = REPO_ROOT / "docs" / "status" / "support-matrix.yaml"
TEST_CMAKE_PATH = REPO_ROOT / "test" / "CMakeLists.txt"

# Statuses that participate in the gate. `partial`, `experimental`, `planned`,
# `unsupported` are explicitly OK without ci_test (the spec).
PROMOTED_STATUSES = {"usable", "stable"}


def load_test_targets(cmake_path: Path) -> set[str]:
    """Return the set of `add_executable(<name> ...)` target names."""
    if not cmake_path.exists():
        return set()
    text = cmake_path.read_text(encoding="utf-8")
    return set(re.findall(r"add_executable\(\s*([\w\-]+)\b", text))


def parse_entries(matrix_text: str) -> list[dict]:
    """Yield {path, status, ci_test, line} for every leaf entry with a status:.

    Pure-Python, no PyYAML. Tracks current path via indent levels. An entry
    is a dict whose `status:` field is set; we capture its surrounding
    `ci_test:` (if any) by scanning the same indent block.

    The parse is intentionally narrow: it handles the indent shape used by
    support-matrix.yaml today (2-space increments, status/ci_test keys at
    the same indent level inside a leaf block).
    """
    lines = matrix_text.splitlines()
    entries: list[dict] = []
    # Stack of (indent, key) pairs representing the current path.
    path_stack: list[tuple[int, str]] = []

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)

        # Skip blank / comment lines.
        if not stripped or stripped.startswith("#"):
            i += 1
            continue

        # Pop the path stack until top is shallower than this line.
        while path_stack and path_stack[-1][0] >= indent:
            path_stack.pop()

        # Key-only line: `   accessibility:` (no value)
        m_keyonly = re.match(r"^([\w\-]+):\s*(?:#.*)?$", stripped)
        # Key-value line: `   status: usable`
        m_keyval = re.match(r"^([\w\-]+):\s*([^\n]+?)(?:\s*#.*)?$", stripped)

        if m_keyonly and not m_keyval:
            path_stack.append((indent, m_keyonly.group(1)))
            i += 1
            continue

        if m_keyval:
            key, raw_value = m_keyval.group(1), m_keyval.group(2).strip()
            value = raw_value.strip("\"'")

            if key == "status":
                # Found a status. Look in the same indent block for ci_test.
                ci_test = _find_ci_test_in_block(lines, i, indent)
                limitations = _find_limitations_in_block(lines, i, indent)
                platform = _find_platform_in_block(lines, i, indent)
                path = ".".join(p for _, p in path_stack)
                entries.append({
                    "path": path or "<root>",
                    "status": value,
                    "ci_test": ci_test,
                    "limitations": limitations,
                    "platform": platform,
                    "line": i + 1,
                })
            i += 1
            continue

        # List items, multiline values — skip cleanly.
        i += 1

    return entries


def _find_ci_test_in_block(
    lines: list[str], start_idx: int, block_indent: int,
) -> list[str]:
    """Look forward from start_idx for a `ci_test:` field at block_indent.

    Stops when indent decreases below block_indent. Handles both:
        ci_test: pulp-test-foo
        ci_test:
          - pulp-test-foo
          - pulp-test-foo-extras
    """
    out: list[str] = []
    i = start_idx + 1
    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if not stripped or stripped.startswith("#"):
            i += 1
            continue
        if indent < block_indent:
            break
        if indent == block_indent:
            m_inline = re.match(r"^ci_test:\s*([^\s#].*?)(?:\s*#.*)?$", stripped)
            if m_inline:
                out.append(m_inline.group(1).strip("\"'"))
                return out
            m_listhead = re.match(r"^ci_test:\s*(?:#.*)?$", stripped)
            if m_listhead:
                # Read subsequent `  - <name>` items.
                j = i + 1
                while j < len(lines):
                    sub = lines[j]
                    sub_strip = sub.lstrip(" ")
                    sub_indent = len(sub) - len(sub_strip)
                    if not sub_strip or sub_strip.startswith("#"):
                        j += 1
                        continue
                    if sub_indent <= block_indent:
                        break
                    m_item = re.match(r"^- (.+?)\s*$", sub_strip)
                    if m_item:
                        out.append(m_item.group(1).strip("\"'"))
                    j += 1
                return out
        i += 1
    return out


def _find_platform_in_block(
    lines: list[str], start_idx: int, block_indent: int,
) -> str | None:
    i = start_idx + 1
    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if not stripped or stripped.startswith("#"):
            i += 1
            continue
        if indent < block_indent:
            break
        if indent == block_indent:
            m = re.match(r"^platform:\s*([^\s#].*?)(?:\s*#.*)?$", stripped)
            if m:
                return m.group(1).strip("\"'")
        i += 1
    return None


def _find_limitations_in_block(
    lines: list[str], start_idx: int, block_indent: int,
) -> list[str]:
    i = start_idx + 1
    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if not stripped or stripped.startswith("#"):
            i += 1
            continue
        if indent < block_indent:
            break
        if indent == block_indent:
            if re.match(r"^limitations:\s*(?:#.*)?$", stripped):
                out: list[str] = []
                j = i + 1
                while j < len(lines):
                    sub = lines[j]
                    sub_strip = sub.lstrip(" ")
                    sub_indent = len(sub) - len(sub_strip)
                    if not sub_strip or sub_strip.startswith("#"):
                        j += 1
                        continue
                    if sub_indent <= block_indent:
                        break
                    m = re.match(r"^- (.+?)\s*$", sub_strip)
                    if m:
                        out.append(m.group(1).strip("\"'"))
                    j += 1
                return out
        i += 1
    return []


def check(entries: list[dict], targets: set[str]) -> list[str]:
    problems: list[str] = []
    for e in entries:
        if e["status"] not in PROMOTED_STATUSES:
            continue
        if not e["ci_test"]:
            problems.append(
                f"{e['path']} (line {e['line']}): status='{e['status']}' "
                f"requires `ci_test:` per docs/guides/status-ladder.md"
            )
            continue
        for tgt in e["ci_test"]:
            if tgt not in targets:
                problems.append(
                    f"{e['path']} (line {e['line']}): ci_test '{tgt}' is not "
                    f"an add_executable target in test/CMakeLists.txt"
                )
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode",
        choices=["report", "block"],
        default="report",
        help="report (default): always exit 0; block: exit 1 on violations.",
    )
    parser.add_argument("--matrix", type=Path, default=MATRIX_PATH)
    parser.add_argument("--cmake", type=Path, default=TEST_CMAKE_PATH)
    args = parser.parse_args(argv)

    if not args.matrix.exists():
        print(f"ERROR: matrix file missing: {args.matrix}", file=sys.stderr)
        return 2

    matrix_text = args.matrix.read_text(encoding="utf-8")
    targets = load_test_targets(args.cmake)
    entries = parse_entries(matrix_text)
    problems = check(entries, targets)

    promoted = sum(1 for e in entries if e["status"] in PROMOTED_STATUSES)

    if problems:
        print(f"check_status_ladder: {len(problems)} ladder violation(s) "
              f"out of {promoted} promoted entries (mode={args.mode})")
        for p in problems:
            print(f"  - {p}")
        if args.mode == "block":
            return 1
        return 0
    print(f"check_status_ladder: OK ({promoted} promoted entries, all linked to CI)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
