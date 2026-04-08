# JSFX Bounded Subset Guide

Use this guide when you want to author JSFX-style DSP in the narrow scope Pulp
supports today.

## Truthful Scope

Pulp supports a **bounded audio-focused JSFX authoring lane** today:

- real `.jsfx` source files
- three source-only reference examples
- validation against the current supported subset
- explicit failure for unsupported sections like `@gfx`

The current supported subset is:

- `@init`
- `@slider`
- `@block`
- `@sample`
- `slider1..slider64`

This guide does **not** imply:

- full REAPER JSFX compatibility
- Cockos/Reaper runtime embedding
- `@gfx`
- file I/O
- MIDI/sysex parity
- a finished Pulp `Processor` adapter for JSFX code

The supported model is: **author within the bounded subset, validate clearly,
and keep exclusions explicit.**

## What Pulp Ships

Guide + helper substrate:

- `docs/guides/jsfx.md`
- `.agents/skills/jsfx-subset/SKILL.md`
- `tools/scripts/jsfx_subset.py`

Reference source-only examples:

- `examples/jsfx-gain/`
- `examples/jsfx-tremolo/`
- `examples/jsfx-delay/`

## Supported Workflow

### 1. Start from a bounded `.jsfx` file

The current lane is source-first. Pulp does not ship or bundle a REAPER
runtime.

### 2. Validate the file against the supported subset

```bash
python3 tools/scripts/jsfx_subset.py doctor \
  --file examples/jsfx-gain/PulpJsfxGain.jsfx
```

For machine-readable output:

```bash
python3 tools/scripts/jsfx_subset.py doctor \
  --file examples/jsfx-gain/PulpJsfxGain.jsfx \
  --json
```

If the file uses unsupported sections such as `@gfx`, the helper exits non-zero
with a clear diagnostic.

### 3. Keep the scope explicit

Supported today:

- metadata via `desc:`
- slider declarations (`slider1..slider64`)
- section presence/validation for `@init`, `@slider`, `@block`, `@sample`

Not supported today:

- `@gfx`
- file loading/saving
- claims of behavioral parity with REAPER's JSFX runtime
- a built-in execution engine in Pulp core

## Reference Examples

The shipped source-only examples intentionally stay inside the bounded subset:

- `examples/jsfx-gain/PulpJsfxGain.jsfx`
- `examples/jsfx-tremolo/PulpJsfxTremolo.jsfx`
- `examples/jsfx-delay/PulpJsfxDelay.jsfx`

They are meant to show:

- a simple gain processor
- a modulation effect
- a memory-based delay idiom

without implying that Pulp already ships a full JSFX runtime.

## Validation Expectations

Today the truthful validation floor is:

- bounded-subset parsing succeeds for shipped examples
- unsupported sections fail with clear diagnostics
- slider and section metadata are extractable for future adapter work

That means Pulp is validating the **authoring contract** for its JSFX subset,
while leaving runtime/execution work as follow-up.

## Known Boundaries

- JSFX support remains **experimental**
- this is a bounded authoring/validation lane, not full runtime support
- no claim of full REAPER compatibility
- no `@gfx`
- no finished Pulp `Processor` adapter yet
