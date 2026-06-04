#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


VALID_SIDECAR = {
    "schema_version": 1,
    "provenance": {
        "source": "khronos-boxtextured",
        "exporter": "pulp-scene3d-sidecar",
        "exported_at": "2026-06-03T00:00:00Z",
        "runtime_evidence": "https://github.com/danielraffel/pulp/issues/3369#issuecomment-4610177927",
    },
    "diagnostics": [],
    "unsupported_features": [],
    "runtime_hints": [],
}


def write_fake_sidecar_tool(path, payload):
    if isinstance(payload, str):
        body = f"sys.stdout.write({payload!r})\n"
    else:
        body = f"json.dump({payload!r}, sys.stdout)\n"
    path.write_text(
        "#!/usr/bin/env python3\n"
        "import json\n"
        "import sys\n"
        f"{body}",
        encoding="utf-8")
    path.chmod(0o755)


def run_case(verifier, fixture, fake_tool, expect_success):
    result = subprocess.run(
        [
            sys.executable,
            str(verifier),
            "--sidecar-tool",
            str(fake_tool),
            "--fixture",
            str(fixture),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    passed = result.returncode == 0
    if passed != expect_success:
        print(result.stdout, file=sys.stderr, end="")
        expected = "success" if expect_success else "failure"
        actual = "success" if passed else f"failure code {result.returncode}"
        raise RuntimeError(f"expected {expected}, got {actual}")
    return result.stdout


def case_payloads():
    missing_runtime_hints = dict(VALID_SIDECAR)
    missing_runtime_hints.pop("runtime_hints")

    extra_root_key = dict(VALID_SIDECAR)
    extra_root_key["asset"] = {"uri": "BoxTextured.glb"}

    provenance_drift = json.loads(json.dumps(VALID_SIDECAR))
    provenance_drift["provenance"]["exporter"] = "foreign-exporter"

    empty_source = json.loads(json.dumps(VALID_SIDECAR))
    empty_source["provenance"]["source"] = ""

    empty_exported_at = json.loads(json.dumps(VALID_SIDECAR))
    empty_exported_at["provenance"]["exported_at"] = ""

    empty_runtime_evidence = json.loads(json.dumps(VALID_SIDECAR))
    empty_runtime_evidence["provenance"]["runtime_evidence"] = ""

    non_empty_diagnostics = json.loads(json.dumps(VALID_SIDECAR))
    non_empty_diagnostics["diagnostics"] = [{
        "severity": "warning",
        "code": "NativeRuntimeGap",
        "message": "native renderer still lacks a final software golden",
    }]

    malformed_json = '{"schema_version": 1, "provenance": '

    return [
        ("valid-fake-sidecar", VALID_SIDECAR, True),
        ("missing-runtime-hints", missing_runtime_hints, False),
        ("extra-root-key", extra_root_key, False),
        ("provenance-exporter-drift", provenance_drift, False),
        ("empty-provenance-source", empty_source, False),
        ("empty-provenance-exported-at", empty_exported_at, False),
        ("empty-runtime-evidence", empty_runtime_evidence, False),
        ("non-empty-diagnostics", non_empty_diagnostics, False),
        ("malformed-json", malformed_json, False),
    ]


def main():
    parser = argparse.ArgumentParser(
        description="Verify the scene3d sidecar JSON verifier rejects malformed synthetic sidecars.")
    parser.add_argument("--sidecar-verifier", type=Path, required=True)
    args = parser.parse_args()

    if not args.sidecar_verifier.exists():
        print(f"sidecar_verifier_exists=false path={args.sidecar_verifier}")
        return 2

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        fixture = temp_path / "BoxTextured.glb"
        fixture.write_bytes(b"fixture bytes are not read by the synthetic fake")

        for name, payload, expect_success in case_payloads():
            fake_tool = temp_path / f"{name}.py"
            write_fake_sidecar_tool(fake_tool, payload)
            run_case(args.sidecar_verifier, fixture, fake_tool, expect_success)
            print(f"scene3d_sidecar_contract_case={name}")

    print("scene3d_sidecar_contract_verified=true")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
