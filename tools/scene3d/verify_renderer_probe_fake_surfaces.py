#!/usr/bin/env python3
import argparse
import ast
import importlib.util
import sys
from pathlib import Path


def read_text(path: Path):
    return path.read_text(encoding="utf-8")


def extract_assignment(path: Path, name: str):
    tree = ast.parse(read_text(path))
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == name:
                return ast.literal_eval(node.value)
    raise ValueError(f"{path}: missing {name}")


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ValueError(f"{path}: cannot load module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_key_values(text):
    values = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def fake_script_output_to_text(source):
    prefix = "print("
    suffix = ", end='')\n"
    if not source.startswith(prefix) or not source.endswith(suffix):
        raise ValueError("fake probe source has unexpected shape")
    return ast.literal_eval(source[len(prefix):-len(suffix)])


def require_fields(name, actual_fields, expected_fields, errors):
    if actual_fields != expected_fields:
        errors.append(
            f"{name}: expected fields {sorted(expected_fields)!r}, "
            f"got {sorted(actual_fields)!r}")
    print(f"renderer_probe_fake_surface_verified={name}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify synthetic Renderer3D probe outputs cover every probe field.")
    parser.add_argument("--probe-verifier", type=Path, required=True)
    parser.add_argument("--probe-contract", type=Path, required=True)
    parser.add_argument("--final-eligibility-contract", type=Path, required=True)
    args = parser.parse_args()

    for label, path in (
            ("probe_verifier", args.probe_verifier),
            ("probe_contract", args.probe_contract),
            ("final_eligibility_contract", args.final_eligibility_contract)):
        if not path.exists():
            print(f"{label}_exists=false path={path}")
            return 2

    expected_fields = extract_assignment(args.probe_verifier, "PROBE_FIELDS")
    probe_lines = extract_assignment(args.probe_contract, "PROBE_LINES")
    final_module = load_module(args.final_eligibility_contract,
                               "renderer_probe_final_eligibility_contract")

    errors = []
    require_fields(
        "probe-contract-lines",
        set(parse_key_values("\n".join(probe_lines)).keys()),
        expected_fields,
        errors,
    )
    for final_value in (True, False):
        source = final_module.fake_probe_source(final_value)
        label = "final-eligibility-true" if final_value else "final-eligibility-false"
        require_fields(
            label,
            set(parse_key_values(fake_script_output_to_text(source)).keys()),
            expected_fields,
            errors,
        )

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("renderer_probe_fake_surfaces_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
