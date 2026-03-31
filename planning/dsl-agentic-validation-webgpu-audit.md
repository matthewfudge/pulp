# DSL, Agentic Validation, and WebGPU Compute Audit

**Revision:** v2  
**Revision date:** 2026-03-31  
**Status:** Planning, repo-backed truth pass  

Purpose:
- audit what is real today in Pulp versus what is still planning/spec language
- update the execution order for four related tracks:
  1. DSP DSLs as first-class citizens
  2. agent-driven validation workflows
  3. agent-usable validation APIs
  4. WebGPU/audio exploration
- record the current status of related cross-cutting topics that materially affect those tracks:
  - external GPU-audio reference candidates
  - multi-window support
  - optional WebView support
  - audio visualization primitives
  - JS engine abstraction reality
  - validator and sanitizer reality
  - offline video rendering as future exploration only

This v2 intentionally distinguishes:
- **real code today**
- **stale docs/spec claims**
- **future exploration**

---

## Executive Summary

Pulp already has real GPU-backed UI rendering, real screenshot capture and screenshot-diff tooling, real view inspection and synthetic interaction seams, real CPU DSP primitives, real sanitizer CI, and real validator workflows in CI.

Pulp does **not** currently have:
- first-class FAUST, Cmajor, or JSFX support
- a shipped WebGPU compute-for-audio pipeline
- a stable closed-loop agent validation harness across plugins, screenshots, validators, and reports
- a shipped multi-window application framework
- a verified first-class STFT abstraction for analyzer/spectrogram work
- alternate JS engines wired in beyond QuickJS
- RTSan or `vstvalidator` integration

The highest-value next work is still:
1. stabilize an honest agent-validation contract and report schema
2. turn existing screenshot/inspector/validator pieces into one deterministic harness
3. keep DSL work narrow by doing FAUST offline codegen first
4. treat WebGPU/audio as an exploration track, not a shipped capability

---

## Repo-Backed Reality Snapshot

### Real today

- Dawn + Skia Graphite render path:
  [core/render/src/gpu_surface_dawn.cpp](../core/render/src/gpu_surface_dawn.cpp),
  [core/render/src/skia_surface.cpp](../core/render/src/skia_surface.cpp)
- Skia/SkSL-backed canvas effects:
  [core/canvas/src/skia_canvas.cpp](../core/canvas/src/skia_canvas.cpp)
- Screenshot capture:
  [core/view/include/pulp/view/screenshot.hpp](../core/view/include/pulp/view/screenshot.hpp)
- Screenshot diff/crop/bounds:
  [core/view/include/pulp/view/screenshot_compare.hpp](../core/view/include/pulp/view/screenshot_compare.hpp)
- View inspection:
  [core/view/include/pulp/view/inspector.hpp](../core/view/include/pulp/view/inspector.hpp)
- Synthetic interaction seams:
  [core/view/include/pulp/view/view.hpp](../core/view/include/pulp/view/view.hpp)
- Design-debug bundle tooling:
  [tools/design/pulp_design_debug.cpp](../tools/design/pulp_design_debug.cpp)
- Script engine wrapper, currently QuickJS only:
  [core/view/include/pulp/view/script_engine.hpp](../core/view/include/pulp/view/script_engine.hpp),
  [core/view/src/script_engine.cpp](../core/view/src/script_engine.cpp)
- Audio visualization building blocks:
  [core/view/include/pulp/view/audio_bridge.hpp](../core/view/include/pulp/view/audio_bridge.hpp),
  [core/view/include/pulp/view/widgets.hpp](../core/view/include/pulp/view/widgets.hpp),
  [core/view/src/widgets.cpp](../core/view/src/widgets.cpp),
  [core/signal/include/pulp/signal/fft.hpp](../core/signal/include/pulp/signal/fft.hpp)
- Optional WebView wrapper:
  [core/view/include/pulp/view/web_view.hpp](../core/view/include/pulp/view/web_view.hpp),
  [core/view/src/web_view.cpp](../core/view/src/web_view.cpp),
  [core/view/CMakeLists.txt](../core/view/CMakeLists.txt)
- Sanitizer CI:
  [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml),
  [tools/cmake/Sanitizers.cmake](../tools/cmake/Sanitizers.cmake),
  [docs/guides/testing-advanced.md](../docs/guides/testing-advanced.md)
- Validator CI:
  [.github/workflows/validate.yml](../.github/workflows/validate.yml)

### Not real yet

- FAUST/Cmajor/JSFX implementation in `core/`, `tools/`, templates, or CLI
- public WGSL/WebGPU compute API for audio workloads
- stable agent-validation protocol/schema shared across CLI and MCP
- shipped debugger control layer
- shipped annotation workflow over screenshots
- verified first-class STFT abstraction with configurable window/overlap
- alternate JS engines wired in beyond QuickJS
- dedicated multi-window application support beyond manually creating separate hosts
- offline video rendering support
- RTSan integration
- `vstvalidator` integration

---

## Cross-Cutting Reality Checks

### 1. `gpu.cpp` / AnswerDotAI `gpu.cpp` as a reference candidate

Repo truth:
- there is no repo-local implementation or planning dependency on `gpu.cpp` or AnswerDotAI `gpu.cpp`
- there are no references to those names in the current tree

Audit position:
- it is reasonable to treat `gpu.cpp` as an **external research/reference candidate** for GPU-audio exploration
- it should **not** be treated as source-of-truth architecture, a dependency, or a promised integration path
- any use of it should be gated by a later research pass covering:
  - license compatibility
  - realtime safety model
  - buffer ownership and upload/readback cost
  - whether its abstractions fit Pulp's current Dawn/WebGPU stack

Practical rule:
- okay as a future comparison target for Phase 4 exploration
- not okay to cite as if Pulp has already validated or adopted it

### 2. Multi-window support current status

Real code:
- `WindowHost` is a single host abstraction with `create(View&, WindowOptions)`:
  [core/view/include/pulp/view/window_host.hpp](../core/view/include/pulp/view/window_host.hpp)
- standalone/app examples create a single window host:
  [core/format/src/standalone.cpp](../core/format/src/standalone.cpp),
  [examples/ui-preview/main.cpp](../examples/ui-preview/main.cpp),
  [examples/design-tool/main.cpp](../examples/design-tool/main.cpp)
- platform hosts are per-window implementations, not a higher-level multi-window framework:
  [core/view/platform/mac/window_host_mac.mm](../core/view/platform/mac/window_host_mac.mm),
  [core/view/platform/ios/window_host_ios.mm](../core/view/platform/ios/window_host_ios.mm)

Spec/planning drift:
- [planning/08-architecture-spec.md](./08-architecture-spec.md) lists multi-window support as a responsibility
- [planning/design-tool-phase-plan.md](./design-tool-phase-plan.md) says the API already supports multi-window and “we just need to use it”

Audit position:
- current status is **partial/manual at best**
- multiple windows may be possible by manually constructing multiple hosts in an app, but there is no productized multi-window system, shared lifecycle, or explicit design-tool support
- do not describe multi-window support as shipped framework capability today

### 3. Optional WebView support current status

Real code:
- WebView exists behind `PULP_BUILD_WEBVIEW`:
  [core/view/CMakeLists.txt](../core/view/CMakeLists.txt)
- implementation wraps `choc::ui::WebView`:
  [core/view/src/web_view.cpp](../core/view/src/web_view.cpp)
- tests exist but are weak/headless-tolerant:
  [test/test_web_view.cpp](../test/test_web_view.cpp)

Audit position:
- optional WebView support is **real but off by default**
- it is an embedded content panel option, not the primary UI architecture
- docs and prompts should keep saying the main path is native view/canvas/render, not WebView

### 4. Audio visualization system status

Real today:
- meter ballistics are real:
  [core/view/include/pulp/view/audio_bridge.hpp](../core/view/include/pulp/view/audio_bridge.hpp),
  [test/test_audio_bridge.cpp](../test/test_audio_bridge.cpp)
- latest-value publication from audio thread to UI is real via `TripleBuffer`:
  [core/view/include/pulp/view/audio_bridge.hpp](../core/view/include/pulp/view/audio_bridge.hpp),
  [docs/guides/modules/runtime.md](../docs/guides/modules/runtime.md)
- visualization widgets are real for:
  - `Meter`
  - `WaveformView`
  - `SpectrumView`
  [core/view/include/pulp/view/widgets.hpp](../core/view/include/pulp/view/widgets.hpp),
  [core/view/src/widgets.cpp](../core/view/src/widgets.cpp)
- FFT primitives are real:
  [core/signal/include/pulp/signal/fft.hpp](../core/signal/include/pulp/signal/fft.hpp)

Important honesty point:
- [planning/STATUS.md](./STATUS.md) still contains an older “CHOC FIFO + MeterBallistics” line for the audio bridge, but the current code uses `TripleBuffer`

Not verified in current code:
- a first-class STFT abstraction with configurable window/overlap
- a production spectrogram pipeline
- render-thread analyzer/STFT infrastructure as described in some planning docs

Audit position:
- say “metering ballistics, CPU FFT primitives, waveform/spectrum widgets, and lock-free latest-value publication are real”
- do **not** say “STFT abstractions” or “spectrogram system” are implemented unless code lands

### 5. JS engine abstraction status

Real code:
- `ScriptEngine` wraps `choc::javascript::Context` and explicitly says “currently QuickJS”:
  [core/view/include/pulp/view/script_engine.hpp](../core/view/include/pulp/view/script_engine.hpp)
- implementation includes QuickJS directly and constructs `createQuickJSContext()`:
  [core/view/src/script_engine.cpp](../core/view/src/script_engine.cpp)

Spec/planning drift:
- several planning docs talk about QuickJS/V8/JSC adaptability as if it is already a supported choice

Audit position:
- current shipped reality is **QuickJS only**
- there is an abstraction seam worth preserving
- JSC/V8 adaptability is still **planned**, not implemented
- prompts should not assume a backend selector or alternate-engine validation matrix already exists

### 6. Sanitizer and validator reality

Real today:
- ASan CI:
  [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml)
- TSan CI:
  [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml)
- UBSan CI:
  [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml)
- `PULP_SANITIZER` CMake option supports `address`, `thread`, `undefined`, `memory`:
  [tools/cmake/Sanitizers.cmake](../tools/cmake/Sanitizers.cmake)
- CI installs and runs `pluginval`, attempts `clap-validator`, and runs an `auval` scan step:
  [.github/workflows/validate.yml](../.github/workflows/validate.yml)

Important limitations:
- RTSan is **not** present in current CMake or CI
- `vstvalidator` is **not** present in current repo workflows or CLI
- `pulp validate` currently runs:
  - CLAP validation
  - AU validation
  and does **not** currently invoke `pluginval` for VST3:
  [tools/cli/pulp_cli.cpp](../tools/cli/pulp_cli.cpp)
- CLI docs currently understate/overstate parts of this depending on the page:
  [docs/reference/cli.md](../docs/reference/cli.md),
  [docs/concepts/overview.md](../docs/concepts/overview.md),
  [docs/reference/capabilities.md](../docs/reference/capabilities.md)
- the `auval` CI step is still lighter-weight than “full AU validation”; it scans installed components and notes signing caveats

Audit position:
- honest current list: **ASan, TSan, UBSan, pluginval in CI, clap-validator attempted in CI, auval scan/check path in CI, AU/CLAP validation in CLI**
- do not claim RTSan
- do not claim `vstvalidator`
- do not claim `pulp validate` already covers VST3 with `pluginval` until code changes

#### Validator Coverage Matrix

| Tool | In CI? | In `pulp validate`? | Current coverage | Notes |
|------|--------|---------------------|------------------|-------|
| `pluginval` | Yes | No | VST3 in CI only | Present in [`validate.yml`](../.github/workflows/validate.yml); not currently invoked by [`pulp validate`](../tools/cli/pulp_cli.cpp) |
| `clap-validator` | Attempted | Yes | CLAP | CI attempts it when available; CLI runs CLAP validation paths |
| `auval` | Yes | Yes | AU scan/check path | CI installs components and runs an `auval` path with signing caveats; do not treat it as “full AU validation parity” |
| `vstvalidator` | No | No | None | Not integrated or evaluated in current repo workflows |
| RTSan | No | No | None | No current toolchain/CMake/CI integration |

### 7. Offline video rendering

Real code:
- none verified in current repo

Audit position:
- keep it as a **future exploration phase only**
- its dependency chain is later than:
  - validation bundle stabilization
  - honest GPU capture/reporting
  - any real WebGPU/audio feasibility result

---

## Track 1: DSP DSLs as First-Class Citizens

### Current State

Current support is effectively none. FAUST, Cmajor, and JSFX appear in planning/comparison material, not in runtime or build integration. The actual DSP authoring model today is handwritten C++ `Processor` subclasses plus `StateStore` parameters.

Useful seams that already exist:
- `Processor` / `ProcessorFactory`:
  [core/format/include/pulp/format/processor.hpp](../core/format/include/pulp/format/processor.hpp)
- `StateStore`:
  [core/state/include/pulp/state/store.hpp](../core/state/include/pulp/state/store.hpp)
- Headless processing:
  [core/format/include/pulp/format/headless.hpp](../core/format/include/pulp/format/headless.hpp)
- CLI/plugin scaffolding seams:
  [tools/plugin-cli/plugin_cli.hpp](../tools/plugin-cli/plugin_cli.hpp),
  [tools/cmake/PulpUtils.cmake](../tools/cmake/PulpUtils.cmake)

### Honest Position

- FAUST/Cmajor/JSFX are **planned**
- there is no shipped DSL ingestion/runtime today
- FAUST remains the best first implementation target because it matches the existing offline codegen + C++ wrapper model

### Best Next Step

Pick one DSL and make it real. Start with **FAUST offline codegen only**.

### Phase 1 Deliverables

- define a `pulp-dsl` contract:
  - parameter reflection into `StateStore`
  - MIDI/bus mapping
  - state/preset serialization
  - generated-wrapper ownership boundaries
  - compile/build error reporting
- ship FAUST offline compile only:
  - `pulp dsp compile foo.dsp`
  - generated C++ wrapper subclassing `Processor`
  - FAUST metadata mapped into `ParamInfo`
- add 2-3 real examples:
  - gain
  - filter
  - simple synth voice
- add headless validation:
  - compile
  - build
  - instantiate
  - process fixed buffers
  - verify output

### Later Work

- Phase 2:
  - FAUST hot reload
  - `pulp create --dsl faust`
  - rebuild/reload continuity tests
- Phase 3 exploration:
  - Cmajor adapter spike
  - JSFX scope decision before any compatibility promise

### Top Risks

- trying to support multiple DSLs before FAUST proves the model
- parameter/state mismatch between generated code and `StateStore`
- hot reload complexity before offline codegen is stable
- JSFX scope explosion

---

## Track 2: Agent-Driven Validation Workflows

### Current State

This is partially real, but not productized. Pulp already has strong building blocks for screenshot capture, diffing, inspection, and validator/sanitizer execution. It does not yet have a complete deterministic “agent controls plugin, validators, screenshots, reports, and failure artifacts” system.

Real building blocks:
- screenshot capture/diff/crop APIs
- `pulp design-debug` before/after/diff/report bundles
- validator workflows in CI
- sanitizer workflows in CI/docs
- synthetic click/drag and inspector APIs
- headless processing for audio validation

Missing:
- one durable report schema shared by CLI/tools
- complete validator coverage in `pulp validate`
- stable plugin-control/report contract for agents
- debugger control
- screenshot annotation workflow
- explicit handling of current limitations such as QuickJS-only runtime and partial validator coverage

### Honest Position

Say:
- “agent-friendly validation building blocks exist”

Do not say:
- “full autonomous agent validation runtime is shipped”

### Best Next Step

Turn the current building blocks into one deterministic validation harness before adding more “agent loop” language.

### Phase 1 Deliverables

- one stable validation bundle format containing:
  - screenshots
  - diffs
  - inspector JSON
  - validator outputs
  - sanitizer configuration metadata
- make `pulp validate` more explicit and truthful:
  - current CLAP coverage
  - current AU coverage
  - VST3/pluginval status
  - what is skipped when tools are missing
- replace placeholder MCP wrappers with real wrappers over:
  - click/drag
  - view tree
  - screenshot capture

### Phase 2 Deliverables

- stable plugin-control/report surface:
  - load preset
  - set/get parameter
  - send MIDI
  - process audio file/buffer
  - capture screenshot
  - export machine-readable report
- short agent-oriented orchestration loops on top of that surface

### Phase 3 Exploration

- screenshot annotations and regions of interest
- debugger integration only after the validation surface is stable
- autonomous issue triage/fix loops as an experiment, not a foundational promise

### Top Risks

- overclaiming “agent workflow” before the harness is deterministic
- mixing design-tool visual QA with general plugin runtime QA without a shared schema
- debugger integration before validation APIs are stable

---

## Track 3: APIs Agents Can Reason About and Use for Validation

### Current State

This is the strongest of the four tracks. Pulp already has useful low-level APIs:
- screenshot capture
- screenshot comparison
- crop and diff bounds
- inspector JSON
- synthetic click/drag
- JS bridge helpers
- design-debug and validation CLI entry points

The main gaps are protocol stability and truthfulness.

### Honest Position

- the agent-facing surface is **usable in pieces**
- the protocol/report contract is **not yet frozen**
- some docs still imply more maturity or broader coverage than the current code supports

### Best Next Step

Freeze and document a small, durable agent-validation contract before building higher-level agent behavior.

### Phase 1 Deliverables

- define a stable machine-readable contract for:
  - view tree JSON
  - screenshot capture metadata
  - diff metrics
  - parameter set/get
  - validation report schema
- mark experimental vs usable surfaces honestly in planning/docs
- expose real CLI/MCP endpoints rather than placeholders where those placeholders still exist

### Phase 2 Deliverables

- richer query semantics:
  - find control by id/type/label
  - get semantic control state
  - capture region by view id
  - compare targeted regions only
- standardize JSON artifacts so agent tools can chain without custom parsing

### Phase 3 Exploration

- higher-level semantic APIs:
  - “is this control visually disabled?”
  - “did the focused state appear?”
  - “did the waveform render change after processing?”
- domain-specific validation recipes built on the low-level contract

### Top Risks

- inventing a too-large protocol before stabilizing the useful core
- hiding experimental or backend-specific caveats from agents
- allowing docs to outrun implementation

---

## Track 4: WebGPU/Audio Exploration

### Current State

Pulp has a real GPU UI/rendering base through Dawn + Skia Graphite. It does **not** have real WebGPU compute-audio implementation today.

Current reality:
- render stack is real
- SkSL runtime effects are real
- CPU DSP primitives are real
- audio-to-UI publication primitives are real
- meter/waveform/spectrum display primitives are real

Not real:
- compute pipelines for audio DSP
- public WGSL compute API
- GPU resource model for audio workloads
- scheduling and safety model for audio-callback compute
- first-class STFT abstraction
- offline video rendering mode

### Honest Position

Do not pitch this as “almost there.” Treat it as exploration.

### Best Next Step

Prove usefulness with an isolated, non-realtime compute spike and keep the docs honest about the difference between:
- SkSL UI shaders
- WebGPU compute

### Phase 1 Deliverables

- documentation truth pass:
  - clearly separate SkSL/Skia effects from WebGPU compute
  - explicitly state QuickJS-only current reality where scripting is discussed
  - explicitly state current audio visualization reality: meter/waveform/spectrum + FFT, not STFT framework
- one compute feasibility spike, off the audio callback path:
  - offline FFT batch
  - spectrogram generation
  - or batch convolution
- benchmark/report:
  - CPU vs GPU crossover points
  - upload/readback overhead
  - platform stability
  - failure modes

### Phase 2 Deliverables

- UI-side GPU processing first:
  - analyzer/spectrogram generation
  - visualization helpers using already-published audio data
- an internal experimental compute resource model in `render` or a dedicated experimental module

### Phase 3 Exploration

- offline or high-latency workloads only:
  - additive synthesis batch rendering
  - spectral resynthesis
  - ML inference experiments
- external reference comparison:
  - `gpu.cpp` / AnswerDotAI `gpu.cpp` may be studied as external reference candidates only after a separate license/runtime-fit pass

### Phase 4 Exploration

- offline video rendering, only after:
  - validation bundles are stable
  - screenshot/video frame capture contracts exist
  - at least one WebGPU/audio experiment produced credible value

### Top Risks

- chasing GPU DSP prestige without a clear CPU baseline
- trying to put GPU work directly in the audio callback too early
- conflating UI shader support with compute support
- dragging in external reference code before the realtime/resource model is understood

---

## Documentation / Planning Overclaims To Fix

The repo should speak more carefully in these areas:

- **JS engine**
  - say “QuickJS today, abstraction seam exists”
  - do not say QuickJS/V8/JSC are current interchangeable backends
- **Multi-window**
  - say “not productized yet”
  - do not say the framework already supports multi-window in a shipped way
- **Audio visualization**
  - say “meter ballistics + waveform/spectrum + CPU FFT + TripleBuffer publication are real”
  - do not say STFT abstractions or spectrogram systems are already implemented
- **WebView**
  - say “optional embedded panel path, off by default”
  - do not confuse it with the primary native UI path
- **Validation**
  - say “pluginval/clap-validator/auval are present in CI with current caveats”
  - do not say `pulp validate` already covers the full validator matrix unless code changes
- **Sanitizers**
  - say “ASan, TSan, UBSan are real”
  - do not say RTSan is already present
- **WebGPU**
  - say “GPU-backed rendering and SkSL effects exist”
  - do not imply general compute-audio support
- **DSLs**
  - say “planned”
  - do not imply first-class support until FAUST Phase 1 lands
- **Offline video**
  - keep it firmly in future exploration

---

## Recommended Execution Order (v2)

### Dependency order

1. **Track 3 Phase 1 first**
   - stabilize the agent-usable validation/report contract
   - this is the prerequisite truth/contract layer
2. **After that, three tracks can run in parallel**
   - Track 2 Phase 1: deterministic validation harness
   - Track 1 Phase 1: FAUST offline codegen spike
   - Track 4 Phase 1: WebGPU/audio feasibility spike
3. **Track 2 Phase 2 depends on Track 2 Phase 1**
4. **Track 1 Phase 2 depends on Track 1 Phase 1**
5. **Offline video exploration depends on both**
   - stable validation artifacts/reporting
   - some meaningful WebGPU/audio exploration result
6. **Treat Cmajor, JSFX, autonomous agent loops, realtime compute audio, and offline video as later exploration unless earlier phases prove leverage**

### Parallelism summary

- **Must run first**
  - agent-validation/report contract truth pass
- **Can run in parallel after that**
  - deterministic validation harness
  - FAUST offline compile track
  - WebGPU/audio feasibility track
- **Cannot start early**
  - offline video exploration
  - production claims around multi-window agent tools
  - production claims around alternate JS engines

---

## Recommended Default Position

Near-term, Pulp should present itself as:
- a framework with real GPU-backed native UI rendering
- strong screenshot/inspection/debug seams
- real sanitizer and validator building blocks
- credible paths to DSLs and GPU-audio exploration

It should **not** yet present itself as:
- a first-class DSP DSL host
- a full autonomous agent-operated plugin runtime
- a multi-window design-tool framework
- a WebGPU compute-audio framework
- a framework with interchangeable QuickJS/V8/JSC backends
- a framework with offline video rendering support

That only becomes true after the corresponding Phase 1 work lands and is validated in code.
