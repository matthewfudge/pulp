"Implement Phase 1: truthful agent-validation contract and reality pass for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- tools/cli/pulp_cli.cpp
- docs/reference/cli.md
- docs/reference/capabilities.md
- docs/concepts/overview.md
- .github/workflows/validate.yml
- .github/workflows/sanitizers.yml
- core/view/include/pulp/view/script_engine.hpp
- core/view/src/script_engine.cpp
- core/view/include/pulp/view/window_host.hpp
- core/view/include/pulp/view/audio_bridge.hpp
- core/view/include/pulp/view/web_view.hpp
- core/view/include/pulp/view/theme.hpp
- core/view/src/theme.cpp
- core/view/CMakeLists.txt
- examples/design-tool/design-tool.js

GOAL:
Define the durable, truthful contract that the later 11 phases will build on.

DEPENDENCIES:
- none; this phase must run first

CAN RUN IN PARALLEL:
- no; all other phases wait for this phase

NON-NEGOTIABLES:
- Document QuickJS as the only current JS backend.
- Document WebView as optional CHOC-based panel, off by default.
- Document multi-window support as not productized today.
- Document audio visualization as:
  - MeterBallistics real
  - TripleBuffer latest-value publication real
  - FFT real
  - Meter, WaveformView, SpectrumView widgets real
  - STFT abstraction not yet verified
  - multi-channel meter not yet implemented
- Document sanitizer reality as ASan/TSan/UBSan only.
- Document validator reality as:
  - pluginval in CI (VST3)
  - clap-validator attempted in CI
  - auval check path in CI
  - pulp validate currently CLAP/AU oriented, not full validator parity
  - vstvalidator not integrated
  - RTSan not integrated
- Document theme system as:
  - token-based with JSON serialization
  - 3 built-in presets (dark, light, pro_audio)
  - 13 token groups in style designer
  - no import/export, no OS appearance tracking, no contrast logic
- Document widget inventory honestly:
  - solid audio set (Knob, Fader, Toggle, XYPad, Meter, WaveformView, SpectrumView, etc.)
  - missing: multi-channel meter, EQ curve, MIDI keyboard, piano roll, step sequencer, file browser, color picker
- Document resource/asset system as file-path-based loading only, no centralized manager.
- Do not invent new capabilities in order to make the matrix cleaner.

DELIVERABLES:
1. A small machine-readable validation/report contract for:
   - screenshot metadata
   - diff metrics
   - inspector/view-tree payloads
   - validator results
   - sanitizer metadata
2. A truth pass over any planning/docs surface touched by this phase so claims match code.
3. Clear status labels:
   - shipped
   - partial
   - experimental
   - planned
4. A short dependency map that later phases can consume without re-litigating current reality.

ACCEPTANCE:
- later prompts can point to this phase as the authoritative contract
- no prompt or doc produced by this phase claims:
  - JSC/V8 support today
  - STFT framework today
  - multi-window support today
  - RTSan today
  - vstvalidator today
  - WebGPU compute-audio today
  - theme import/export today
  - centralized asset management today
  - iPlug2-style WebView today"
