//! `pulp-rs doctor [--versions] [--json]` entry point.
//!
//! Phase 2 ports the `--versions --json` lane to parity with the C++
//! writer. The other combinations still stub to keep the CLI surface
//! stable while the prototype grows.

use std::io::Write;

use crate::diag;
use crate::error::Result;

/// Run the `doctor` subcommand.
///
/// # Errors
///
/// Returns an error only if `std::env::current_dir()` fails, which in
/// practice means the process has no working directory — essentially
/// unreachable in a real shell.
pub fn run(versions: bool, json: bool, out: &mut impl Write) -> Result<()> {
    if versions && json {
        let cwd = std::env::current_dir().map_err(|e| crate::error::CliError::io(".", e))?;
        let diag = diag::collect(&cwd)?;
        writeln!(out, "{}", diag::emit_json(&diag))
            .map_err(|e| crate::error::CliError::io("<stdout>", e))?;
        return Ok(());
    }
    if versions {
        writeln!(
            out,
            "pulp-rs doctor --versions (human lane not ported in Phase 2)"
        )
        .map_err(|e| crate::error::CliError::io("<stdout>", e))?;
        return Ok(());
    }
    writeln!(
        out,
        "pulp-rs doctor (stub — Phase 2 only ports --versions --json)"
    )
    .map_err(|e| crate::error::CliError::io("<stdout>", e))?;
    Ok(())
}
