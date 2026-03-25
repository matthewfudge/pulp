# Agent Contribution Rules

Rules for AI agents and automated tools contributing to the Pulp repository.

## What Counts As a Public-Facing Change

A change is public-facing if it affects:

- Any header in `core/<subsystem>/include/pulp/<subsystem>/`
- The signature or behavior of `pulp::format::Processor` or `pulp::state::StateStore`
- CLI command names, arguments, or output format
- CMake function signatures (`pulp_add_plugin`, `pulp_add_app`, `pulp_add_binary_data`)
- Plugin format entry macros (`PULP_CLAP_PLUGIN`, `PULP_VST3_PLUGIN`, `PULP_AU_PLUGIN`)
- Build output paths or bundle structure
- Example plugin behavior

## When Docs Must Be Updated

Update docs when:

- A public header's API changes (add, remove, or modify a function/class)
- A CLI command is added, removed, or its behavior changes
- A CMake function's parameters change
- A new example is added
- A new subsystem is added

The relevant files are in `docs/reference/` and `docs/guides/`.

## When Status Manifests Must Be Updated

Update the YAML manifests in `docs/status/` when:

- A module's maturity level changes (e.g., experimental to usable)
- A platform or format gains or loses support
- A CLI command is added or its status changes
- A CMake function is added or its status changes

Manifests and their corresponding docs must stay consistent.

## When Tests Are Required

Tests are required when:

- Adding a new public function or class
- Changing DSP behavior (must have golden-file or unit test coverage)
- Adding a new plugin format adapter
- Adding a new example plugin
- Fixing a bug (add a test that would have caught it)

Tests are encouraged but not strictly required for:

- Internal refactoring that does not change public behavior
- Documentation-only changes
- Build system changes (though build smoke tests are preferred)

## What Agents Should Avoid Changing Casually

Do not modify without explicit instruction:

- **`pulp::format::Processor` interface** -- this is the central plugin API; changes break all plugins
- **`pulp::state::StateStore` serialization format** -- changes break saved plugin state
- **Plugin format entry macros** -- changes break all existing entry-point files
- **CMake function signatures** -- changes break all plugin CMakeLists.txt files
- **CLAUDE.md** -- this is the project's operating rules
- **DEPENDENCIES.md** -- dependency changes require license review first
- **External SDK integrations** -- VST3, AU, CLAP adapter code is carefully matched to SDK versions

## Audio Thread Safety

Never introduce code that violates real-time safety in `Processor::process()` or any function called from it:

- No heap allocation
- No locks
- No exceptions
- No I/O
- No unbounded-time operations

See `docs/policies/code-style.md` for the full list of rules and approved lock-free primitives.

## Clean-Room Discipline

- Never reference JUCE source code during implementation
- No JUCE class names, module names, API names, or naming patterns
- When studying external code for inspiration (iPlug2, AudioKit, format SDKs), note it
- See CLAUDE.md for the full clean-room policy

## Commit Standards

- Commits on main are clean and purposeful
- Commit messages use imperative mood and explain why, not just what
- No "WIP", "fix", "stuff", or "misc" commits on main
- Every commit should build and pass tests
