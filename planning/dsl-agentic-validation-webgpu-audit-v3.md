# DSL, Agentic Validation, WebGPU, and Full UI Framework Audit

**Revision:** v3
**Revision date:** 2026-03-31
**Status:** Planning, repo-backed truth pass

Purpose:
- audit what is real today in Pulp versus what is still planning/spec language
- define the execution order for an expanded set of tracks covering:
  1. DSP DSLs as first-class citizens (FAUST, Cmajor, JSFX — sequentially)
  2. agent-driven validation workflows + sanitizer/validator expansion
  3. agent-usable validation APIs
  4. WebGPU/audio exploration
  5. multi-window framework
  6. native WebView integration (iPlug2-style, not embedded browser)
  7. audio visualization system (STFT, spectrogram pipeline)
  8. themeable widget library + resource/asset system
  9. JS engine abstraction (V8/JSC beyond QuickJS)
  10. offline video rendering exploration
- record the current status of all cross-cutting topics that materially affect those tracks

This v3 supersedes v2. Key changes from v2:
- Cmajor and JSFX are now discrete phases (sequential after FAUST proves the DSL contract)
- Multi-window framework is a real phase, not just a reality check
- WebView integration is a real phase modeled on iPlug2's approach
- Audio visualization gets its own phase for STFT/spectrogram pipeline work
- Themeable widget library + resource/asset system gets its own phase
- JS engine abstraction gets its own phase
- RTSan and vstvalidator integration are explicit deliverables in the validation phase
- Offline video stays exploration-only with a pros/cons write-up

This v3 intentionally distinguishes:
- **real code today**
- **stale docs/spec claims**
- **future exploration**

---

## Executive Summary

Pulp already has real GPU-backed UI rendering, real screenshot capture and screenshot-diff tooling, real view inspection and synthetic interaction seams, real CPU DSP primitives, real sanitizer CI, real validator workflows in CI, a real token-based theme system with JSON serialization and built-in dark/light/pro_audio presets, a real style designer tool with per-token undo/redo and live hot-reload, a real set of audio-optimized widgets (Knob, Fader, Meter, XYPad, WaveformView, SpectrumView, etc.), and an optional CHOC-based WebView wrapper.

Pulp does **not** currently have:
- first-class FAUST, Cmajor, or JSFX support
- a shipped WebGPU compute-for-audio pipeline
- a stable closed-loop agent validation harness across plugins, screenshots, validators, and reports
- a shipped multi-window application framework (multiple hosts possible manually, no coordination)
- a verified first-class STFT abstraction for analyzer/spectrogram work
- alternate JS engines wired in beyond QuickJS
- RTSan or vstvalidator integration
- iPlug2-style native WebView with bidirectional JS↔C++ bridge (current WebView is optional CHOC panel)
- a centralized resource/asset manager
- theme import/export, OS appearance tracking, or accessibility-aware contrast logic
- offline video rendering

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
- Token-based theme system with JSON serialization, dark/light/pro_audio presets:
  [core/view/include/pulp/view/theme.hpp](../core/view/include/pulp/view/theme.hpp),
  [core/view/src/theme.cpp](../core/view/src/theme.cpp)
- Style designer tool with per-token editing, undo/redo, live hot-reload, preset selector:
  [examples/design-tool/design-tool.js](../examples/design-tool/design-tool.js)
- 13 token groups (Background, Text, Accent, Controls, Knob, Slider, Meter, Waveform, Cards, Overlay, Tabs, Effects, Gradient)
- Widget inventory — Knob, Fader, Toggle, Checkbox, ToggleButton, XYPad, Meter, WaveformView, SpectrumView, Panel, ScrollView, TabPanel, ListBox, TextEditor, Label, ComboBox, Tooltip, ProgressBar, CallOutBox, Icon, ImageView, CanvasWidget
- Audio visualization building blocks:
  [core/view/include/pulp/view/audio_bridge.hpp](../core/view/include/pulp/view/audio_bridge.hpp),
  [core/view/include/pulp/view/widgets.hpp](../core/view/include/pulp/view/widgets.hpp),
  [core/view/src/widgets.cpp](../core/view/src/widgets.cpp),
  [core/signal/include/pulp/signal/fft.hpp](../core/signal/include/pulp/signal/fft.hpp)
- Optional WebView wrapper (CHOC-based, off by default):
  [core/view/include/pulp/view/web_view.hpp](../core/view/include/pulp/view/web_view.hpp),
  [core/view/src/web_view.cpp](../core/view/src/web_view.cpp)
- Custom SkSL GPU shaders on widgets (Knob, Fader, Toggle):
  `set_custom_sksl()` in widgets.hpp
- Lottie animation support on widgets:
  `set_lottie_json()` in widgets.hpp
- Sanitizer CI: ASan, TSan, UBSan:
  [.github/workflows/sanitizers.yml](../.github/workflows/sanitizers.yml),
  [tools/cmake/Sanitizers.cmake](../tools/cmake/Sanitizers.cmake)
- Validator CI: pluginval, clap-validator attempted, auval scan:
  [.github/workflows/validate.yml](../.github/workflows/validate.yml)

### Not real yet

- FAUST/Cmajor/JSFX implementation in `core/`, `tools/`, templates, or CLI
- public WGSL/WebGPU compute API for audio workloads
- stable agent-validation protocol/schema shared across CLI and MCP
- multi-window coordination framework (palettes, inspectors, popups, multi-monitor)
- iPlug2-style native WebView with bidirectional message bridge and custom URL scheme
- verified first-class STFT abstraction with configurable window/overlap
- production spectrogram pipeline
- alternate JS engines wired in beyond QuickJS
- RTSan integration
- vstvalidator integration
- centralized resource/asset manager (font loading, image caching, shader compilation error handling, resource bundling)
- theme import/export, OS appearance change tracking, accessibility-aware contrast
- component-level style variants (primary/secondary button, etc.)
- multi-channel meter, EQ curve widget, MIDI keyboard widget, piano roll, step sequencer, file browser
- offline video rendering

---

## Cross-Cutting Reality Checks

### 1. Multi-window support current status

Real code:
- `WindowHost` is a single host abstraction with `create(View&, WindowOptions)`:
  [core/view/include/pulp/view/window_host.hpp](../core/view/include/pulp/view/window_host.hpp)
- standalone/app examples create a single window host
- platform hosts are per-window implementations, not a higher-level multi-window framework:
  [core/view/platform/mac/window_host_mac.mm](../core/view/platform/mac/window_host_mac.mm)

Audit position:
- current status is **partial/manual at best**
- multiple windows may be possible by manually constructing multiple hosts, but there is no:
  - shared lifecycle/coordination
  - floating palette support
  - inspector window management
  - popup menu routing across windows
  - multi-monitor awareness
  - window state persistence/restoration
- do not describe multi-window support as shipped framework capability today

### 2. WebView support current status

Real code:
- WebView exists behind `PULP_BUILD_WEBVIEW`:
  [core/view/CMakeLists.txt](../core/view/CMakeLists.txt)
- implementation wraps `choc::ui::WebView` (ISC license):
  [core/view/src/web_view.cpp](../core/view/src/web_view.cpp)
- supports navigate/set_html/evaluate_js/bind

Audit position:
- optional WebView support is **real but off by default**
- current approach is CHOC-based embedded panel
- does NOT yet match iPlug2's model which includes:
  - custom URL scheme handlers for local resource serving
  - structured bidirectional message bridge (JSON-based)
  - resource bundling for offline use
  - DevTools enable/disable
- Pulp's main UI path is native view/canvas/render, not WebView
- WebView is an "escape hatch" for rich content (Monaco editor, HTML dashboards, etc.)

### 3. Audio visualization system status

Real today:
- meter ballistics real
- latest-value publication via TripleBuffer real
- visualization widgets: Meter, WaveformView, SpectrumView
- FFT primitives real

Not verified:
- first-class STFT abstraction with configurable window/overlap
- production spectrogram pipeline
- multi-channel meter support

### 4. Theme system status

Real today:
- Token-based Theme struct with color/dimension/string maps
- JSON serialization (from_json/to_json)
- Built-in presets: dark(), light(), pro_audio()
- Style designer tool with 13 token groups, per-token undo/redo, live hot-reload
- Preset selector in design tool (Default Dark, Light, Pro Audio, Violet, Amber, Ocean, Neon)
- Theme application via View::set_tokens()

Not real yet:
- OS appearance change detection (auto light/dark switching)
- Theme import/export (JSON file save/load from UI)
- Accessibility-aware contrast logic (auto foreground/background contrast)
- Component-level style variants (primary/secondary/danger button)
- Theme inheritance/composition
- Theme versioning or migration

### 5. Widget inventory gaps

Pulp ships a solid set of audio-optimized widgets. Comparing against JUCE, iPlug2, and Visage, notable gaps include:
- Multi-channel meter (only single Meter today)
- EQ curve visualization widget
- MIDI keyboard / piano roll widget
- Step sequencer widget
- File browser widget
- Color picker widget
- Native dialog integration (file open/save, color picker)
- Drag-and-drop file handling
- Keyboard shortcut registration system
- Gesture recognizers (pinch-zoom, rotate) for touch

### 6. Resource/asset system status

Real today:
- ImageView loads from file paths
- SkSL shaders passed as strings
- Lottie JSON passed as strings
- No centralized manager

Not real:
- Centralized asset manager/loader
- Resource caching
- Font preloading or custom font file loading
- Shader compilation error handling
- Resource bundling for distribution
- SVG support

### 7. JS engine abstraction status

Real code:
- ScriptEngine wraps choc::javascript::Context, explicitly QuickJS only
- Abstraction seam exists but no alternate backends implemented

### 8. Sanitizer and validator reality

Real today:
- ASan, TSan, UBSan in CI and CMake
- pluginval in CI (VST3)
- clap-validator attempted in CI
- auval check path in CI
- `pulp validate` covers CLAP and AU

Not real:
- RTSan (not in CMake, CI, or toolchain)
- vstvalidator (not in repo workflows)
- `pulp validate` does not invoke pluginval for VST3

#### Validator Coverage Matrix

| Tool | In CI? | In `pulp validate`? | Notes |
|------|--------|---------------------|-------|
| pluginval | Yes | No | CI only, not in CLI |
| clap-validator | Attempted | Yes | CLI runs CLAP validation |
| auval | Yes | Yes | AU scan/check with signing caveats |
| vstvalidator | No | No | Not integrated |
| RTSan | No | No | Not integrated |

### 9. Offline video rendering

No code. Future exploration only.

### 10. gpu.cpp / AnswerDotAI gpu.cpp

No repo-local implementation or dependency. External research candidate only.

### 11. Three.js / WebGPU JS bridge feasibility

Proven by reference implementations:
- **react-native-webgpu** (~/Code/react-native-webgpu): JSI HostObjects wrapping WebGPU C++ objects,
  PlatformContext abstraction over Metal/Vulkan, thread-safe surface registry, manual frame scheduling.
  This is the closest existing proof that the pattern works.
- **react-native-skia** (~/Code/react-native-skia): Skia + native UI compositing via dedicated views,
  GPU context sharing (GrDirectContext/MTLDevice shared between Skia and native rendering).
  Proves Skia and a second GPU consumer can share a device.
- **Three.js WebGPU renderer** (~/Code/three.js): 2600-line WebGPUBackend.js uses ~20 core WebGPU
  interfaces. Node-based material system (TSL) generates WGSL. Extensive resource caching.

Audit position:
- the pattern is **proven in production** by react-native-webgpu
- Pulp's stack (Dawn + Skia + QuickJS/V8) maps directly to this pattern
- the key risk is API surface size (~20 core interfaces to bind) and Three.js library parse time in QuickJS
- V8 is strongly recommended for Three.js workloads
- compositing 2D Skia + 3D WebGPU requires a dedicated render target approach (not shared surface interleaving)

---

## Recommended Execution Order (v3)

### Phase overview

| Phase | Name | Depends on | Can parallelize with |
|-------|------|------------|---------------------|
| 1 | Contract/truth pass | — | — |
| 2 | Validation harness + sanitizer/validator expansion | 1 | 3, 6, 7, 8, 9, 10 |
| 3 | FAUST offline codegen | 1 | 2, 6, 7, 8, 9, 10 |
| 4 | Cmajor adapter | 3 | 2, 6, 7, 8, 9, 10 |
| 5 | JSFX adapter | 4 | 2, 6, 7, 8, 9, 10 |
| 6 | Multi-window framework | 1 | 2, 3, 8, 9, 10 |
| 7 | WebView integration (iPlug2-style) | 6 | 2, 3, 4, 8, 9, 10 |
| 8 | Audio visualization (STFT/spectrogram) | 1 | 2, 3, 6, 9, 10 |
| 9 | Themeable widgets + resource/asset system | 1 | 2, 3, 6, 8, 10 |
| 10 | JS engine abstraction (V8/JSC) | 1 | 2, 3, 6, 8, 9 |
| 11 | WebGPU compute exploration | 1 | 2, 3, 6, 8, 9, 10 |
| 12 | Offline video exploration | 2, 11 | — |
| 13 | Three.js / WebGPU JS bridge | 10, 11 | 12 |

### Dependency chains

```
Phase 1 (contract)
├── Phase 2 (validation harness + RTSan/vstvalidator)
│   └── Phase 12 (offline video exploration) ←── also needs Phase 11
├── Phase 3 (FAUST)
│   └── Phase 4 (Cmajor)
│       └── Phase 5 (JSFX)
├── Phase 6 (multi-window)
│   └── Phase 7 (WebView)
├── Phase 8 (audio visualization)
├── Phase 9 (themeable widgets + assets)
├── Phase 10 (JS engine abstraction) ──┐
│                                       ├── Phase 13 (Three.js / WebGPU JS bridge)
└── Phase 11 (WebGPU compute) ─────────┤
    └── Phase 12 (offline video)        │
                                        └── Phase 13
```

### Sequencing rules

- Phase 1 must land first. All other phases depend on it.
- DSL phases are strictly sequential: FAUST → Cmajor → JSFX. Each proves the contract before the next begins.
- Multi-window must land before WebView (WebView windows need the coordination framework).
- Offline video cannot start until both the validation harness and WebGPU exploration produce stable artifacts.
- All other phases can run in parallel after Phase 1 if different people/agents own them.

### Parallelism summary

- **Must run first**: Phase 1 (contract)
- **Can run in parallel after Phase 1**: Phases 2, 3, 6, 8, 9, 10, 11
- **Strictly sequential chains**: 3→4→5 (DSLs), 6→7 (multi-window→WebView)
- **Cannot start early**: Phase 12 (offline video), Phase 13 (Three.js bridge — needs Phases 10 + 11)

---

## Documentation / Planning Overclaims To Fix

The repo should speak more carefully in these areas:

- **JS engine**: say "QuickJS today, abstraction seam exists" — not interchangeable backends
- **Multi-window**: say "not productized yet" — not shipped framework
- **Audio visualization**: say "meter ballistics + waveform/spectrum + CPU FFT + TripleBuffer real" — not STFT framework
- **WebView**: say "optional CHOC-based embedded panel, off by default" — not iPlug2-style integration
- **Theme system**: say "token-based with JSON serialization, 3 built-in presets, style designer with hot-reload" — not full theme management platform with import/export/OS tracking
- **Widget library**: say "solid audio-optimized set with knobs, faders, meters, XY pads" — not complete application UI framework
- **Validation**: say "pluginval/clap-validator/auval present in CI with caveats" — not full validator parity
- **Sanitizers**: say "ASan, TSan, UBSan are real" — not RTSan
- **WebGPU**: say "GPU-backed rendering and SkSL effects exist" — not general compute-audio
- **DSLs**: say "planned" — not first-class support
- **Offline video**: keep it firmly in future exploration
- **Resource system**: say "file-path-based loading exists" — not centralized asset management

---

## Recommended Default Position

Near-term, Pulp should present itself as:
- a framework with real GPU-backed native UI rendering via Dawn/Skia Graphite
- a token-based theming system with a live style designer
- a solid set of audio-optimized widgets
- strong screenshot/inspection/debug seams
- real sanitizer and validator building blocks
- credible paths to DSLs, multi-window, and GPU-audio exploration

It should **not** yet present itself as:
- a first-class DSP DSL host
- a full autonomous agent-operated plugin runtime
- a multi-window design-tool framework
- a WebGPU compute-audio framework
- a framework with interchangeable JS backends
- a framework with offline video rendering support
- a complete application UI framework on par with JUCE's widget breadth

That only becomes true after the corresponding phase work lands and is validated in code.
