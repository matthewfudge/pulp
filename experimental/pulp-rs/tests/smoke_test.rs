// smoke_test.rs — minimal Phase 1 integration test.
//
// Shells out to the built `pulp-rs` binary and asserts that
// `doctor --versions --json` emits valid JSON containing all
// 9 top-level keys that Phase 2 will populate with real data.

use std::process::Command;

use serde_json::Value;

const REQUIRED_KEYS: &[&str] = &[
    "cli",
    "plugin",
    "plugin_min_cli",
    "plugin_json_path",
    "project_root",
    "project_sdk",
    "project_cli_min",
    "projects",
    "findings",
];

#[test]
fn doctor_versions_json_has_required_shape() {
    // Cargo sets CARGO_BIN_EXE_<name> for integration tests and rebuilds
    // the binary on demand, so we don't need to invoke `cargo build` here.
    let bin = env!("CARGO_BIN_EXE_pulp-rs");

    let output = Command::new(bin)
        .args(["doctor", "--versions", "--json"])
        .output()
        .expect("failed to run pulp-rs binary");

    assert!(
        output.status.success(),
        "pulp-rs exited with {:?}; stderr: {}",
        output.status.code(),
        String::from_utf8_lossy(&output.stderr),
    );

    let parsed: Value = serde_json::from_slice(&output.stdout)
        .expect("pulp-rs stdout was not valid JSON");

    let obj = parsed
        .as_object()
        .expect("top-level JSON value must be an object");

    for key in REQUIRED_KEYS {
        assert!(
            obj.contains_key(*key),
            "missing required key `{key}` in doctor --versions --json output; got keys: {:?}",
            obj.keys().collect::<Vec<_>>(),
        );
    }
}
