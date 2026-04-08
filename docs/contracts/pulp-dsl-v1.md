# pulp-dsl Contract v1

The DSL contract defines how code-generating DSL backends (FAUST, Cmajor, JSFX) integrate with Pulp's Processor model.

## Status

- **FAUST**: experimental (offline codegen only, Phase 3)
- **Cmajor**: experimental external-toolchain support (guide/example/helper shipped; core Processor adapter still planned)
- **JSFX**: experimental bounded-subset authoring support (guide/example/helper shipped; core Processor adapter still planned)

## Contract

### Parameter Reflection

DSL backends populate `DslParamDescriptor` from their native metadata:
- FAUST: `buildUserInterface()` callbacks → `DslParamDescriptor`
- Cmajor: endpoint declarations (future Processor adapter work)
- JSFX: `slider` declarations (source validation today; Processor mapping future)

Parameters are registered into `StateStore` with sequential IDs starting at 1. The mapping is deterministic: same DSL source produces the same parameter layout.

### MIDI / Bus Mapping

`DslBusLayout` describes the channel configuration extracted from DSL source:
- `num_inputs` / `num_outputs` from FAUST `getNumInputs()` / `getNumOutputs()`
- `accepts_midi` is false for basic FAUST (no MIDI mapping in this phase)
- A zero-input plugin is categorized as `Instrument`

### State / Preset Serialization

DSL processors inherit `StateStore::serialize()` / `deserialize()` from the Processor model. No DSL-specific serialization is needed — all state lives in StateStore parameters.

### Generated-Wrapper Ownership

- `FaustProcessor<T>` owns the FAUST DSP instance (`std::unique_ptr<T>`)
- The DSP instance lifetime matches the Processor lifetime
- `init()` is called in `prepare()`, `compute()` in `process()`
- Zone pointers are synced from StateStore before each `compute()` call

### Compile / Build Error Reporting

`DslError` provides structured error reporting:
- `Severity`: Warning or Error
- `message`, `file`, `line` for source location
- CMake integration reports FAUST compiler errors through standard CMake mechanisms

## Files

| File | Purpose |
|------|---------|
| `core/dsl/include/pulp/dsl/dsl_processor.hpp` | Base contract: DslProcessor, DslParamDescriptor, DslBusLayout, DslError |
| `core/dsl/include/pulp/dsl/faust_processor.hpp` | FAUST-specific wrapper: FaustProcessor<T>, PulpFaustUI, PulpFaustMeta |
| `core/dsl/include/pulp/dsl/faust_base.hpp` | Minimal FAUST base classes (dsp, UI, Meta) |
| `tools/cmake/PulpFaust.cmake` | CMake functions for FAUST codegen |
| `tools/scripts/cmajor_external.py` | External-helper flow for validating patch layout and invoking a user-supplied `cmaj` |
| `tools/scripts/jsfx_subset.py` | Bounded JSFX subset validator for source-only examples and authoring workflows |

## Scope Boundaries

This contract covers offline code generation only. It does NOT cover:
- Hot reload / live recompilation
- JIT compilation
- Multi-DSL parity (each backend implements what it can)
- DSL-specific UI generation
- MIDI mapping from FAUST metadata

For Cmajor specifically, the shipped support today is the external-toolchain
lane around `.cmajor` / `.cmajorpatch` sources. A real Pulp `Processor` adapter
for generated Cmajor artifacts remains follow-up work.

For JSFX specifically, the shipped support today is a bounded source-validation
lane around `.jsfx` files that stay inside `@init`, `@slider`, `@block`,
`@sample`, and `slider1..slider64`. A real Pulp runtime/adapter for executing
that subset remains follow-up work.
