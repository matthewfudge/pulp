//! `pulp-rs projects …` subcommand tree.
//!
//! # Scope
//!
//! Phase 4 ports `projects list` — human and `--json` output — with
//! parity against the C++ human output. `add` and `remove` are NOT
//! ported yet; they stay on the C++ binary until a later phase.
//!
//! # JSON shape
//!
//! The C++ CLI has no `--json` flag on `projects list` today. The
//! Rust port introduces one with this deliberately small shape:
//!
//! ```json
//! {
//!   "registry": "/path/to/projects.json",
//!   "projects": [
//!     {
//!       "path": "/abs/path",
//!       "name": "display name",
//!       "registered_at": "2026-04-23T10:00:00Z",
//!       "missing_on_disk": false
//!     }
//!   ]
//! }
//! ```
//!
//! Design rationale:
//!
//! - `registry` is explicit so tools don't have to reimplement the
//!   `$PULP_HOME` precedence rule just to know which file they're
//!   reading.
//! - `path` / `name` / `registered_at` mirror the on-disk schema
//!   one-for-one.
//! - `missing_on_disk` is *derived* (stat-at-read-time) and included
//!   because every real use case — lint, CI report, CLI upgrade
//!   suggester — needs it and would otherwise call `stat` themselves.
//! - Unknown fields are silently tolerated on read (see
//!   `registry::Project` serde defaults) — the JSON writer stays in
//!   lockstep with the reader.

use std::io::Write;
use std::path::{Path, PathBuf};

use serde_json::{json, Value};

use crate::color;
use crate::error::{CliError, Result};
use crate::registry::{self, Project};

/// Subcommands under `pulp-rs projects`.
#[derive(Debug, Clone, Copy)]
pub enum Sub {
    /// Show registered projects.
    List,
}

/// Parse the post-`projects` argument slice into a [`Sub`].
///
/// Unknown subcommands map to [`CliError::UnknownSubcommand`]; `help`
/// and `-h` short-circuit via the caller so we don't flatten them
/// here.
///
/// # Errors
///
/// Returns [`CliError::UnknownSubcommand`] for any unrecognised input.
pub fn parse_sub(args: &[String]) -> Result<Sub> {
    match args.first().map(String::as_str) {
        Some("list" | "ls") => Ok(Sub::List),
        _ => Err(CliError::UnknownSubcommand),
    }
}

/// Run the `projects` subcommand using the ambient `$PULP_HOME` /
/// `~/.pulp` registry path.
///
/// # Errors
///
/// Surfaces I/O failures on the output stream. Registry read failures
/// are swallowed into an empty list (matching the C++ policy).
pub fn run(sub: Sub, json: bool, out: &mut impl Write) -> Result<()> {
    let reg_path = registry::registry_path().unwrap_or_default();
    run_with_registry(sub, &reg_path, json, out)
}

/// Same as [`run`] but takes an explicit registry path. Tests use this
/// so they don't have to mutate `$PULP_HOME` (which would race with
/// other tests in the same process).
///
/// # Errors
///
/// See [`run`].
pub fn run_with_registry(
    sub: Sub,
    reg_path: &Path,
    json: bool,
    out: &mut impl Write,
) -> Result<()> {
    match sub {
        Sub::List => do_list(reg_path, json, out),
    }
}

fn do_list(reg_path: &Path, json: bool, out: &mut impl Write) -> Result<()> {
    let projects = registry::read(reg_path);

    // Annotate each project with missing_on_disk at render time. The
    // registry struct itself doesn't carry this so readers that don't
    // care (e.g. the on-disk writer) don't pay for the stat.
    let annotated: Vec<(Project, bool)> = projects
        .into_iter()
        .map(|p| {
            let missing = !Path::new(&p.path).exists();
            (p, missing)
        })
        .collect();

    if json {
        render_json(reg_path, &annotated, out)
    } else {
        render_human(reg_path, &annotated, out)
    }
}

fn render_json(reg_path: &Path, projects: &[(Project, bool)], out: &mut impl Write) -> Result<()> {
    let entries: Vec<Value> = projects
        .iter()
        .map(|(p, missing)| {
            json!({
                "path": p.path,
                "name": display_name(p),
                "registered_at": p.registered_at,
                "missing_on_disk": missing,
            })
        })
        .collect();
    let mut obj = serde_json::Map::new();
    obj.insert("registry".to_owned(), json!(generic_str(reg_path)));
    obj.insert("projects".to_owned(), Value::Array(entries));
    let rendered =
        serde_json::to_string_pretty(&Value::Object(obj)).unwrap_or_else(|_| "{}".to_owned());
    writeln!(out, "{rendered}").map_err(|e| CliError::io("<stdout>", e))
}

fn render_human(reg_path: &Path, projects: &[(Project, bool)], out: &mut impl Write) -> Result<()> {
    writeln!(out, "Registry: {}", reg_path.display()).map_err(|e| CliError::io("<stdout>", e))?;

    if projects.is_empty() {
        writeln!(out, "  (no projects registered)").map_err(io_err)?;
        writeln!(
            out,
            "  Run `pulp projects add` in a project directory to register it,"
        )
        .map_err(io_err)?;
        writeln!(
            out,
            "  or use `pulp doctor --versions --scan-parents` to surface"
        )
        .map_err(io_err)?;
        writeln!(out, "  ancestors without modifying the registry.").map_err(io_err)?;
        return Ok(());
    }

    let noun = if projects.len() == 1 {
        "project"
    } else {
        "projects"
    };
    writeln!(out, "  {} {noun}:", projects.len()).map_err(io_err)?;

    for (p, missing) in projects {
        write!(out, "  - {}", display_name(p)).map_err(io_err)?;
        if *missing {
            write!(out, " {}(missing){}", color::yellow(), color::reset()).map_err(io_err)?;
        }
        writeln!(out).map_err(io_err)?;

        write!(out, "      {}{}{}", color::dim(), p.path, color::reset()).map_err(io_err)?;
        if !p.registered_at.is_empty() {
            write!(out, "  registered {}", p.registered_at).map_err(io_err)?;
        }
        writeln!(out).map_err(io_err)?;
    }
    Ok(())
}

#[inline]
fn io_err(e: std::io::Error) -> CliError {
    CliError::io("<stdout>", e)
}

/// Fallback rule matches `cmd_projects.cpp`: if the stored `name` is
/// empty, use the path's basename.
fn display_name(p: &Project) -> String {
    if p.name.is_empty() {
        PathBuf::from(&p.path)
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_default()
    } else {
        p.name.clone()
    }
}

fn generic_str(p: &Path) -> String {
    if p.as_os_str().is_empty() {
        String::new()
    } else {
        p.to_string_lossy().replace('\\', "/")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    fn plant_registry(entries: &[(&str, &str, &str)]) -> (tempfile::TempDir, PathBuf) {
        use std::fmt::Write as _;
        let home = tempfile::tempdir().unwrap();
        let reg_path = home.path().join("projects.json");
        let mut body = String::from(r#"{"projects":["#);
        for (i, (path, name, ts)) in entries.iter().enumerate() {
            if i > 0 {
                body.push(',');
            }
            write!(
                &mut body,
                r#"{{"path":"{path}","name":"{name}","registered_at":"{ts}"}}"#
            )
            .unwrap();
        }
        body.push_str("]}");
        let mut f = std::fs::File::create(&reg_path).unwrap();
        f.write_all(body.as_bytes()).unwrap();
        (home, reg_path)
    }

    #[test]
    fn it_renders_empty_registry_in_json_lane() {
        let home = tempfile::tempdir().unwrap();
        let reg = home.path().join("projects.json");
        let mut buf = Vec::new();
        run_with_registry(Sub::List, &reg, true, &mut buf).unwrap();
        let v: Value = serde_json::from_slice(&buf).unwrap();
        assert_eq!(v["projects"].as_array().unwrap().len(), 0);
        assert!(v["registry"].as_str().unwrap().contains("projects.json"));
    }

    #[test]
    fn it_renders_populated_registry_in_json_lane() {
        let (_home, reg) =
            plant_registry(&[("/tmp/does-not-exist", "ExampleA", "2026-04-22T00:00:00Z")]);
        let mut buf = Vec::new();
        run_with_registry(Sub::List, &reg, true, &mut buf).unwrap();
        let v: Value = serde_json::from_slice(&buf).unwrap();
        let entries = v["projects"].as_array().unwrap();
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0]["name"], "ExampleA");
        assert_eq!(entries[0]["missing_on_disk"], true);
    }

    #[test]
    fn it_renders_empty_registry_in_human_lane() {
        let home = tempfile::tempdir().unwrap();
        let reg = home.path().join("projects.json");
        let mut buf = Vec::new();
        run_with_registry(Sub::List, &reg, false, &mut buf).unwrap();
        let s = String::from_utf8(buf).unwrap();
        assert!(s.starts_with("Registry: "));
        assert!(s.contains("(no projects registered)"));
    }

    #[test]
    fn it_pluralises_project_count_in_human_lane() {
        let (_home, reg) = plant_registry(&[
            ("/tmp/a", "A", "2026-04-22T00:00:00Z"),
            ("/tmp/b", "B", "2026-04-22T00:00:00Z"),
        ]);
        let mut buf = Vec::new();
        run_with_registry(Sub::List, &reg, false, &mut buf).unwrap();
        let s = String::from_utf8(buf).unwrap();
        assert!(s.contains("2 projects:"));
    }

    #[test]
    fn it_uses_singular_for_one() {
        let (_home, reg) = plant_registry(&[("/tmp/solo", "Solo", "2026-04-22T00:00:00Z")]);
        let mut buf = Vec::new();
        run_with_registry(Sub::List, &reg, false, &mut buf).unwrap();
        let s = String::from_utf8(buf).unwrap();
        assert!(s.contains("1 project:"));
    }

    #[test]
    fn parse_sub_accepts_list_alias_ls() {
        assert!(matches!(parse_sub(&["ls".to_owned()]).unwrap(), Sub::List));
    }

    #[test]
    fn parse_sub_rejects_unknown() {
        assert!(matches!(
            parse_sub(&["wat".to_owned()]),
            Err(CliError::UnknownSubcommand)
        ));
    }
}
