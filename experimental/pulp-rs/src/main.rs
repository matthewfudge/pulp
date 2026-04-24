//! `pulp-rs` binary entry point.
//!
//! This file is intentionally thin: parse flags with `clap`, pick the
//! right `cmd::*` dispatcher, map library errors to process exit codes
//! that match the C++ CLI.

use std::io::{self, Write};
use std::process::ExitCode;

use clap::{Parser, Subcommand};
use pulp_rs::cmd;

/// Prototype version string printed by `pulp-rs version`.
const VERSION_STRING: &str = "pulp-rs v0.0.1-experimental";

#[derive(Parser, Debug)]
#[command(
    name = "pulp-rs",
    about = "Experimental Rust prototype of the Pulp CLI (not for production)",
    disable_version_flag = true,
    disable_help_subcommand = true
)]
struct Cli {
    #[command(subcommand)]
    command: Option<Command>,
}

#[derive(Subcommand, Debug)]
enum Command {
    /// Print the prototype's version string.
    Version,

    /// Environment diagnostics. Phase 2 ports `--versions --json`.
    Doctor(DoctorArgs),

    /// Manage the `~/.pulp/projects.json` registry. Phase 4 ports `list`.
    Projects(ProjectsArgs),
}

#[derive(clap::Args, Debug)]
struct DoctorArgs {
    /// Emit the version-diagnostics view (matches `pulp doctor --versions`).
    #[arg(long)]
    versions: bool,

    /// Emit JSON output instead of human-readable text.
    #[arg(long)]
    json: bool,
}

#[derive(clap::Args, Debug)]
struct ProjectsArgs {
    /// `list` (or `ls`). Other subcommands (`add`, `remove`) are not
    /// ported yet — use the C++ CLI for those.
    subcommand: Option<String>,

    /// Emit JSON instead of the human-readable table.
    #[arg(long)]
    json: bool,
}

fn main() -> ExitCode {
    match real_main() {
        Ok(()) => ExitCode::SUCCESS,
        Err(code) => code,
    }
}

fn real_main() -> Result<(), ExitCode> {
    let cli = match Cli::try_parse() {
        Ok(cli) => cli,
        Err(err) => return Err(clap_exit_code(&err)),
    };

    let stdout = io::stdout();
    let mut out = stdout.lock();

    let command = cli.command.ok_or_else(|| {
        eprintln!("unknown subcommand");
        ExitCode::from(2)
    })?;

    match command {
        Command::Version => {
            writeln!(out, "{VERSION_STRING}").map_err(|e| io_exit(&e))?;
            Ok(())
        }
        Command::Doctor(args) => {
            cmd::doctor::run(args.versions, args.json, &mut out).map_err(|e| {
                eprintln!("pulp-rs: {e}");
                ExitCode::from(1)
            })
        }
        Command::Projects(args) => {
            // Treat `pulp-rs projects` with no subcommand as `list` —
            // consistent with the C++ CLI printing help + exit(1), but
            // the prototype's default target today is strictly `list`
            // since that's what Phase 4 ports.
            let args_vec = args.subcommand.clone().map(|s| vec![s]).unwrap_or_default();
            let sub = cmd::projects::parse_sub(&args_vec).map_err(|_| {
                eprintln!("pulp-rs projects: unknown subcommand");
                eprintln!("  only `list` / `ls` is ported; use the C++ CLI for add/remove");
                ExitCode::from(2)
            })?;
            cmd::projects::run(sub, args.json, &mut out).map_err(|e| {
                eprintln!("pulp-rs: {e}");
                ExitCode::from(1)
            })
        }
    }
}

fn clap_exit_code(err: &clap::error::Error) -> ExitCode {
    use clap::error::ErrorKind;
    match err.kind() {
        ErrorKind::InvalidSubcommand | ErrorKind::UnknownArgument => {
            // Match the C++ CLI's wording exactly — the parity test
            // on subcommand errors greps for this string.
            eprintln!("unknown subcommand");
            ExitCode::from(2)
        }
        ErrorKind::DisplayHelp | ErrorKind::DisplayVersion => {
            let _ = err.print();
            ExitCode::SUCCESS
        }
        _ => {
            let _ = err.print();
            ExitCode::from(2)
        }
    }
}

fn io_exit(e: &io::Error) -> ExitCode {
    eprintln!("pulp-rs: {e}");
    ExitCode::from(1)
}
