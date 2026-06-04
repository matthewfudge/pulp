#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run_validator(script: Path, asset: Path, *, env, require=False):
    command = [sys.executable, str(script)]
    if require:
        command.append("--require-validator")
    command.append(str(asset))
    return subprocess.run(
        command,
        check=False,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def write_fake_validator(path: Path):
    path.write_text(
        """import json
import os
import sys

if "-o" not in sys.argv:
    print("fake validator expected -o <asset>", file=sys.stderr)
    sys.exit(9)

mode = os.environ.get("PULP_FAKE_GLTF_VALIDATOR_MODE", "success")
if mode == "success":
    print(json.dumps({"issues": {"numErrors": 0, "numWarnings": 1, "numInfos": 2}}))
    sys.exit(0)
if mode == "errors":
    print(json.dumps({"issues": {"numErrors": 2, "numWarnings": 0, "numInfos": 0}}))
    sys.exit(0)
if mode == "invalid-json":
    print("not-json")
    sys.exit(0)

print(f"unknown fake validator mode: {mode}", file=sys.stderr)
sys.exit(8)
""",
        encoding="utf-8",
    )


def require(condition, message, errors):
    if not condition:
        errors.append(message)


def require_output(result, expected, label, errors):
    require(
        expected in result.stdout,
        f"{label}: missing {expected!r} in output:\n{result.stdout}",
        errors,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify validate_gltf.py wrapper behavior without a real validator.")
    parser.add_argument("--validator-wrapper", type=Path, required=True)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="pulp-gltf-validator-wrapper-") as tmp:
        tmpdir = Path(tmp)
        asset = tmpdir / "fixture.glb"
        asset.write_bytes(b"glb")
        fake_validator = tmpdir / "fake_validator.py"
        write_fake_validator(fake_validator)

        base_env = {
            key: value
            for key, value in os.environ.items()
            if key not in {"NODE_PATH", "PULP_GLTF_VALIDATOR"}
        }
        base_env["PATH"] = str(tmpdir / "empty-path")

        missing = run_validator(args.validator_wrapper, asset, env=base_env)
        missing_required = run_validator(
            args.validator_wrapper, asset, env=base_env, require=True)

        configured_env = dict(base_env)
        configured_env["PULP_GLTF_VALIDATOR"] = (
            f"{sys.executable} {fake_validator}")

        success = run_validator(
            args.validator_wrapper, asset, env=configured_env, require=True)

        error_env = dict(configured_env)
        error_env["PULP_FAKE_GLTF_VALIDATOR_MODE"] = "errors"
        errors_report = run_validator(
            args.validator_wrapper, asset, env=error_env, require=True)

        invalid_env = dict(configured_env)
        invalid_env["PULP_FAKE_GLTF_VALIDATOR_MODE"] = "invalid-json"
        invalid_json = run_validator(
            args.validator_wrapper, asset, env=invalid_env, require=True)

    failures = []
    require(missing.returncode == 77,
            f"missing validator default returned {missing.returncode}, expected 77",
            failures)
    require_output(missing, "Khronos glTF-Validator not found",
                   "missing validator default", failures)

    require(missing_required.returncode == 2,
            f"missing validator required returned {missing_required.returncode}, expected 2",
            failures)
    require_output(missing_required, "Khronos glTF-Validator not found",
                   "missing validator required", failures)

    require(success.returncode == 0,
            f"configured validator success returned {success.returncode}, expected 0",
            failures)
    require_output(success, "glTF-Validator errors=0 warnings=1 infos=2",
                   "configured validator success", failures)

    require(errors_report.returncode == 1,
            f"validator error report returned {errors_report.returncode}, expected 1",
            failures)
    require_output(errors_report, "glTF-Validator errors=2 warnings=0 infos=0",
                   "validator error report", failures)

    require(invalid_json.returncode == 2,
            f"invalid JSON returned {invalid_json.returncode}, expected 2",
            failures)
    require_output(invalid_json, "validator did not write a JSON report to stdout",
                   "invalid JSON", failures)

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print("gltf_validator_wrapper_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
