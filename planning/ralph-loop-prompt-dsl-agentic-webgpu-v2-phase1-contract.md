"Implement Phase 1: truthful agent-validation contract and reality pass for the DSL / agentic-validation / WebGPU v2 track.

References:
- planning/dsl-agentic-validation-webgpu-audit.md
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
- core/view/CMakeLists.txt

GOAL:
Define the durable, truthful contract that the later phases will build on.

DEPENDENCIES:
- none; this phase must run first

CAN RUN IN PARALLEL:
- no; Phase 2, Phase 3, and Phase 4 wait for this phase

NON-NEGOTIABLES:
- Document QuickJS as the only current JS backend.
- Document WebView as optional and off by default.
- Document multi-window support as not productized today.
- Document audio visualization as:
  - MeterBallistics real
  - TripleBuffer latest-value publication real
  - FFT real
  - STFT abstraction not yet verified
- Document sanitizer reality as ASan/TSan/UBSan only.
- Document validator reality as:
  - pluginval in CI
  - clap-validator attempted in CI
  - auval check path in CI
  - `pulp validate` currently CLAP/AU oriented, not full validator parity
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
  - `vstvalidator` today
  - WebGPU compute-audio today"
