// doctor.rs — stubbed `pulp doctor` command for Phase 1.
//
// Phase 2 will port the real version-diagnostics logic from the C++ CLI:
//   - read CMakeLists.txt VERSION
//   - read pulp.toml for sdk_version + cli_min_version
//   - read .claude-plugin/plugin.json
//   - read ~/.pulp/projects.json
//   - compose the `findings` array with the same semantics
//
// For now, emit the same top-level JSON shape with empty values so downstream
// tests (and any human eyeballing the output) can confirm the schema matches.

use anyhow::Result;
use serde_json::json;

/// Entry point for `pulp-rs doctor`.
///
/// - `versions && json`  -> print the empty version-diagnostics JSON shape.
/// - `versions && !json` -> print a human-readable stub line.
/// - otherwise            -> print a generic stub line.
pub fn run(versions: bool, json: bool) -> Result<()> {
    if versions && json {
        let shape = json!({
            "cli": {},
            "plugin": {},
            "plugin_min_cli": {},
            "plugin_json_path": "",
            "project_root": "",
            "project_sdk": {},
            "project_cli_min": {},
            "projects": [],
            "findings": []
        });
        // `to_string_pretty` keeps the output human-inspectable; the integration
        // test parses via serde_json and doesn't care about formatting.
        println!("{}", serde_json::to_string_pretty(&shape)?);
        return Ok(());
    }

    if versions {
        println!("pulp-rs doctor --versions (stub, Phase 1)");
        return Ok(());
    }

    println!("pulp-rs doctor (stub, Phase 1)");
    Ok(())
}
