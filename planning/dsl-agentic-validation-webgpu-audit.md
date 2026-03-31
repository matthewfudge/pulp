# DSL, Agentic Validation, and WebGPU Compute Audit

Date: 2026-03-30

Purpose:
- Audit what is real today in Pulp versus what is still aspirational.
- Create phased plans for four capability tracks:
  1. DSP DSLs as first-class citizens
  2. Agent-driven validation workflows
  3. Agent-usable validation APIs
  4. WebGPU compute for audio
- Use the repo as the source of truth and align with a Claude second opinion.

## Executive Summary

Pulp already has a real GPU-backed rendering stack, real screenshot and screenshot-diff tooling, real UI inspection and synthetic input primitives, real validator and sanitizer workflows, and a solid C++ DSP base. Those are not hypothetical.

Pulp does not currently have first-class FAUST, Cmajor, or JSFX support. It also does not currently have real WebGPU compute-audio pipelines. Those show up in planning and vision material, but not in implementation.

The repo is strongest today at:
- native/web-style UI rendering
- visual regression and debug capture
- agent-friendly inspection and deterministic interaction seams
- plugin-format integration and host validation basics

The repo is weakest today at:
- DSL ingestion/codegen/runtime
- closed-loop agent orchestration around real plugins
- debugger control
- annotation workflows
- honest documentation boundaries around “agent workflows” and “WebGPU compute”

## Evidence

Real today:
- Dawn + Skia Graphite render path: [core/render/src/gpu_surface_dawn.cpp](../core/render/src/gpu_surface_dawn.cpp), [core/render/src/skia_surface.cpp](../core/render/src/skia_surface.cpp)
- Skia/SkSL-backed canvas effects: [core/canvas/src/skia_canvas.cpp](../core/canvas/src/skia_canvas.cpp)
- Screenshot capture: [core/view/include/pulp/view/screenshot.hpp](../core/view/include/pulp/view/screenshot.hpp)
- Screenshot diff/crop/bounds: [core/view/include/pulp/view/screenshot_compare.hpp](../core/view/include/pulp/view/screenshot_compare.hpp)
- View inspection: [core/view/include/pulp/view/inspector.hpp](../core/view/include/pulp/view/inspector.hpp)
- Synthetic interaction: [core/view/include/pulp/view/view.hpp](../core/view/include/pulp/view/view.hpp)
- Design debug bundle tooling: [tools/design/pulp_design_debug.cpp](../tools/design/pulp_design_debug.cpp)
- Validator CLI surface: [tools/cli/pulp_cli.cpp](../tools/cli/pulp_cli.cpp)
- Sanitizer workflow: [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml), [docs/guides/testing-advanced.md](../docs/guides/testing-advanced.md)
- Signal/DSP primitives: [core/signal/include/pulp/signal/fft.hpp](../core/signal/include/pulp/signal/fft.hpp)

Not real yet:
- FAUST/Cmajor/JSFX implementation in `core/`, `tools/`, CLI, or templates
- Public WGSL/WebGPU compute API for audio workloads
- Agent debugger control layer
- First-class screenshot annotation workflow
- Fully honest docs around current agent/MCP maturity and WebGPU compute scope

## Track 1: DSP DSLs as First-Class Citizens

### Current State

Current support is effectively none. FAUST, Cmajor, and JSFX only appear in planning and comparison notes, not in runtime or build integration. The actual DSP authoring model today is hand-written C++ `Processor` subclasses plus `StateStore` parameters.

Useful extension seams that already exist:
- `Processor` / `ProcessorFactory`: [core/format/include/pulp/format/processor.hpp](../core/format/include/pulp/format/processor.hpp)
- `StateStore`: [core/state/include/pulp/state/store.hpp](../core/state/include/pulp/state/store.hpp)
- Headless processing: [core/format/include/pulp/format/headless.hpp](../core/format/include/pulp/format/headless.hpp)
- Plugin CLI wrapper: [tools/plugin-cli/plugin_cli.hpp](../tools/plugin-cli/plugin_cli.hpp)
- Build integration seam: [tools/cmake/PulpUtils.cmake](../tools/cmake/PulpUtils.cmake)

### Best Next Step

Pick one DSL and make it real. FAUST is the best first target:
- mature compiler
- C++ codegen
- low runtime coupling
- clear fit with the existing `Processor` model

Do not start with three DSLs at once.

### Phase 1 Deliverables

- Write a `pulp-dsl` contract:
  - parameter reflection into `StateStore`
  - MIDI/bus mapping
  - state/preset serialization
  - error reporting
  - codegen ownership boundaries
- Ship FAUST offline codegen only:
  - `pulp dsp compile foo.dsp`
  - generated C++ wrapper that subclasses `Processor`
  - FAUST metadata mapped into `ParamInfo`
- Add 2-3 real examples:
  - gain
  - filter
  - simple synth voice
- Add headless validation:
  - compile
  - build
  - instantiate
  - process fixed buffers
  - verify output

### Phase 2 Deliverables

- FAUST dev hot reload:
  - file watcher
  - generated-wrapper rebuild loop
  - state preservation across reloads where possible
- CLI scaffolding:
  - `pulp create --dsl faust`
- Tests for rebuild/reload stability and parameter continuity

### Phase 3 / Exploration Deliverables

- Cmajor adapter track:
  - runtime/build integration decision
  - one example instrument/effect
  - headless validation
- JSFX exploration track:
  - choose scope first: subset import/transpile vs embedded runtime
  - do not promise “first-class” until a real compatibility plan exists

### Top Risks

- trying to support multiple DSLs before FAUST proves the model
- parameter/state mismatch between generated code and `StateStore`
- hot reload complexity before offline codegen is stable
- JSFX scope explosion

## Track 2: Agent-Driven Validation Workflows

### Current State

This is partially real, but only partially. Pulp already has strong building blocks for screenshot capture, diffing, inspection, and validator/sanitizer execution. It does not yet have a complete closed-loop “agent controls plugin, debugger, annotations, and validators” system.

Real building blocks:
- screenshot capture/diff/crop APIs
- `pulp design-debug` before/after/diff/report bundles
- validator orchestration in CLI/CI
- sanitizer workflows in CI/docs
- synthetic click/drag and inspector APIs

Missing:
- debugger control
- annotation workflows
- end-to-end plugin-host control surface for agents
- honest GPU-faithful capture in every validation path

### Best Next Step

Make the current building blocks into one deterministic validation harness before adding “autonomous agents” language.

### Phase 1 Deliverables

- One stable validation bundle format:
  - screenshots
  - diffs
  - inspector JSON
  - validator outputs
  - sanitizer configuration metadata
- Make `pulp validate` more complete and explicit:
  - include `pluginval` when installed
  - clearly state current CLAP/AU/VST coverage
- Replace placeholder MCP wrappers with real wrappers over:
  - click/drag
  - view tree
  - screenshot capture

### Phase 2 Deliverables

- Stable headless plugin-control surface:
  - load preset
  - set/get parameter
  - send MIDI
  - process audio file/buffer
  - capture screenshot
  - export machine-readable report
- Agent-oriented orchestration layer on top of that surface:
  - short validation loops
  - expected-vs-actual result reporting

### Phase 3 / Exploration Deliverables

- Annotation import/export:
  - screenshot comments
  - regions of interest
  - user-marked issues
- Debugger loop exploration:
  - LLDB/GDB integration only after the validation surface is stable
- Autonomous issue triage/fix loops as an experiment, not as a foundational promise

### Top Risks

- overclaiming “agent workflow” before the harness is deterministic
- mixing design-tool visual QA with general plugin runtime QA without a stable shared schema
- debugger integration before core validation APIs are stable

## Track 3: APIs That Agents Can Reason About and Use for Validation

### Current State

This is the strongest of the four tracks. Pulp already has real APIs that agents can use:
- screenshot capture
- screenshot comparison
- crop and diff bounds
- inspector JSON
- synthetic click/drag
- JS bridge helpers
- validation and design-debug CLI entry points

What is missing is not raw capability so much as productization and truthfulness:
- the agent-facing protocol is not fully stabilized
- some MCP tools are still placeholders
- docs sometimes imply more maturity than the implementation supports

### Best Next Step

Freeze and document a small, durable agent-validation contract.

### Phase 1 Deliverables

- Define a stable machine-readable contract for:
  - view tree JSON
  - screenshot capture metadata
  - diff metrics
  - parameter set/get
  - validation report schema
- Mark experimental vs usable APIs honestly in docs/status
- Expose the existing primitives through real CLI/MCP endpoints, not placeholders

### Phase 2 Deliverables

- Add richer query semantics:
  - find control by id/type/label
  - get semantic control state
  - capture region by view id
  - compare only targeted regions
- Standardize JSON artifacts so agents can chain tools without custom parsing per command

### Phase 3 / Exploration Deliverables

- Higher-level semantic APIs:
  - “is this control visually disabled?”
  - “did the focused state appear?”
  - “does the waveform render change after processing?”
- Domain-specific plugin validation recipes built on the stable low-level contract

### Top Risks

- inventing a too-large protocol before stabilizing the small useful core
- hiding experimental or backend-specific caveats from agents
- allowing docs to outrun implementation

## Track 4: WebGPU Compute for Audio

### Current State

Pulp has a real GPU UI/rendering base through Dawn + Skia Graphite. It does not have real WebGPU compute-audio implementation today.

Current reality:
- render stack is real
- SkSL runtime effects are real
- signal/DSP primitives are real on CPU
- audio-to-UI publication primitives are real

Not real:
- compute pipelines for audio DSP
- public WGSL compute API
- GPU buffer/resource model for audio workloads
- scheduling and safety model for real-time audio callback compute

### Best Next Step

Do not pitch this as “available soon.” Treat it as exploration and prove usefulness with an isolated, non-real-time compute spike.

### Phase 1 Deliverables

- Documentation truth pass:
  - clearly separate SkSL/Skia effects from WebGPU compute
- One compute feasibility spike, off the audio callback path:
  - offline FFT
  - spectrogram generation
  - or batch convolution
- Benchmark:
  - CPU vs GPU crossover points
  - upload/readback overhead
  - platform stability

### Phase 2 Deliverables

- UI-side GPU processing first:
  - analyzer/spectrogram generation
  - audio visualization helpers using published audio data
- Internal compute resource model in `render` or a dedicated experimental module

### Phase 3 / Exploration Deliverables

- Offline or high-latency workloads only:
  - additive synthesis batch rendering
  - spectral resynthesis
  - ML inference experiments
- Only pursue real plugin/runtime integration if Phase 1 benchmarks justify it

### Top Risks

- chasing GPU DSP prestige without a clear win over CPU SIMD
- trying to put GPU work directly in the audio callback too early
- conflating UI shader support with compute support

## Overclaims To Fix

These areas should be tightened so the repo speaks accurately:

- Agent workflows:
  - say “agent-friendly validation building blocks exist”
  - do not say full agent runtime/closed-loop workflows are shipped
- MCP:
  - avoid implying complete view-control/debug orchestration until placeholder tools are replaced
- WebGPU:
  - say “GPU-backed rendering and SkSL effects exist”
  - do not imply general WebGPU compute-audio support yet
- DSLs:
  - say “planned”
  - do not imply first-class support until at least FAUST Phase 1 lands

## Recommended Execution Order

1. Track 3 Phase 1: stabilize the agent-usable API/report contract
2. Track 2 Phase 1: turn the existing validation pieces into one deterministic harness
3. Track 4 Phase 1: run a small compute feasibility spike and fix docs truth
4. Track 1 Phase 1: land FAUST offline codegen on top of the now-better validation surface
5. Track 2 Phase 2 and Track 1 Phase 2 next
6. Treat Cmajor, JSFX, autonomous agent loops, and production compute-audio as later exploration unless earlier phases prove real leverage

## Recommended Default Position

Near-term:
- Pulp should present itself as a framework with strong agent-friendly validation seams, real GPU rendering, and a credible path to DSLs and compute.

It should not yet present itself as:
- a first-class DSP DSL host
- a full agent-operated plugin runtime
- a WebGPU compute-audio framework

That becomes true only after the Phase 1 work above lands.
