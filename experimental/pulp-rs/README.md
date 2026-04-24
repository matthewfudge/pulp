# pulp-rs (experimental)

This is the Phase 1 scaffold of an experimental Rust prototype to evaluate
whether the Pulp CLI (`tools/cli/*.cpp`) should be rewritten in Rust.
**Not shipping. Not user-facing. Not wired into any Pulp build.**

See GitHub issue [#686](https://github.com/danielraffel/pulp/issues/686) for
the full evaluation framework and decision criteria.

## Build and run

```bash
cd experimental/pulp-rs
cargo build --release
cargo run -- doctor --versions --json
```

## Test

```bash
cd experimental/pulp-rs
cargo test
```

The one integration test (`tests/smoke_test.rs`) shells out to the built
binary and asserts that `doctor --versions --json` emits a JSON object
containing the 9 top-level keys Phase 2 will populate.

## Decision framework (from issue #686)

A Rust rewrite is only worthwhile if it delivers on all six of these:

- **Parity** — behavior-for-behavior match with the existing C++ CLI on
  every command, flag, and exit code users currently depend on.
- **Lower complexity** — the Rust implementation is meaningfully simpler
  to read, modify, and onboard contributors to than the C++ one.
- **Better test ergonomics** — integration tests, fixtures, and CI
  feedback loops are faster and easier to write than with the current
  Catch2 + CMake harness.
- **No production coupling** — the prototype stays isolated from the
  shipping codebase until a full cutover is justified; no partial
  integrations, no shared state.
- **Cross-platform CI** — the Rust build runs green on macOS, Linux,
  and Windows with the same effort (or less) as the C++ CLI.
- **Clear migration boundary** — there is a single, reviewable cutover
  point from C++ to Rust, not a long coexistence that doubles the
  maintenance burden.

## Current phase

**Phase 1 — scaffold only.** This crate exposes a clap-based CLI
skeleton with a stubbed `doctor` subcommand. It does not read any
project files yet; `doctor --versions --json` emits an empty-valued
JSON shape so tests can lock in the schema.

**Phase 2** will port `doctor --versions --json` to parity with the
C++ implementation: read `CMakeLists.txt` VERSION, read `pulp.toml`
(sdk_version + cli_min_version), read `.claude-plugin/plugin.json`,
read `~/.pulp/projects.json`, and compose the `findings` array.

## Why this directory isn't in CMake

Deliberate. The prototype runs entirely via `cargo`. CMake never
discovers it. No Pulp build, test, or ship flow is affected by
anything under `experimental/pulp-rs/`. The only way to exercise this
crate is to `cd experimental/pulp-rs && cargo <subcommand>` — which
keeps the main Pulp CI and release pipeline completely untouched
while the evaluation is in progress.
