# `.pulp/identity.lock` — plugin identity pin

`.pulp/identity.lock` is a committed lockfile that records the
**stable identity** of every plugin in a Pulp project: AU four-character
component subtype + manufacturer code, AAX product code, optional
VST3 FUID, optional CLAP plugin id, and the project version those
fields are released against. The lock travels with the source so
unintended drift in any of these fields surfaces at review time
instead of after a release — a changed AU 4CC or VST3 FUID silently
invalidates every saved DAW session that referenced the previous
identity.

This document describes the file format. The CLI surface that
reads and writes it is `pulp identity record` /
`pulp identity check`, with the same checks wired into
`pulp build --check-identity` (see Track 3.12 of
`planning/2026-05-24-macos-plugin-authoring-plan.md`).

## Where it lives

```
<project-root>/.pulp/identity.lock
```

`.pulp/` is created automatically by `pulp identity record` when
needed. The directory is reserved for committed CLI-managed state;
do not put scratch files there.

## Format (schema = 1)

The lockfile is TOML. The on-disk shape is intentionally narrow so a
human reviewer can read a `git diff` and immediately tell whether a
change is a typo, a rename, or a deliberate identity flip.

```toml
schema = 1

[[plugins]]
target               = "PulpGain"      # CMake target name
plugin_name          = "PulpGain"      # PLUGIN_NAME or target
manufacturer         = "Pulp"
bundle_id            = "com.pulp.gain"
version              = "1.0.0"
au_plugin_code       = "PGan"          # 4 chars, PLUGIN_CODE
au_manufacturer_code = "Pulp"          # 4 chars, MANUFACTURER_CODE
aax_product_code     = "PGaP"          # 4 chars, AAX_PRODUCT_CODE
# Optional — empty strings are omitted on write.
# vst3_fuid          = "Steinberg::FUID(0x..., 0x..., 0x..., 0x...)"
# clap_plugin_id     = "com.pulp.gain"
```

### Field reference

| Field                  | Required | Source                                   | Drift = lost compat in |
|------------------------|----------|------------------------------------------|------------------------|
| `target`               | yes      | `pulp_add_plugin(<target> ...)`          | n/a — lookup key       |
| `plugin_name`          | yes      | `PLUGIN_NAME` (default = `target`)       | Display name only      |
| `manufacturer`         | yes      | `MANUFACTURER`                           | All formats            |
| `bundle_id`            | yes      | `BUNDLE_ID`                              | macOS bundle id, AU    |
| `version`              | yes      | `VERSION`                                | Host upgrade banners   |
| `au_plugin_code`       | optional | `PLUGIN_CODE` (4-char ASCII)             | **AU saved sessions**  |
| `au_manufacturer_code` | optional | `MANUFACTURER_CODE` (4-char ASCII)       | **AU saved sessions**  |
| `aax_product_code`     | optional | `AAX_PRODUCT_CODE` (4-char ASCII)        | AAX saved sessions     |
| `vst3_fuid`            | optional | `Steinberg::FUID(...)` literal in source | **VST3 saved sessions**|
| `clap_plugin_id`       | optional | CLAP plugin descriptor (reverse-DNS)     | **CLAP saved sessions**|

"Optional" means the field is permitted to be the empty string in the
lock — typically because the project hasn't pinned that identity
surface yet. An empty recorded value is treated as **"not yet pinned"**
and never triggers drift; the recorder fills it in on the next
`pulp identity record` run. A non-empty recorded value that no longer
matches the current source **is** drift and blocks the build unless
the developer explicitly accepts the change.

### Schema versioning

`schema` is an integer. The current schema is `1`. The CLI refuses
to parse a lock whose `schema` exceeds the version it understands; a
schema bump is reserved for required-field additions or layout
changes that would otherwise silently lose information.

## Workflows

### First-time setup

```bash
cd <project>
pulp identity record
git add .pulp/identity.lock
git commit -m "chore: pin plugin identity"
```

### Continuous integration

Add `pulp build --check-identity` to the CI build step. Any PR that
changes an AU 4CC, VST3 FUID, CLAP id, or AAX product code without
also re-running `pulp identity record --allow-identity-change` fails
the build with a per-field diff:

```
pulp identity check: drift detected:
  - PulpGain.au_plugin_code: lock="PGan" vs current="PGn2"
pulp identity check: run `pulp identity record --allow-identity-change`
to accept the change, or revert the source to match the lock.
```

### Deliberately changing an identity

A deliberate identity change is rare — it loses host compatibility
with every saved session that referenced the previous identity. When
it does happen, accept the change explicitly:

```bash
pulp identity record --allow-identity-change
git diff .pulp/identity.lock        # review the change
git commit .pulp/identity.lock
```

The diff against the previous lock is the audit trail. Reviewers
should treat any non-trivial change to this file the same way they
treat a public API rename — it's a breaking change that needs an
explicit decision.

## CLI reference

| Command                                              | Effect                                                                                     | Exit code |
|------------------------------------------------------|--------------------------------------------------------------------------------------------|-----------|
| `pulp identity record`                               | Refresh the lockfile from the project's current state. Refuses to overwrite drifted entries. | 0 ok / 1 drift refused |
| `pulp identity record --allow-identity-change`       | As above but accepts drift and writes anyway.                                              | 0         |
| `pulp identity record --dry-run`                     | Print the would-be lock without writing it.                                                | 0         |
| `pulp identity check`                                | Compare current state against the lock. No writes.                                         | 0 ok / 1 drift |
| `pulp identity check --allow-identity-change`        | As above but treats drift as success (still prints the diff).                              | 0         |
| `pulp build --check-identity`                        | Run `identity check` before configuring; abort the build on drift.                         | 0 ok / 1 drift |
| `pulp build --check-identity --allow-identity-change`| As above but treats drift as a warning, not a build break.                                 | 0         |

## Implementation notes

- The recorder parses `pulp_add_plugin(...)` blocks out of the
  project's top-level `CMakeLists.txt`. A follow-up slice can teach
  it to follow `add_subdirectory` / `include` directives if real
  projects start hiding plugin declarations in nested files.
- `vst3_fuid` and `clap_plugin_id` are present in the schema today
  but the recorder leaves them blank pending a regex shape for the
  source-side scrape. They can be edited by hand in the lock without
  breaking the check — the comparison only fires when both sides
  carry a non-empty value.
- The lockfile sort order is stable (`plugins` sorted by `target`)
  so review diffs stay readable across recordings.

## See also

- `planning/2026-05-24-macos-plugin-authoring-plan.md` § 3.12
- `core/format/include/pulp/format/processor.hpp` — `PluginDescriptor`
- `tools/cmake/PulpUtils.cmake` — `pulp_add_plugin(...)` definition
