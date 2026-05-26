//! Integration smoke for `pulp identity record` / `pulp identity check`.
//!
//! Drives the real binary through `assert_cmd` so the CLI surface is
//! exercised end-to-end: arg parsing, project-root discovery,
//! CMakeLists.txt scan, lockfile write, and the drift-detection
//! exit-code semantics that `pulp build --check-identity` and CI
//! gates depend on. The unit tests in `cmd::identity::tests` cover
//! the parser / diff machinery directly; this file pins the binary
//! contract.

use std::fs;

use predicates::prelude::*;

const SAMPLE_CMAKE: &str = r#"
cmake_minimum_required(VERSION 3.24)
project(SmokeProj VERSION 1.0.0)

pulp_add_plugin(SmokeGain
    FORMATS         VST3 AU CLAP
    PLUGIN_NAME     "Smoke Gain"
    BUNDLE_ID       "com.smoke.gain"
    MANUFACTURER    "SmokeCo"
    VERSION         "1.0.0"
    PLUGIN_CODE     "Smkg"
    MANUFACTURER_CODE "Smco"
    AAX_PRODUCT_CODE "SmkP"
)
"#;

fn setup_project() -> tempfile::TempDir {
    let td = tempfile::tempdir().unwrap();
    fs::write(td.path().join("pulp.toml"), "sdk_version = \"0.40.0\"\n").unwrap();
    fs::write(td.path().join("CMakeLists.txt"), SAMPLE_CMAKE).unwrap();
    td
}

#[test]
fn identity_record_then_check_round_trip() {
    let td = setup_project();

    // record → exit 0, lockfile present.
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "record"])
        .assert()
        .success()
        .stdout(predicate::str::contains(".pulp/identity.lock"));
    assert!(td.path().join(".pulp/identity.lock").is_file());

    // check → exit 0, OK message.
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "check"])
        .assert()
        .success()
        .stdout(predicate::str::contains("OK"));
}

#[test]
fn identity_check_fails_on_drifted_au_code() {
    let td = setup_project();

    // Record first.
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "record"])
        .assert()
        .success();

    // Drift the AU 4CC.
    let drifted = SAMPLE_CMAKE.replace("PLUGIN_CODE     \"Smkg\"", "PLUGIN_CODE     \"Smk2\"");
    fs::write(td.path().join("CMakeLists.txt"), drifted).unwrap();

    // check → exit 1, names the drifted field.
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "check"])
        .assert()
        .failure()
        .code(1)
        .stdout(predicate::str::contains("au_plugin_code"))
        .stdout(predicate::str::contains("SmokeGain"));

    // check --allow-identity-change → exit 0.
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "check", "--allow-identity-change"])
        .assert()
        .success();
}

#[test]
fn identity_record_dry_run_does_not_write_lock() {
    let td = setup_project();

    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity", "record", "--dry-run"])
        .assert()
        .success()
        .stdout(predicate::str::contains("SmokeGain"))
        .stdout(predicate::str::contains("dry-run"));
    assert!(!td.path().join(".pulp/identity.lock").exists());
}

#[test]
fn identity_help_lists_subcommands() {
    // No args = help (parity with `pulp identity help`).
    let td = setup_project();
    assert_cmd::Command::cargo_bin("pulp")
        .unwrap()
        .current_dir(td.path())
        .args(["identity"])
        .assert()
        .success()
        .stdout(predicate::str::contains("record"))
        .stdout(predicate::str::contains("check"));
}
