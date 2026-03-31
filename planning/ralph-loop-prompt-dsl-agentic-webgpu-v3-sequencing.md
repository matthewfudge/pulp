"Execute the DSL / agentic-validation / WebGPU / UI framework v3 track for Pulp.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md (v3 truth pass; source of sequencing)
- planning/phase-10-ai-shader-design-system.md (prompt style reference)
- planning/ralph-loop-prompt-phase10-shader-design.md (prompt structure reference)
- CLAUDE.md (build, test, architecture)
- planning/STATUS.md (use carefully; some entries are stale and must be verified against code)
- ~/Code/three.js (Three.js source — WebGPU renderer analysis for Phase 13)
- ~/Code/react-native-webgpu (reference: JSI HostObject → WebGPU native binding pattern)
- ~/Code/react-native-skia (reference: Skia + native compositing, GPU context sharing)

GOAL:
Run the expanded v3 track in the correct dependency order without overclaiming shipped support.

v3 CHANGES FROM v2:
- Cmajor and JSFX are now discrete sequential phases after FAUST proves the DSL contract
- Multi-window framework is a real implementation phase
- WebView integration (iPlug2-style native, not embedded browser) is a real implementation phase
- Audio visualization gets its own phase for STFT/spectrogram pipeline
- Themeable widget library + resource/asset system gets its own phase
- JS engine abstraction (V8/JSC) gets its own phase
- RTSan and vstvalidator are explicit deliverables in the validation phase
- Offline video stays exploration-only with pros/cons write-up

SEQUENCING RULES:
- Phase 1 is the contract/truth pass. It must land first.
- After Phase 1 lands, Phases 2, 3, 6, 8, 9, 10, 11 may run in parallel if different people own them.
- DSL phases are strictly sequential: Phase 3 (FAUST) → Phase 4 (Cmajor) → Phase 5 (JSFX).
- Multi-window (Phase 6) must land before WebView (Phase 7).
- Phase 12 (offline video) cannot start until both the validation harness (Phase 2) and WebGPU exploration (Phase 11) have produced stable artifacts.

PHASES:
1. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
   - Must run first
   - Stabilizes the truthful agent-validation/report contract

2. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase2-harness.md
   - Depends on Phase 1
   - Validation harness + RTSan/vstvalidator expansion

3. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase3-faust.md
   - Depends on Phase 1
   - FAUST offline codegen — proves the DSL contract

4. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase4-cmajor.md
   - Depends on Phase 3
   - Cmajor adapter using the proven DSL contract

5. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase5-jsfx.md
   - Depends on Phase 4
   - JSFX adapter — scope-bounded, tests validate compatibility

6. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase6-multiwindow.md
   - Depends on Phase 1
   - Multi-window framework for palettes, inspectors, popups, multi-monitor

7. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase7-webview.md
   - Depends on Phase 6
   - iPlug2-style native WebView integration (not embedded browser)

8. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase8-audio-viz.md
   - Depends on Phase 1
   - STFT abstractions, spectrogram pipeline, multi-channel meters

9. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase9-widgets-assets.md
   - Depends on Phase 1
   - Themeable widget expansion, resource/asset system, theme management platform

10. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase10-jsengine.md
    - Depends on Phase 1
    - JS engine abstraction: V8 on desktop, JSC on Apple, QuickJS as portable fallback

11. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase11-webgpu.md
    - Depends on Phase 1
    - WebGPU compute exploration (non-realtime feasibility spike)

12. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase12-offline-video.md
    - Depends on Phase 2 and Phase 11
    - Exploration only with pros/cons write-up

13. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase13-threejs-bridge.md
    - Depends on Phase 10 (JS engine abstraction, V8 strongly preferred) and Phase 11 (WebGPU exploration)
    - Native Three.js via WebGPU JS API bindings against Dawn — no browser, no WebView

14. planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase14-verification.md
    - Depends on ALL phases (1-13) merged to main
    - Independent audit: reads each phase's acceptance criteria, checks code, runs tests
    - Produces verification report with pass/fail per phase and deficiency remediation steps
    - Status tracked in planning/v3-phase-status.md

DEPENDENCY GRAPH:
Phase 1 (contract)
├── Phase 2 (validation + RTSan/vstvalidator)
│   └── Phase 12 (offline video exploration) ← also needs Phase 11
├── Phase 3 (FAUST)
│   └── Phase 4 (Cmajor)
│       └── Phase 5 (JSFX)
├── Phase 6 (multi-window)
│   └── Phase 7 (WebView)
├── Phase 8 (audio visualization)
├── Phase 9 (widgets + assets + themes)
├── Phase 10 (JS engine abstraction) ──┐
│                                       ├── Phase 13 (Three.js/WebGPU JS bridge)
└── Phase 11 (WebGPU compute) ─────────┤
    └── Phase 12 (offline video)        │
                                        └── Phase 13

CROSS-CUTTING REQUIREMENT — CLI, CLAUDE PLUGIN, AND SKILLS MUST GROW WITH FEATURES:
Every phase that adds new capabilities MUST also update:
1. pulp CLI (tools/cli/pulp_cli.cpp):
   - Phase 2: pulp validate --all, pulp validate --rtsan, pulp validate --vstvalidator
   - Phase 3: pulp dsp compile foo.dsp (FAUST)
   - Phase 4: pulp dsp compile foo.cmajor (Cmajor)
   - Phase 5: pulp dsp compile foo.jsfx (JSFX)
   - Phase 6: (no CLI changes expected)
   - Phase 8: pulp viz benchmark (visualization perf testing)
   - Phase 9: pulp theme list, pulp theme import, pulp theme export, pulp theme validate
   - Phase 10: pulp engine list, pulp engine benchmark
   - Phase 11: pulp gpu benchmark (compute feasibility)
   - Phase 13: pulp threejs test (Three.js bridge smoke test)
2. Claude Code plugin (claude/):
   - Update skills and commands so agents can use new capabilities
   - New commands should be discoverable: agent asks "what can Pulp do?" → gets current reality
   - Phase 2: validation skills (run validators, capture reports)
   - Phase 3-5: DSL compilation skills
   - Phase 9: theme management skills (apply theme, list presets, validate contrast)
   - Phase 13: Three.js scene creation/testing skills
3. Documentation (docs/):
   - docs/reference/cli.md updated with new commands
   - docs/reference/capabilities.md updated with new capabilities
   - docs/status/ manifests updated (support-matrix.yaml, modules.yaml, cli-commands.yaml)
   - Example pages added for new features
4. Design tool / style designer:
   - Phase 9: new widgets appear in preview, new theme presets available, dimension/string token editing

Each phase's ACCEPTANCE criteria implicitly include: CLI commands work, docs match implementation,
and agents can discover and use the new capability. Do not mark a phase complete if the CLI and
docs are stale.

NON-NEGOTIABLES:
- Keep docs and prompts honest about current code:
  - QuickJS only today
  - optional WebView only, off by default, CHOC-based not iPlug2-style
  - no shipped multi-window framework
  - metering ballistics + FFT + TripleBuffer publication are real
  - STFT abstractions are not verified in current code
  - ASan/TSan/UBSan are real; RTSan is not
  - pluginval exists in CI, but pulp validate does not yet run it for VST3
  - no vstvalidator claim
  - WebGPU/audio is exploration, not shipped compute support
  - theme system is token-based with JSON and 3 presets; no import/export/OS-tracking yet
  - widget set is solid for audio but missing some categories (EQ curve, MIDI keyboard, etc.)
- External references such as gpu.cpp / AnswerDotAI gpu.cpp are research inputs only
- DSL phases are strictly sequential: FAUST → Cmajor → JSFX
- WebView phase must follow iPlug2's pattern (native WKWebView/WebView2, not embedded Chromium)

SUCCESS CRITERIA:
- each phase produces repo-backed changes or reports without inflated claims
- DSL phases each have tests validating the specific DSL before the next begins
- the WebGPU/audio phase ends with a benchmark/result write-up, not a promise
- offline video exploration ends with a concrete pros/cons/feasibility document
- later exploration phases stay explicitly gated behind earlier proof

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
  - What is blocked
  - Why
  - What was attempted
  - What assumption may be wrong" --completion-promise "DONE" --max-iterations 120
