#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys
from pathlib import Path


def run_sidecar(command, fixture, source, exported_at, runtime_evidence):
    return subprocess.run(
        [
            str(command),
            "--source",
            source,
            "--exported-at",
            exported_at,
            "--runtime-evidence",
            runtime_evidence,
            str(fixture),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def require(condition, message, errors):
    if not condition:
        errors.append(message)


ROOT_KEYS = {
    "schema_version",
    "provenance",
    "diagnostics",
    "unsupported_features",
    "runtime_hints",
}

PROVENANCE_KEYS = {
    "source",
    "exporter",
    "exported_at",
    "runtime_evidence",
}


def main():
    parser = argparse.ArgumentParser(
        description="Verify generated pulp-scene3d-sidecar JSON for BoxTextured.")
    parser.add_argument("--sidecar-tool", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--source", default="khronos-boxtextured")
    parser.add_argument("--exported-at", default="2026-06-03T00:00:00Z")
    parser.add_argument(
        "--runtime-evidence",
        default="https://github.com/danielraffel/pulp/issues/3369#issuecomment-4610177927")
    args = parser.parse_args()

    if not args.sidecar_tool.exists():
        print(f"sidecar_tool_exists=false path={args.sidecar_tool}")
        return 2
    if not args.fixture.exists():
        print(f"fixture_exists=false path={args.fixture}")
        return 2

    result = run_sidecar(args.sidecar_tool,
                         args.fixture,
                         args.source,
                         args.exported_at,
                         args.runtime_evidence)
    sys.stdout.write(result.stdout)
    if result.returncode != 0:
        print(f"sidecar_exit_code={result.returncode}", file=sys.stderr)
        return result.returncode

    try:
        sidecar = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        print(f"sidecar_json_valid=false error={exc}", file=sys.stderr)
        return 1

    errors = []
    require(isinstance(sidecar, dict), "sidecar root must be an object", errors)
    require(sidecar.get("schema_version") == 1,
            "schema_version must be 1",
            errors)
    require(set(sidecar.keys()) == ROOT_KEYS,
            f"sidecar keys mismatch: {sorted(sidecar.keys())}",
            errors)

    provenance = sidecar.get("provenance")
    require(isinstance(provenance, dict),
            "provenance must be an object",
            errors)
    if isinstance(provenance, dict):
        require(set(provenance.keys()) == PROVENANCE_KEYS,
                f"provenance keys mismatch: {sorted(provenance.keys())}",
                errors)
        expected_provenance = {
            "source": args.source,
            "exporter": "pulp-scene3d-sidecar",
            "exported_at": args.exported_at,
            "runtime_evidence": args.runtime_evidence,
        }
        for key, expected in expected_provenance.items():
            require(provenance.get(key) == expected,
                    f"provenance.{key}: expected {expected!r}, got {provenance.get(key)!r}",
                    errors)

    for key in ("diagnostics", "unsupported_features", "runtime_hints"):
        require(sidecar.get(key) == [],
                f"{key} must be an empty array",
                errors)

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print("scene3d_sidecar_verified=boxtextured")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
