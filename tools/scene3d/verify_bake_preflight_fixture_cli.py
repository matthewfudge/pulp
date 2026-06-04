#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


CASES = [
    {
        "name": "clean",
        "fixture": "clean.pulp3d.json",
        "exit_code": 0,
        "args": [],
        "fragments": [
            "bake_readiness=clean",
            "export_blocked=false",
            "native_runtime_has_gaps=false",
        ],
    },
    {
        "name": "clean-runtime-evidence-url",
        "fixture": "clean.pulp3d.json",
        "exit_code": 0,
        "args": ["--require-runtime-evidence-url"],
        "fragments": [
            "bake_readiness=clean",
            "export_blocked=false",
            "runtime_evidence_missing=false",
            "runtime_evidence_url_invalid=false",
        ],
    },
    {
        "name": "shader-material-blocked",
        "fixture": "shader-material-blocked.pulp3d.json",
        "exit_code": 2,
        "args": [],
        "fragments": [
            "bake_readiness=blocked",
            "export_blocked=true",
            "export_blocker: ShaderMaterial",
        ],
    },
    {
        "name": "live-runtime-blocked",
        "fixture": "live-runtime-blocked.pulp3d.json",
        "exit_code": 2,
        "args": [],
        "fragments": [
            "bake_readiness=blocked",
            "export_blocked=true",
            "export_blockers=6",
        ],
    },
    {
        "name": "texture-encoding-blocked",
        "fixture": "texture-encoding-blocked.pulp3d.json",
        "exit_code": 2,
        "args": [],
        "fragments": [
            "bake_readiness=blocked",
            "texture_encoding_blocked=true",
            "texture_encoding_blocker: TextureEncoding",
        ],
    },
    {
        "name": "native-gap",
        "fixture": "native-gap.pulp3d.json",
        "exit_code": 0,
        "args": [],
        "fragments": [
            "bake_readiness=native_gaps",
            "export_blocked=false",
            "native_runtime_has_gaps=true",
            "native_runtime_gap: TransformAnimation",
        ],
    },
    {
        "name": "missing-runtime-evidence",
        "fixture": "missing-runtime-evidence.pulp3d.json",
        "exit_code": 2,
        "args": ["--require-runtime-evidence"],
        "fragments": [
            "bake_readiness=blocked",
            "export_blocked=true",
            "runtime_evidence_missing=true",
        ],
    },
    {
        "name": "invalid-runtime-evidence-url",
        "fixture": "native-gap.pulp3d.json",
        "exit_code": 2,
        "args": ["--require-runtime-evidence-url"],
        "fragments": [
            "bake_readiness=blocked",
            "export_blocked=true",
            "runtime_evidence_missing=false",
            "runtime_evidence_url_invalid=true",
        ],
    },
]


def run_case(preflight_tool, fixtures_dir, case):
    fixture = fixtures_dir / case["fixture"]
    command = [str(preflight_tool), *case["args"], str(fixture)]
    print(f"bake_preflight_fixture_cli_case={case['name']}")
    print("command=" + " ".join(command))

    result = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    sys.stdout.write(result.stdout)

    if result.returncode != case["exit_code"]:
        print(
            f"{case['name']}: expected exit code {case['exit_code']}, "
            f"got {result.returncode}",
            file=sys.stderr,
        )
        return False

    missing = [fragment for fragment in case["fragments"] if fragment not in result.stdout]
    if missing:
        print(
            f"{case['name']}: missing output fragments: {', '.join(missing)}",
            file=sys.stderr,
        )
        return False

    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preflight-tool", required=True)
    parser.add_argument("--fixtures-dir", required=True)
    args = parser.parse_args()

    preflight_tool = Path(args.preflight_tool)
    fixtures_dir = Path(args.fixtures_dir)

    if not preflight_tool.exists():
        print(f"missing preflight tool: {preflight_tool}", file=sys.stderr)
        return 1
    if not fixtures_dir.is_dir():
        print(f"missing fixtures directory: {fixtures_dir}", file=sys.stderr)
        return 1

    ok = True
    for case in CASES:
        if not run_case(preflight_tool, fixtures_dir, case):
            ok = False

    if not ok:
        return 1

    print("bake_preflight_fixture_cli_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
