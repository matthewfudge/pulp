# Cmajor External Toolchain Guide

Use this guide when you want to author DSP in Cmajor without making the Cmajor
engine/runtime a shipped dependency of Pulp itself.

## Truthful Scope

Pulp supports a **narrow MIT-safe Cmajor lane** today:

- source-owned `.cmajor` / `.cmajorpatch` examples
- explicit helper workflow for an externally installed `cmaj` executable
- validation of patch/example structure even when `cmaj` is not installed
- generated-artifact export driven by the user's own Cmajor toolchain

This guide does **not** imply:

- a bundled Cmajor engine/JIT/runtime inside Pulp
- checked-in generated Cmajor-derived code inside this repo
- a shipped `Processor` adapter for arbitrary `.cmajorpatch` files
- universal hot reload
- license assumptions on behalf of the user

The supported model is: **you bring `cmaj`, Pulp helps you succeed, and Pulp
stays on the safe side of its MIT licensing policy.**

## Why The Lane Is External

The upstream Cmajor repository is dual GPLv3/commercial. That means Pulp must
not quietly embed or redistribute the official Cmajor engine/runtime as if it
were just another MIT-friendly dependency.

What Pulp can safely ship:

- docs, examples, and agent skills
- helpers that invoke a user-provided `cmaj`
- validation around missing-tool diagnostics and expected project layout
- opt-in workflows for generated-artifact import or bring-your-own runtime

What Pulp does **not** ship here:

- the upstream compiler binary
- the JIT engine/runtime
- checked-in generated Cmajor output from the upstream toolchain

## What Pulp Ships

Guide + helper substrate:

- `docs/guides/cmajor.md`
- `.agents/skills/cmajor-external/SKILL.md`
- `tools/scripts/cmajor_external.py`

Reference source-only example:

- `examples/cmajor-gain/CmajorGain.cmajor`
- `examples/cmajor-gain/CmajorGain.cmajorpatch`
- `examples/cmajor-gain/README.md`

## Supported Workflow

### 1. Keep the patch source in your project

The Pulp-owned part of the workflow starts with checked-in source:

- `.cmajor`
- `.cmajorpatch`

Pulp does **not** currently check generated Cmajor output into this repo.

### 2. Validate the patch structure first

You can validate that the patch manifest and source layout are sane even when
`cmaj` is not installed:

```bash
python3 tools/scripts/cmajor_external.py doctor \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch
```

If you want `doctor` to fail when `cmaj` is missing:

```bash
python3 tools/scripts/cmajor_external.py doctor \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --require-tool
```

### 3. Generate with your own `cmaj`

If `cmaj` is available on `PATH` or via `CMAJ_BIN`, Pulp can invoke it for you:

```bash
python3 tools/scripts/cmajor_external.py generate \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --target cpp \
  --output /tmp/CmajorGain.cpp
```

You can also pass through target-specific args, for example:

```bash
python3 tools/scripts/cmajor_external.py generate \
  --patch examples/cmajor-gain/CmajorGain.cmajorpatch \
  --target clap \
  --output /tmp/CmajorGainClap \
  --arg --clapIncludePath=/path/to/clap/include
```

Supported targets in the helper today:

- `cpp`
- `clap`
- `juce`
- `javascript`
- `webaudio`
- `webaudio-html`

Notes:

- `clap`, `juce`, `javascript`, `webaudio`, and `webaudio-html` are documented
  upstream targets.
- `cpp` is the most promising artifact-import lane for Pulp because the
  upstream Cmajor docs describe the generated C++ class as dependency-free.
- Pulp does not yet claim a finished, shipped `Processor` wrapper around that
  generated class.

### 4. Optional advanced lane: bring your own runtime

If you already have your own licensed or otherwise self-supplied Cmajor
runtime/JIT environment, Pulp can eventually support that as an **opt-in**
integration lane.

That lane is not the default shipped story and is not bundled by Pulp core.

## Reference Example

The source-only reference example lives at:

- `examples/cmajor-gain/`

It is intentionally minimal:

- one gain-like parameter path
- stereo input/output
- no generated output checked in
- no build-system claim that the repo can compile it without your own `cmaj`

## Validation Expectations

Today the truthful validation floor is:

- patch/source layout checks pass
- helper command construction is tested
- missing-tool errors are explicit
- generated-artifact invocation path is proven against a fake `cmaj` in unit
  tests

That means Pulp is validating **its side** of the external-toolchain contract
without claiming to redistribute or embed upstream Cmajor.

## Known Boundaries

- Cmajor support remains **experimental**
- this is an external-toolchain lane, not a bundled runtime lane
- no checked-in generated Cmajor examples are shipped in Pulp today
- no first-class C++ `Processor` adapter is shipped yet
- broader hot-reload or BYO-runtime work remains follow-up
