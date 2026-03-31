planning/pulp-create-standalone-spec.md

Implement the Pulp Create Standalone SDK Bootstrapping spec across all 4 phases. Work in a worktree (feature/standalone-create). Build, test, and verify each phase before moving to the next.

## Phase S1: SDK Packaging
- Add CMake install rules for all public headers and static libraries
- Create PulpConfig.cmake, PulpTargets.cmake, PulpPlugin.cmake templates in tools/cmake/
- PulpConfig.cmake must define imported targets: Pulp::format, Pulp::state, Pulp::audio, Pulp::signal, Pulp::midi, Pulp::canvas, Pulp::view, Pulp::render
- PulpPlugin.cmake provides pulp_add_plugin() macro for standalone projects
- Copy templates/ and external format SDK headers (CLAP, LV2, VST3 pluginterfaces) into SDK staging
- Create version.txt in SDK root
- Update tools/scripts/release-cli-local.sh to build SDK tarballs alongside CLI binaries
- Update .github/workflows/release-cli.yml to build and upload SDK tarballs
- Verify: cmake --install produces a complete SDK directory

## Phase S2: SDK Fetching in CLI
- Add PULP_SDK_VERSION constant to pulp_cli.cpp (pinned to current version)
- Add PULP_GITHUB_REPO constant ("danielraffel/pulp")
- Implement sdk_cache_path() -> ~/.pulp/sdk/<version>/
- Implement ensure_sdk() that checks cache, downloads if missing, extracts
- Download from GitHub Releases: pulp-sdk-<platform>.tar.gz
- Platform detection: darwin-arm64, darwin-x64, linux-x64, linux-arm64, windows-x64
- Modify find_project_root() to also detect standalone projects (has pulp.toml but no core/)
- In cmd_create: if outside a repo, call ensure_sdk() before scaffolding
- Error handling: clear messages if download fails, offline mode if SDK already cached
- Verify: pulp create works outside a repo after SDK is cached

## Phase S3: Standalone Project Templates
- Create tools/templates/standalone/ directory with CMakeLists.txt.template that uses find_package(Pulp)
- Standalone CMakeLists.txt must: cmake_minimum_required, project(), find_package(Pulp REQUIRED), use Pulp:: targets
- In cmd_create: detect standalone mode, use standalone templates, set CMAKE_PREFIX_PATH to SDK path
- Generate pulp.toml with sdk_version field
- In cmd_build: if pulp.toml exists and no core/ directory, set CMAKE_PREFIX_PATH automatically
- Template variables same as existing templates plus SDK_VERSION
- Standalone templates for all 4 types: effect, instrument, app, bare
- Verify: full round-trip — pulp create outside repo, pulp build, pulp test, pulp run

## Phase S4: Skia Asset Management
- Skip if GPU rendering is not needed for basic plugin creation (most plugins don't need it)
- Add pulp cache command to manage ~/.pulp/cache/
- If a standalone project enables GPU features, CMake module checks ~/.pulp/cache/ for Skia
- If missing, print clear message: "GPU rendering requires Skia. Run: pulp cache fetch skia"
- pulp cache fetch skia downloads platform-specific Skia binaries to ~/.pulp/cache/
- Verify: standalone project with GPU=OFF builds without Skia, GPU=ON gives clear guidance

## Migration (do throughout)
- Remove "new" command alias — create is the only command name
- Update all help text, README.md, docs/guides/getting-started.md, docs/reference/cli.md
- Update docs/status/cli-commands.yaml — remove new entry, update create entry
- Update planning/STATUS.md when complete

## Constraints
- Work in worktree feature/standalone-create
- Run tests after each phase: ctest --test-dir build --output-on-failure --exclude-regex AudioWorkgroup
- Build CLI and verify new commands work before moving to next phase
- Commit after each completed phase with clear commit messages
- When all 4 phases pass, merge to main and clean up worktree
