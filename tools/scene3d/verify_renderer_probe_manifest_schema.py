#!/usr/bin/env python3
import argparse
import ast
import sys
from pathlib import Path


SCHEMA_CONSTANTS = (
    "REQUIRED_MANIFEST_KEYS",
    "REQUIRED_ENTRY_KEYS",
    "REQUIRED_SOFTWARE_ADAPTER_KEYS",
)


def read_text(path: Path):
    return path.read_text(encoding="utf-8")


def extract_assignments(path: Path):
    tree = ast.parse(read_text(path))
    values = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id in SCHEMA_CONSTANTS:
                value = ast.literal_eval(node.value)
                if not isinstance(value, set):
                    raise ValueError(f"{path}: {target.id} must be a set literal")
                values[target.id] = value
    missing = [name for name in SCHEMA_CONSTANTS if name not in values]
    if missing:
        raise ValueError(f"{path}: missing constants: {', '.join(missing)}")
    return values


def main():
    parser = argparse.ArgumentParser(
        description="Verify renderer probe manifest schema mirrors the manifest validator.")
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--manifest-validator", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("probe_verifier", args.probe_verifier),
            ("manifest_validator", args.manifest_validator)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    probe_values = extract_assignments(args.probe_verifier)
    validator_values = extract_assignments(args.manifest_validator)

    errors = []
    for name in SCHEMA_CONSTANTS:
        if probe_values[name] != validator_values[name]:
            errors.append(
                f"{name} drift: probe missing "
                f"{sorted(validator_values[name] - probe_values[name])!r}, "
                f"probe extra {sorted(probe_values[name] - validator_values[name])!r}")

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("renderer_probe_manifest_schema_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
