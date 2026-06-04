#!/usr/bin/env python3
import argparse
import ast
import sys
from pathlib import Path


FULL_SCHEMA_CONSTANTS = (
    "ROOT_KEYS",
    "PROVENANCE_KEYS",
    "DIAGNOSTIC_KEYS",
    "UNSUPPORTED_FEATURE_KEYS",
    "RUNTIME_HINT_KEYS",
)


def read_text(path: Path):
    return path.read_text(encoding="utf-8")


def extract_sets(path: Path):
    tree = ast.parse(read_text(path))
    values = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            if not isinstance(target, ast.Name):
                continue
            if not target.id.endswith("_KEYS"):
                continue
            value = ast.literal_eval(node.value)
            if not isinstance(value, set):
                raise ValueError(f"{path}: {target.id} must be a set literal")
            values[target.id] = value
    return values


def require_equal(name, left_label, left, right_label, right, errors):
    if left != right:
        errors.append(
            f"{name} drift: {left_label} missing {sorted(right - left)!r}, "
            f"{left_label} extra {sorted(left - right)!r} versus {right_label}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify native sidecar schema constants stay aligned across tools.")
    parser.add_argument("--canonical", type=Path, required=True)
    parser.add_argument("--preflight-matrix", type=Path, required=True)
    parser.add_argument("--boxtextured-sidecar", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("canonical", args.canonical),
            ("preflight_matrix", args.preflight_matrix),
            ("boxtextured_sidecar", args.boxtextured_sidecar)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    canonical = extract_sets(args.canonical)
    preflight = extract_sets(args.preflight_matrix)
    boxtextured = extract_sets(args.boxtextured_sidecar)

    errors = []
    for name in FULL_SCHEMA_CONSTANTS:
        if name not in canonical:
            errors.append(f"canonical missing {name}")
            continue
        if name not in preflight:
            errors.append(f"preflight matrix missing {name}")
            continue
        require_equal(name,
                      "preflight matrix",
                      preflight[name],
                      "canonical",
                      canonical[name],
                      errors)

    for name in ("ROOT_KEYS", "PROVENANCE_KEYS"):
        if name not in canonical:
            continue
        if name not in boxtextured:
            errors.append(f"BoxTextured sidecar verifier missing {name}")
            continue
        require_equal(name,
                      "BoxTextured sidecar verifier",
                      boxtextured[name],
                      "canonical",
                      canonical[name],
                      errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("sidecar_schema_constants_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
