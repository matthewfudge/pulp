#!/usr/bin/env python3
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def load_sidecar(path):
    return json.loads(path.read_text(encoding="utf-8"))


def run_artifact_verifier(artifact_verifier, asset, sidecar, preflight_tool):
    return subprocess.run(
        [
            sys.executable,
            str(artifact_verifier),
            "--asset",
            str(asset),
            "--sidecar",
            str(sidecar),
            "--preflight-tool",
            str(preflight_tool),
            "--require-extensions",
            "--require-runtime-evidence",
            "--require-runtime-evidence-url",
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def expect_failure(name, result, expected_text, errors):
    sys.stdout.write(result.stdout)
    if result.returncode != 2:
        errors.append(f"{name}: expected exit 2, got {result.returncode}")
    if expected_text not in result.stdout:
        errors.append(
            f"{name}: expected output containing {expected_text!r}")
    print(f"bake_artifact_contract_rejected={name}")


def write_sidecar(path, sidecar):
    path.write_text(json.dumps(sidecar), encoding="utf-8")


def write_fake_preflight_with_row_count_drift(path):
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "print('bake_readiness=clean')",
            "print('export_blocked=false')",
            "print('texture_encoding_blocked=false')",
            "print('native_runtime_has_gaps=false')",
            "print('has_error_diagnostics=false')",
            "print('runtime_evidence_missing=false')",
            "print('runtime_evidence_url_invalid=false')",
            "print('export_blockers=0 texture_encoding_blockers=0 native_runtime_gaps=0 diagnostics=0')",
            "print('native_runtime_gap: FutureRuntimeGap node=/Scene/Future')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(0o755)


def write_fake_preflight_with_mismatched_feature(path):
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "print('bake_readiness=native_gaps')",
            "print('export_blocked=false')",
            "print('texture_encoding_blocked=false')",
            "print('native_runtime_has_gaps=true')",
            "print('has_error_diagnostics=false')",
            "print('runtime_evidence_missing=false')",
            "print('runtime_evidence_url_invalid=false')",
            "print('export_blockers=0 texture_encoding_blockers=0 native_runtime_gaps=1 diagnostics=0')",
            "print('native_runtime_gap: DifferentRuntimeGap node=/Scene/Future')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(0o755)


def write_fake_preflight_with_sidecar_count_drift(path):
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "print('bake_readiness=clean')",
            "print('export_blocked=false')",
            "print('texture_encoding_blocked=false')",
            "print('native_runtime_has_gaps=false')",
            "print('has_error_diagnostics=false')",
            "print('runtime_evidence_missing=false')",
            "print('runtime_evidence_url_invalid=false')",
            "print('export_blockers=0 texture_encoding_blockers=0 native_runtime_gaps=0 diagnostics=0')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(0o755)


def write_fake_preflight_with_readiness_drift(path):
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "print('bake_readiness=native_gaps')",
            "print('export_blocked=false')",
            "print('texture_encoding_blocked=false')",
            "print('native_runtime_has_gaps=false')",
            "print('has_error_diagnostics=false')",
            "print('runtime_evidence_missing=false')",
            "print('runtime_evidence_url_invalid=false')",
            "print('export_blockers=0 texture_encoding_blockers=0 native_runtime_gaps=0 diagnostics=0')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(0o755)


def write_fake_preflight_with_exit_code_drift(path):
    path.write_text(
        "\n".join([
            "#!/usr/bin/env python3",
            "print('bake_readiness=blocked')",
            "print('export_blocked=true')",
            "print('texture_encoding_blocked=false')",
            "print('native_runtime_has_gaps=false')",
            "print('has_error_diagnostics=false')",
            "print('runtime_evidence_missing=true')",
            "print('runtime_evidence_url_invalid=false')",
            "print('export_blockers=0 texture_encoding_blockers=0 native_runtime_gaps=0 diagnostics=0')",
        ]) + "\n",
        encoding="utf-8")
    path.chmod(0o755)


def main():
    parser = argparse.ArgumentParser(
        description="Verify bake-artifact checker rejects malformed artifact pairs.")
    parser.add_argument("--artifact-verifier", type=Path, required=True)
    parser.add_argument("--asset", type=Path, required=True)
    parser.add_argument("--base-sidecar", type=Path, required=True)
    parser.add_argument("--preflight-tool", type=Path, required=True)
    args = parser.parse_args()

    for name, path in (
        ("artifact_verifier", args.artifact_verifier),
        ("asset", args.asset),
        ("base_sidecar", args.base_sidecar),
        ("preflight_tool", args.preflight_tool),
    ):
        if not path.exists():
            print(f"{name}_exists=false path={path}")
            return 2

    base = load_sidecar(args.base_sidecar)
    errors = []
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        wrong_extension = temp_path / "clean.json"
        shutil.copy2(args.base_sidecar, wrong_extension)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            wrong_extension,
            args.preflight_tool)
        expect_failure(
            "sidecar-extension",
            result,
            "error=sidecar must have .pulp3d.json extension",
            errors,
        )

        extra_root = dict(base)
        extra_root["unexpected"] = "drift"
        extra_root_path = temp_path / "extra-root.pulp3d.json"
        write_sidecar(extra_root_path, extra_root)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            extra_root_path,
            args.preflight_tool)
        expect_failure(
            "extra-root-key",
            result,
            "sidecar_root_keys_valid=false",
            errors,
        )

        nonstring_runtime_evidence = json.loads(json.dumps(base))
        nonstring_runtime_evidence["provenance"]["runtime_evidence"] = 7
        nonstring_runtime_path = temp_path / "nonstring-runtime.pulp3d.json"
        write_sidecar(nonstring_runtime_path, nonstring_runtime_evidence)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            nonstring_runtime_path,
            args.preflight_tool)
        expect_failure(
            "nonstring-runtime-evidence",
            result,
            "sidecar_provenance_runtime_evidence_valid=false",
            errors,
        )

        invalid_runtime_evidence_url = json.loads(json.dumps(base))
        invalid_runtime_evidence_url["provenance"][
            "runtime_evidence"] = "native-preflight-fixture"
        invalid_runtime_url_path = temp_path / "invalid-runtime-url.pulp3d.json"
        write_sidecar(invalid_runtime_url_path, invalid_runtime_evidence_url)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            invalid_runtime_url_path,
            args.preflight_tool)
        expect_failure(
            "invalid-runtime-evidence-url",
            result,
            "runtime_evidence_url_valid=false",
            errors,
        )

        empty_source = json.loads(json.dumps(base))
        empty_source["provenance"]["source"] = ""
        empty_source_path = temp_path / "empty-source.pulp3d.json"
        write_sidecar(empty_source_path, empty_source)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            empty_source_path,
            args.preflight_tool)
        expect_failure(
            "empty-provenance-source",
            result,
            "sidecar_provenance_source_valid=false",
            errors,
        )

        empty_exporter = json.loads(json.dumps(base))
        empty_exporter["provenance"]["exporter"] = ""
        empty_exporter_path = temp_path / "empty-exporter.pulp3d.json"
        write_sidecar(empty_exporter_path, empty_exporter)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            empty_exporter_path,
            args.preflight_tool)
        expect_failure(
            "empty-provenance-exporter",
            result,
            "sidecar_provenance_exporter_valid=false",
            errors,
        )

        empty_exported_at = json.loads(json.dumps(base))
        empty_exported_at["provenance"]["exported_at"] = ""
        empty_exported_at_path = temp_path / "empty-exported-at.pulp3d.json"
        write_sidecar(empty_exported_at_path, empty_exported_at)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            empty_exported_at_path,
            args.preflight_tool)
        expect_failure(
            "empty-provenance-exported-at",
            result,
            "sidecar_provenance_exported_at_valid=false",
            errors,
        )

        invalid_severity = json.loads(json.dumps(base))
        invalid_severity["diagnostics"].append({
            "severity": "notice",
            "code": "scene.notice",
            "message": "Unsupported severity must be rejected.",
            "path": "fixture.glb",
        })
        invalid_severity_path = temp_path / "invalid-severity.pulp3d.json"
        write_sidecar(invalid_severity_path, invalid_severity)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            invalid_severity_path,
            args.preflight_tool)
        expect_failure(
            "invalid-diagnostic-severity",
            result,
            "sidecar diagnostics[0].severity must be info, warning, or error",
            errors,
        )

        nonstring_hint = json.loads(json.dumps(base))
        nonstring_hint["runtime_hints"].append({
            "key": "preferredCamera",
            "value": 7,
        })
        nonstring_hint_path = temp_path / "nonstring-hint.pulp3d.json"
        write_sidecar(nonstring_hint_path, nonstring_hint)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            nonstring_hint_path,
            args.preflight_tool)
        expect_failure(
            "nonstring-runtime-hint",
            result,
            "sidecar_runtime_hints_0_value_valid=false",
            errors,
        )

        unclassified_feature = json.loads(json.dumps(base))
        unclassified_feature["unsupported_features"].append({
            "feature": "FutureRuntimeGap",
            "reason": "Future feature names must not pass artifact verification until preflight classifies them.",
            "node_path": "/Scene/Future",
        })
        unclassified_path = temp_path / "unclassified-feature.pulp3d.json"
        write_sidecar(unclassified_path, unclassified_feature)
        fake_preflight = temp_path / "fake-preflight-mismatched-feature.py"
        write_fake_preflight_with_mismatched_feature(fake_preflight)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            unclassified_path,
            fake_preflight)
        expect_failure(
            "unclassified-unsupported-feature",
            result,
            "unsupported_features_classified=false",
            errors,
        )

        fake_preflight = temp_path / "fake-preflight-row-drift.py"
        write_fake_preflight_with_row_count_drift(fake_preflight)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            args.base_sidecar,
            fake_preflight)
        expect_failure(
            "preflight-row-count-drift",
            result,
            "preflight_row_counts_valid=false",
            errors,
        )

        diagnostic_drift = json.loads(json.dumps(base))
        diagnostic_drift["diagnostics"].append({
            "severity": "warning",
            "code": "scene.warning",
            "message": "Fake preflight must not hide sidecar diagnostics.",
            "path": "fixture.glb",
        })
        diagnostic_drift_path = temp_path / "diagnostic-count-drift.pulp3d.json"
        write_sidecar(diagnostic_drift_path, diagnostic_drift)
        fake_preflight = temp_path / "fake-preflight-sidecar-drift.py"
        write_fake_preflight_with_sidecar_count_drift(fake_preflight)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            diagnostic_drift_path,
            fake_preflight)
        expect_failure(
            "preflight-sidecar-count-drift",
            result,
            "preflight_sidecar_counts_valid=false",
            errors,
        )

        fake_preflight = temp_path / "fake-preflight-readiness-drift.py"
        write_fake_preflight_with_readiness_drift(fake_preflight)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            args.base_sidecar,
            fake_preflight)
        expect_failure(
            "preflight-readiness-drift",
            result,
            "preflight_readiness_consistent=false",
            errors,
        )

        fake_preflight = temp_path / "fake-preflight-exit-drift.py"
        write_fake_preflight_with_exit_code_drift(fake_preflight)
        result = run_artifact_verifier(
            args.artifact_verifier,
            args.asset,
            args.base_sidecar,
            fake_preflight)
        expect_failure(
            "preflight-exit-code-drift",
            result,
            "preflight_exit_code_consistent=false",
            errors,
        )

    if errors:
        for error in errors:
            print(f"error={error}")
        return 1
    print("bake_artifact_contract_verified=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
