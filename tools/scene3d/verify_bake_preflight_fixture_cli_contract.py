#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


FAKE_PREFLIGHT = r'''#!/usr/bin/env python3
import os
import sys
from pathlib import Path


DRIFT = os.environ.get("PULP_FAKE_PREFLIGHT_DRIFT", "")


def emit(lines):
    for line in lines:
        print(line)


def main():
    args = sys.argv[1:]
    require_runtime = False
    require_runtime_url = False
    if args and args[0] == "--require-runtime-evidence":
        require_runtime = True
        args = args[1:]
    if args and args[0] == "--require-runtime-evidence-url":
        require_runtime_url = True
        args = args[1:]
    if len(args) != 1:
        print("usage: fake-preflight [--require-runtime-evidence|--require-runtime-evidence-url] <sidecar>")
        return 64

    fixture = Path(args[0]).name
    if fixture == "clean.pulp3d.json":
        emit([
            "bake_readiness=clean",
            "export_blocked=false",
            "native_runtime_has_gaps=false",
            "runtime_evidence_missing=false",
            "runtime_evidence_url_invalid=false",
        ])
        return 2 if DRIFT == "clean_exit" else 0

    if fixture == "shader-material-blocked.pulp3d.json":
        emit([
            "bake_readiness=blocked",
            "export_blocked=true",
        ])
        if DRIFT != "shader_row":
            print("export_blocker: ShaderMaterial node=/Scene/CustomShader")
        return 2

    if fixture == "live-runtime-blocked.pulp3d.json":
        blockers = "5" if DRIFT == "live_count" else "6"
        emit([
            "bake_readiness=blocked",
            "export_blocked=true",
            f"export_blockers={blockers}",
        ])
        return 2

    if fixture == "texture-encoding-blocked.pulp3d.json":
        emit([
            "bake_readiness=blocked",
            "texture_encoding_blocked=true",
        ])
        if DRIFT != "texture_row":
            print("texture_encoding_blocker: TextureEncoding node=/Scene/Textured")
        return 2

    if fixture == "native-gap.pulp3d.json":
        if require_runtime_url:
            emit([
                "bake_readiness=blocked",
                "export_blocked=true",
                "native_runtime_has_gaps=true",
                "runtime_evidence_missing=false",
            ])
            if DRIFT != "runtime_evidence_url_flag":
                print("runtime_evidence_url_invalid=true")
            print("native_runtime_gap: TransformAnimation node=/Scene/Animated")
            return 2
        emit([
            "bake_readiness=native_gaps",
            "export_blocked=false",
            "native_runtime_has_gaps=true",
            "native_runtime_gap: TransformAnimation node=/Scene/Animated",
        ])
        return 2 if DRIFT == "native_gap_exit" else 0

    if fixture == "missing-runtime-evidence.pulp3d.json":
        emit([
            "bake_readiness=blocked",
            "export_blocked=true",
        ])
        if require_runtime and DRIFT != "runtime_evidence_flag":
            print("runtime_evidence_missing=true")
        return 2

    print(f"unexpected fixture: {fixture}")
    return 70


if __name__ == "__main__":
    raise SystemExit(main())
'''


CASES = [
    {
        "name": "valid-current-fixtures",
        "drift": "",
        "exit_code": 0,
        "text": "bake_preflight_fixture_cli_verified=true",
    },
    {
        "name": "clean-exit-drift",
        "drift": "clean_exit",
        "exit_code": 1,
        "text": "clean: expected exit code 0",
    },
    {
        "name": "shader-row-drift",
        "drift": "shader_row",
        "exit_code": 1,
        "text": "shader-material-blocked: missing output fragments",
    },
    {
        "name": "live-count-drift",
        "drift": "live_count",
        "exit_code": 1,
        "text": "live-runtime-blocked: missing output fragments",
    },
    {
        "name": "texture-row-drift",
        "drift": "texture_row",
        "exit_code": 1,
        "text": "texture-encoding-blocked: missing output fragments",
    },
    {
        "name": "native-gap-exit-drift",
        "drift": "native_gap_exit",
        "exit_code": 1,
        "text": "native-gap: expected exit code 0",
    },
    {
        "name": "runtime-evidence-flag-drift",
        "drift": "runtime_evidence_flag",
        "exit_code": 1,
        "text": "missing-runtime-evidence: missing output fragments",
    },
    {
        "name": "runtime-evidence-url-flag-drift",
        "drift": "runtime_evidence_url_flag",
        "exit_code": 1,
        "text": "invalid-runtime-evidence-url: missing output fragments",
    },
]


def write_fake_preflight(path):
    path.write_text(FAKE_PREFLIGHT, encoding="utf-8")
    path.chmod(0o755)


def run_verifier(verifier, preflight_tool, fixtures_dir, drift):
    env = os.environ.copy()
    if drift:
        env["PULP_FAKE_PREFLIGHT_DRIFT"] = drift
    else:
        env.pop("PULP_FAKE_PREFLIGHT_DRIFT", None)
    return subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--preflight-tool",
            str(preflight_tool),
            "--fixtures-dir",
            str(fixtures_dir),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        check=False,
    )


def expect_case(case, result, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != case["exit_code"]:
        errors.append(
            f"{case['name']}: expected exit {case['exit_code']}, got {result.returncode}"
        )
    if case["text"] not in result.stdout:
        errors.append(
            f"{case['name']}: expected output containing {case['text']!r}"
        )
    print(f"bake_preflight_fixture_cli_contract_case={case['name']}")


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake preflight fixture CLI verifier rejects drift.")
    parser.add_argument("--fixture-verifier", type=Path, required=True)
    parser.add_argument("--fixtures-dir", type=Path, required=True)
    args = parser.parse_args()

    if not args.fixture_verifier.exists():
        print(f"fixture_verifier_exists=false path={args.fixture_verifier}")
        return 2
    if not args.fixtures_dir.is_dir():
        print(f"fixtures_dir_exists=false path={args.fixtures_dir}")
        return 2

    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        preflight_tool = Path(temp_dir) / "fake-preflight"
        write_fake_preflight(preflight_tool)
        for case in CASES:
            result = run_verifier(
                args.fixture_verifier,
                preflight_tool,
                args.fixtures_dir,
                case["drift"],
            )
            expect_case(case, result, errors)

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("bake_preflight_fixture_cli_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
