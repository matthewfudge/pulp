---
name: cmajor-external
description: Use Pulp's MIT-safe Cmajor support lane via source-owned patches, an external `cmaj` toolchain, and explicit generated-artifact workflows.
requires:
  scripts:
    - tools/scripts/cmajor_external.py
  tools: []
---

# Cmajor External Toolchain

Use this skill when a task involves Pulp's current Cmajor support lane.

## Truthful Position

Pulp supports Cmajor as an **external toolchain**, not as a vendored runtime.

Supported now:

- `.cmajor` / `.cmajorpatch` source examples
- patch-structure validation through `tools/scripts/cmajor_external.py doctor`
- generated-artifact invocation through a user-provided `cmaj`
- docs and workflow guidance for the MIT-safe lane

Not supported by this skill:

- shipping the Cmajor engine/JIT/runtime in Pulp core
- claiming a finished `Processor` adapter for arbitrary Cmajor patches
- blanket hot-reload claims
- broad multi-DSL parity claims

## Core Files

- `docs/guides/cmajor.md`
- `tools/scripts/cmajor_external.py`
- `docs/contracts/pulp-dsl-v1.md`
- `examples/cmajor-gain/`

## Recommended Workflow

### 1. Validate the example or patch layout first

```bash
python3 tools/scripts/cmajor_external.py doctor \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch
```

Use `--require-tool` when the next step truly depends on a real `cmaj`.

### 2. Keep `cmaj` external

Preferred resolution order:

- `--cmaj /path/to/cmaj`
- `CMAJ_BIN=/path/to/cmaj`
- `PATH`

Do not turn the upstream Cmajor runtime/toolchain into a bundled dependency of
Pulp unless the repo's license policy changes.

### 3. Generate artifacts explicitly

```bash
python3 tools/scripts/cmajor_external.py generate \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --target cpp \
  --output /tmp/CmajorGain.cpp
```

Pass through target-specific args with repeated `--arg` flags.

### 4. Keep docs truthful

When editing this lane, keep these in sync:

- `docs/guides/cmajor.md`
- `docs/reference/capabilities.md`
- `docs/guides/examples.md`
- `planning/v3-phase14-gap-closure-status.md`

## What To Check

For the current Cmajor lane, verify:

- patch manifest/source layout is valid
- missing-tool diagnostics are explicit
- helper commands target the user-supplied `cmaj`, not an assumed bundled tool
- docs do not imply a shipped runtime or checked-in generated output

## Boundaries

This skill is for the **MIT-safe external lane** only.

If a user wants true runtime embedding:

- treat that as a separate BYO-runtime path
- keep it opt-in
- do not convert it into a default shipped dependency
