# Pulp Create — Standalone SDK Bootstrapping

**Date:** 2026-03-28
**Status:** Draft
**Depends on:** First-time setup (complete)

---

## Product Rule

If `pulp create hello` cannot yield a buildable plugin from a fresh machine after `pulp install`, the install UX is not done.

## Design

`pulp create` is the single command for project creation. No aliases (`new` is removed). Its behavior adapts to context:

```
pulp create my-plugin
  │
  ├─ Inside a Pulp repo checkout?
  │   YES → use local framework source (contributor mode)
  │   NO  → check ~/.pulp/sdk/<version>/
  │          ├─ Cached? → use it
  │          └─ Not cached? → fetch pinned SDK release → cache → use it
  │
  ├─ Run doctor checks (compiler, cmake, git-lfs)
  ├─ Scaffold project from templates
  ├─ Configure + build + test
  └─ Report artifacts
```

The user never thinks about this. They run `pulp create` and it works.

## SDK Package

Each Pulp release ships an SDK tarball per platform alongside the CLI binary:

```
GitHub Release v0.1.0:
  pulp-darwin-arm64.tar.gz        ← CLI binary
  pulp-darwin-x64.tar.gz
  pulp-linux-x64.tar.gz
  pulp-linux-arm64.tar.gz
  pulp-windows-x64.zip
  pulp-sdk-darwin-arm64.tar.gz    ← SDK package (NEW)
  pulp-sdk-darwin-x64.tar.gz
  pulp-sdk-linux-x64.tar.gz
  pulp-sdk-linux-arm64.tar.gz
  pulp-sdk-windows-x64.tar.gz
```

### SDK Contents

```
pulp-sdk-<platform>/
  version.txt                     ← "0.1.0"
  include/                        ← all public headers (pulp/format/, pulp/state/, etc.)
  lib/                            ← pre-built static libraries (libpulp-format.a, etc.)
  cmake/
    PulpConfig.cmake              ← find_package(Pulp) support
    PulpTargets.cmake             ← imported targets
    PulpPlugin.cmake              ← pulp_add_plugin() macro
  templates/
    effect/                       ← project templates
    instrument/
    app/
    bare/
  external/
    clap/include/                 ← CLAP headers (MIT)
    lv2/include/                  ← LV2 headers (ISC)
    vst3sdk/pluginterfaces/       ← VST3 public headers (MIT — pluginterfaces only)
```

### What's NOT in the SDK

- Skia binaries (too large — fetched separately on first build via CMake FetchContent or a pulp-managed cache)
- AudioUnitSDK (Apache 2.0 — cloned on demand, macOS only)
- Full VST3 SDK source (only pluginterfaces/ headers needed for building against)
- Test framework (Catch2 — fetched by CMake if tests are enabled)

### Licensing

Everything in the SDK must be distributable under MIT-compatible terms:
- Pulp headers/libs: MIT
- CLAP headers: MIT
- LV2 headers: ISC
- VST3 pluginterfaces: MIT (confirmed — `pluginterfaces/` is MIT-licensed separately from the full SDK)

## Local Cache Layout

```
~/.pulp/
  bin/
    pulp                          ← CLI binary
  sdk/
    0.1.0/                        ← SDK for this version
      include/
      lib/
      cmake/
      templates/
      external/
    0.2.0/                        ← multiple versions can coexist
  cache/
    skia-darwin-arm64.tar.gz      ← downloaded binary assets (optional)
```

## CLI Behavior Changes

### `pulp create` (outside a repo)

1. Determine SDK version: CLI has a built-in `PULP_SDK_VERSION` constant
2. Check `~/.pulp/sdk/<version>/version.txt`
3. If missing: download `pulp-sdk-<platform>.tar.gz` from GitHub Releases, extract to `~/.pulp/sdk/<version>/`
4. Run doctor checks
5. Scaffold project using `~/.pulp/sdk/<version>/templates/`
6. Generate CMakeLists.txt that uses `find_package(Pulp)` with `CMAKE_PREFIX_PATH` pointing to the SDK
7. Build + test + report

### `pulp create` (inside a repo)

Unchanged — uses local source, relative paths, existing templates. Detected by walking up looking for `CMakeLists.txt + core/` (same as today).

### Scaffolded CMakeLists.txt — Two Modes

**Inside repo (today):**
```cmake
# Uses relative paths to framework source
add_executable(MyPlugin-test test_my_plugin.cpp)
target_link_libraries(MyPlugin-test PRIVATE pulp::format Catch2::Catch2WithMain)
pulp_add_plugin(MyPlugin FORMATS VST3 CLAP AU Standalone ...)
```

**Standalone (new):**
```cmake
cmake_minimum_required(VERSION 3.24)
project(MyPlugin VERSION 1.0.0)

find_package(Pulp 0.1.0 REQUIRED)

add_executable(MyPlugin-test test_my_plugin.cpp)
target_link_libraries(MyPlugin-test PRIVATE Pulp::format)
pulp_add_plugin(MyPlugin FORMATS VST3 CLAP AU Standalone ...)
```

The `PulpConfig.cmake` in the SDK defines `Pulp::format`, `Pulp::state`, `Pulp::audio`, `Pulp::signal` as imported targets, and provides the `pulp_add_plugin()` macro.

## SDK Build Process

Added to `release-cli-local.sh` and `release-cli.yml`:

1. Build the framework in Release mode
2. Run `cmake --install` to a staging directory with all headers + libs
3. Copy `tools/templates/`
4. Copy format SDK headers (CLAP, LV2, VST3 pluginterfaces)
5. Generate `PulpConfig.cmake` from a template
6. Package as `pulp-sdk-<platform>.tar.gz`

## Version Pinning

Each scaffolded project gets a `pulp.toml` at its root:

```toml
[pulp]
sdk_version = "0.1.0"
```

`pulp create` reads this (if it exists) to determine which SDK version to use. `pulp build` in a standalone project also checks this.

`pulp upgrade` updates the CLI and optionally the SDK:
```bash
pulp upgrade              # upgrade CLI only
pulp upgrade --sdk        # upgrade CLI + download latest SDK
pulp upgrade --sdk 0.2.0  # upgrade CLI + specific SDK version
```

## Phased Implementation

### Phase S1: SDK Packaging
- Add CMake install rules for headers + libs
- Create `PulpConfig.cmake` template
- Build SDK tarball in release script
- Ship alongside CLI binaries in GitHub Releases

### Phase S2: SDK Fetching in CLI
- Add `PULP_SDK_VERSION` constant to CLI
- Implement SDK download + cache in `~/.pulp/sdk/`
- Detect repo vs standalone context in `pulp create`

### Phase S3: Standalone Project Templates
- Second set of CMakeLists.txt templates using `find_package(Pulp)`
- `pulp.toml` generation and reading
- `pulp build` works in standalone projects (sets `CMAKE_PREFIX_PATH`)

### Phase S4: Skia Asset Management
- Download Skia binaries to `~/.pulp/cache/` on first GPU build
- CMake module to locate cached Skia
- Only fetched if project uses GPU rendering

## Migration

- `pulp new` alias removed — `create` is the only command
- Existing projects inside the repo: no changes needed
- Help text, README, getting-started guide: updated to show `pulp create`
- `docs/reference/cli.md`: `new` section renamed to `create`

## Success Criteria

1. Fresh machine + `curl | sh` + `pulp create hello` = buildable plugin in < 3 minutes
2. No manual `git clone` of the framework repo required
3. `pulp doctor` still works without SDK (environment checking only)
4. Contributor flow (clone repo, use local source) unchanged
5. SDK versions can coexist — old projects keep working
