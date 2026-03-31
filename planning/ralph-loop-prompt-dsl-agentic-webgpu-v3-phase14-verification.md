"Implement Phase 14: verification pass for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/v3-phase-status.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-sequencing.md
- All phase prompt files (phases 1-13)
- docs/contracts/phase1-reality-snapshot.yaml
- tools/cli/pulp_cli.cpp
- docs/reference/cli.md
- docs/reference/capabilities.md
- docs/status/support-matrix.yaml
- docs/status/modules.yaml
- docs/status/cli-commands.yaml

GOAL:
Verify every phase delivered what it promised. This is not a rubber stamp — it is an independent
audit that reads each phase's acceptance criteria, checks the code, runs the tests, and confirms
the docs match reality. Any phase that fails verification gets a deficiency report with specific
remediation steps.

DEPENDENCIES:
- Requires ALL phases (1-13) to be merged to main

CAN RUN IN PARALLEL:
- no; this is the final gate

NON-NEGOTIABLES:
- Do not trust phase self-reports. Verify independently by reading code and running tests.
- Every claim in docs must be backed by code. If docs say 'FAUST is supported', verify
  pulp dsp compile actually works with a .dsp file.
- Every CLI command listed in docs must actually work when run.
- Every test must pass (ctest --test-dir build --output-on-failure).
- The verification pass itself must be documented — produce a verification report, not just
  'it looks fine'.

VERIFICATION CHECKLIST PER PHASE:

### Phase 1 — Contract/truth pass
- [ ] docs/contracts/phase1-reality-snapshot.yaml exists and is machine-readable
- [ ] status labels (shipped/partial/experimental/planned) are present and accurate
- [ ] no doc claims capabilities beyond what code implements

### Phase 2 — Validation harness
- [ ] pulp validate --all runs and produces a JSON report bundle
- [ ] RTSan is in CMake (PULP_SANITIZER=realtime) — verify cmake configure accepts it
- [ ] pluginval is callable from pulp validate for VST3 (or gracefully skips if not installed)
- [ ] vstvalidator has a documented go/no-go decision
- [ ] validation bundle contains: screenshots, diffs, inspector JSON, validator outputs
- [ ] failure artifacts are deterministic (run twice, compare)

### Phase 3 — FAUST offline codegen
- [ ] pulp dsp compile works with a .dsp file
- [ ] generated C++ wrapper subclasses Processor
- [ ] 3 examples exist and build: gain, filter, synth/effect
- [ ] headless validation: compile → build → instantiate → process → verify output
- [ ] pulp-dsl contract document exists and is referenced by Phase 4

### Phase 4 — Cmajor adapter
- [ ] Cmajor .cmajor files load into Processor interface
- [ ] 3 examples exist and build
- [ ] FAUST examples still pass (regression)
- [ ] pulp-dsl contract is stable or changes documented

### Phase 5 — JSFX adapter
- [ ] EEL2 @init/@slider/@block/@sample sections execute correctly
- [ ] slider1..slider64 mapped to StateStore
- [ ] scope boundary document exists (what's supported vs not)
- [ ] no @gfx support (documented as planned)
- [ ] 3 examples exist and build
- [ ] FAUST and Cmajor examples still pass (regression)

### Phase 6 — Multi-window framework
- [ ] WindowManager coordinates multiple windows
- [ ] floating palette windows work in standalone
- [ ] parent-child close cascading works
- [ ] theme propagates across windows
- [ ] parameter state shared across windows
- [ ] shared Dawn device across all windows (verify: not multiple devices)

### Phase 7 — WebView integration
- [ ] WKWebView on macOS, WebView2 on Windows (no Chromium bundled)
- [ ] bidirectional JS↔native message bridge works
- [ ] custom URL scheme serves bundled resources
- [ ] Monaco editor or equivalent example works
- [ ] docs position WebView as optional compatibility layer

### Phase 8 — Audio visualization
- [ ] STFT abstraction exists with configurable window/FFT size/hop size
- [ ] spectrogram pipeline renders with multiple color ramps
- [ ] multi-channel metering (at least stereo + quad verified)
- [ ] all audio→UI paths are lock-free (no mutex in audio thread path)
- [ ] AudioBridge data accessible from JS (for Phase 13 forward compat)

### Phase 9 — Widgets + assets + themes
- [ ] All 38 tweakcn presets ported with light/dark pairs
- [ ] derivation layer: accent → knob.arc, destructive → meter.red, etc. verified
- [ ] OS appearance tracking: simulate light↔dark switch, verify theme changes
- [ ] theme import/export: export JSON, reimport, verify identical
- [ ] WCAG AA contrast validation: known-bad pair triggers warning
- [ ] new widgets exist and are token-aware: EqCurveView, MidiKeyboard, ColorPicker, etc.
- [ ] AssetManager: load image from file, embedded resource, memory buffer
- [ ] AssetManager callable from JS bindings (Phase 13 forward compat)
- [ ] style designer updated with new capabilities

### Phase 10 — JS engine abstraction
- [ ] QuickJS backend passes shared test suite
- [ ] JSC backend works on macOS (if Apple hardware available)
- [ ] V8 backend works on desktop
- [ ] PULP_JS_ENGINE=auto|quickjs|jsc|v8 CMake option works
- [ ] HostObject-style native binding API exists (Phase 13 forward compat)
- [ ] TypedArray zero-copy between JS and C++ verified
- [ ] Promise support from C++ to JS verified
- [ ] performance benchmarks documented

### Phase 11 — WebGPU compute exploration
- [ ] at least one compute experiment ran with benchmark data
- [ ] CPU vs GPU crossover documented
- [ ] Dawn device sharing with Skia verified (Phase 13 forward compat)
- [ ] go/no-go recommendation documented
- [ ] docs no longer conflate SkSL effects with WebGPU compute

### Phase 12 — Offline video exploration
- [ ] pros/cons document exists and is honest
- [ ] feasibility investigation results documented
- [ ] minimal frame sequence export works (if feasible)
- [ ] concrete recommendation (proceed/park/revisit) documented

### Phase 13 — Three.js / WebGPU JS bridge
- [ ] Three.js WebGPU renderer initializes without errors
- [ ] spinning cube renders in a Pulp window alongside native 2D widgets
- [ ] 3D spectrum analyzer example: audio in → FFT → Three.js → screen
- [ ] reactive particle visualizer example works
- [ ] room reverb visualizer example works
- [ ] 3D waveform ribbon example works
- [ ] Tier 1 + Tier 2 WebGPU interfaces pass unit tests
- [ ] no GPU memory leaks over 100 create/destroy cycles
- [ ] V8 performance within 2x of Chrome for GPU-bound workloads
- [ ] bridge is optional (PULP_BUILD_WEBGPU_JS_BRIDGE flag)
- [ ] QuickJS works for simple scenes with documented limitations

### Cross-Cutting Verification
- [ ] pulp CLI: all new commands work (dsp compile, validate --all, theme list, etc.)
- [ ] claude/ skills updated for new capabilities
- [ ] docs/reference/cli.md matches actual CLI commands
- [ ] docs/reference/capabilities.md matches actual capabilities
- [ ] docs/status/ manifests are current
- [ ] no overclaims anywhere in docs — every claim backed by code
- [ ] all tests pass: ctest --test-dir build --output-on-failure
- [ ] clean build from scratch: cmake -S . -B build && cmake --build build
- [ ] validate-build.sh passes

DELIVERABLES:
1. Verification report (planning/v3-verification-report.md):
   - per-phase: pass/fail with evidence
   - deficiency list with specific remediation steps for each failure
   - cross-cutting findings
   - overall go/no-go for v3 completeness
2. Updated v3-phase-status.md:
   - Verified column filled for each phase (pass/fail + date)
3. Deficiency branches (if needed):
   - for each failed verification item, a specific branch with the fix
   - re-verify after fix merges

ACCEPTANCE:
- every phase has been independently verified against its acceptance criteria
- the verification report exists and is honest
- all deficiencies have remediation steps (even if not yet fixed)
- v3-phase-status.md is fully populated
- the repo is in a state where an outsider reading the docs gets an accurate picture

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120
