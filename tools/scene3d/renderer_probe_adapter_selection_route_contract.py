#!/usr/bin/env python3
"""Verifies the adapter-selection route smoke rejects telemetry drift."""

import argparse
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path


def load_route_smoke(path):
    spec = importlib.util.spec_from_file_location(
        "renderer_probe_adapter_selection_route_smoke", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_probe(path, cases):
    lines = [
        "#!/usr/bin/env python3",
        "import sys",
        "args = sys.argv[1:]",
        "if '--adapter-backend' in args and args[args.index('--adapter-backend') + 1] == 'null':",
        "    print(" + repr(cases["null"]) + ", end='')",
        "    sys.exit(2)",
        "if '--force-fallback-adapter' in args:",
        "    print(" + repr(cases["fallback"]) + ", end='')",
        "    sys.exit(2)",
        "print('unexpected_probe_args=' + ' '.join(args))",
        "sys.exit(64)",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    path.chmod(path.stat().st_mode | 0o111)


def fields_to_output(fields):
    return "".join(f"{key}={value}\n" for key, value in fields.items())


def run_smoke(smoke, probe):
    return subprocess.run(
        [
            sys.executable,
            str(smoke),
            "--probe-tool",
            str(probe),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_case(name, result, expected_code, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != expected_code:
        errors.append(
            f"{name}: expected exit {expected_code}, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected diagnostic containing {expected_text!r}")
    print(f"renderer_probe_adapter_selection_route_contract_case={name}")


def base_cases():
    null_fields = {
        "scene": "hardcoded",
        "success": "false",
        "gpu_available": "true",
        "adapter_backend_type": "Null",
        "adapter_scope": "dawn_null_route",
        "adapter_backend_preference": "null",
        "fallback_adapter_requested": "false",
        "null_backend_requested": "true",
        "command_submitted": "true",
        "pixel_output_produced": "false",
        "final_software_golden_eligible": "false",
    }
    fallback_fields = {
        "scene": "hardcoded",
        "success": "false",
        "gpu_available": "false",
        "adapter_backend_type": "",
        "adapter_scope": "fallback_adapter_route",
        "adapter_backend_preference": "default",
        "fallback_adapter_requested": "true",
        "null_backend_requested": "false",
        "command_submitted": "false",
        "pixel_output_produced": "false",
        "final_software_golden_eligible": "false",
    }
    return {
        "null": fields_to_output(null_fields),
        "fallback": fields_to_output(fallback_fields),
    }


def main():
    parser = argparse.ArgumentParser(
        description="Verify adapter-selection route smoke drift cases.")
    parser.add_argument("--route-smoke", type=Path, required=True)
    args = parser.parse_args()

    if not args.route_smoke.exists():
        print(f"route_smoke_exists=false path={args.route_smoke}")
        return 2

    load_route_smoke(args.route_smoke)
    errors = []

    with tempfile.TemporaryDirectory() as temp_dir:
        probe = Path(temp_dir) / "fake-renderer3d-probe.py"

        cases = base_cases()
        write_probe(probe, cases)
        expect_case(
            "valid-adapter-selection-route",
            run_smoke(args.route_smoke, probe),
            0,
            "renderer_probe_adapter_selection_route_verified=true",
            errors,
        )

        cases = base_cases()
        cases["null"] = cases["null"].replace(
            "null_backend_requested=true\n",
            "null_backend_requested=false\n")
        write_probe(probe, cases)
        expect_case(
            "null-backend-request-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "null backend null_backend_requested: expected 'true', got 'false'",
            errors,
        )

        cases = base_cases()
        cases["null"] = cases["null"].replace(
            "adapter_backend_type=Null\n",
            "adapter_backend_type=Metal\n")
        write_probe(probe, cases)
        expect_case(
            "null-backend-type-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "null backend adapter_backend_type: expected 'Null', got 'Metal'",
            errors,
        )

        cases = base_cases()
        cases["null"] = cases["null"].replace(
            "pixel_output_produced=false\n",
            "pixel_output_produced=true\n")
        write_probe(probe, cases)
        expect_case(
            "null-backend-pixel-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "null backend pixel_output_produced: expected 'false', got 'true'",
            errors,
        )

        cases = base_cases()
        cases["fallback"] = cases["fallback"].replace(
            "fallback_adapter_requested=true\n",
            "fallback_adapter_requested=false\n")
        write_probe(probe, cases)
        expect_case(
            "fallback-request-drift",
            run_smoke(args.route_smoke, probe),
            1,
            "forced fallback fallback_adapter_requested: expected 'true', got 'false'",
            errors,
        )

        cases = base_cases()
        cases["fallback"] = cases["fallback"].replace(
            "null_backend_requested=false\n",
            "null_backend_requested=true\n")
        write_probe(probe, cases)
        expect_case(
            "fallback-null-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "forced fallback null_backend_requested: expected 'false', got 'true'",
            errors,
        )

        cases = base_cases()
        cases["fallback"] = cases["fallback"].replace(
            "command_submitted=false\n",
            "command_submitted=true\n")
        write_probe(probe, cases)
        expect_case(
            "fallback-command-leak",
            run_smoke(args.route_smoke, probe),
            1,
            "forced fallback command_submitted: expected 'false', got 'true'",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}", file=sys.stderr)
        return 1

    print("renderer_probe_adapter_selection_route_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
