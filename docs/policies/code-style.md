# Code Style

This document defines the coding standards and architectural rules for the Pulp repository.

## Public API Boundaries

- Consumers include only from `core/<subsystem>/include/pulp/<subsystem>/`.
- Consumer-facing documentation and examples must not include from `src/`.
- Internal helpers must not leak into public headers unless deliberately designed as public API.
- Public headers use `#pragma once` and are self-contained (include their own dependencies).

## Module Boundaries

- Each subsystem lives in `core/<subsystem>/` with its own `CMakeLists.txt`.
- Dependencies flow downward: higher-level subsystems may depend on lower-level ones, never the reverse.
- `format` depends on `state`, `audio`, `midi` -- not on `view` or `render`.
- Platform-specific code belongs in `core/platform/` or behind `#ifdef` guards in platform subdirectories.
- Avoid cross-subsystem includes between peer modules that are not declared dependencies.

## Real-Time Audio Safety

Code that runs on the audio thread (`Processor::process()`) must follow these rules:

- **No heap allocation** -- no `new`, no `malloc`, no `std::vector::push_back`, no `std::string` construction
- **No locks** -- no `std::mutex`, no `std::condition_variable`, no OS-level locks
- **No exceptions** -- no `throw`, no `try/catch`
- **No I/O** -- no file reads, no network calls, no logging in the hot path
- **No non-deterministic work** -- avoid anything with unbounded execution time

Use lock-free primitives from `pulp::runtime` for cross-thread communication:

| Pattern | Primitive |
|---------|-----------|
| Single value, latest-wins | `std::atomic<T>` (relaxed) |
| Multi-field coherent read | `SeqLock<T>` |
| Large data swap | `TripleBuffer<T>` |
| Ordered event stream | `SPSCQueue<T>` |

## Naming

- Namespaces: `pulp::<subsystem>` (e.g., `pulp::format`, `pulp::state`)
- Classes: `PascalCase` (e.g., `Processor`, `StateStore`, `ParamValue`)
- Functions and methods: `snake_case` (e.g., `get_value()`, `define_parameters()`)
- Constants and enum values: `kPascalCase` (e.g., `kGain`, `kBypass`)
- Files: `snake_case.hpp`, `snake_case.cpp`
- No names that match the dominant audio framework's naming conventions

## Header and Source Organization

- Public headers: `core/<subsystem>/include/pulp/<subsystem>/<name>.hpp`
- Implementation: `core/<subsystem>/src/<name>.cpp`
- Platform-specific: `core/<subsystem>/platform/<os>/<name>.cpp` or `<name>.mm`
- Convenience umbrella headers (e.g., `runtime.hpp`, `signal.hpp`) include all public headers for a subsystem

## Build Hygiene

- Configure and build must not modify tracked source files.
- Generated files belong in build outputs, not in the source tree (unless intentionally checked in, like `moduleinfo.json`).
- Every `CMakeLists.txt` change should be followed by a clean reconfigure to verify correctness.

## Comments

- Comments explain constraints, non-obvious decisions, or safety rationale.
- Avoid redundant comments that restate what the code already says.
- Document memory ordering choices on atomics and lock-free structures.
- Public API comments should describe the contract, not the implementation.

## Tests

- New public behavior needs tests.
- Bug fixes should add or adjust a test when practical.
- Example plugins each include a test file.
- Tests use Catch2.
- Test files live in `test/` or alongside examples (`test_pulp_*.cpp`).

## Documentation Updates

- Public behavior changes require updates to the relevant docs in `docs/`.
- Support-level changes require updates to the YAML manifests in `docs/status/`.
- New CLI commands require an entry in `docs/reference/cli.md` and `docs/status/cli-commands.yaml`.
- New CMake functions require an entry in `docs/reference/cmake.md` and `docs/status/cmake-functions.yaml`.
