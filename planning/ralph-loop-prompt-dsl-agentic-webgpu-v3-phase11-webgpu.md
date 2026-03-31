"Implement Phase 11: WebGPU/audio feasibility exploration for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/render/src/gpu_surface_dawn.cpp
- core/render/src/skia_surface.cpp
- core/canvas/src/skia_canvas.cpp
- core/view/include/pulp/view/audio_bridge.hpp
- core/view/include/pulp/view/widgets.hpp
- core/view/src/widgets.cpp
- core/signal/include/pulp/signal/fft.hpp
- docs/guides/custom-rendering.md

GOAL:
Run one serious, non-realtime WebGPU/audio feasibility spike and write down the result honestly.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 3, 6, 8, 9, 10

NON-NEGOTIABLES:
- Treat this as exploration, not productization.
- Keep SkSL UI shaders and WebGPU compute clearly separate.
- Do not run GPU work directly in the audio callback.
- Current audio-visualization reality must stay explicit:
  - MeterBallistics real
  - TripleBuffer publication real
  - waveform/spectrum widgets real
  - FFT real
  - STFT framework may exist from Phase 8 if it ran first
- QuickJS is the current scripting reality (or the selected engine from Phase 10 if it ran first).
- External references such as gpu.cpp / AnswerDotAI gpu.cpp may be consulted only as research candidates, never as assumed architecture.
- FORWARD COMPATIBILITY FOR PHASE 13 (Three.js bridge):
  Phase 13 needs a shared Dawn wgpu::Device between Skia Graphite and the WebGPU JS bridge.
  This phase MUST verify that:
  - Skia Graphite's Dawn device can be obtained and passed to external code
  - a second consumer (not Skia) can create render targets on the same device
  - command buffers from two sources can be submitted to the same queue without corruption
  - GPU memory pressure from two allocators on the same device is manageable
  Document the device-sharing approach in the feasibility report so Phase 13 can build on it.
  Reference: react-native-webgpu uses a PlatformContext that owns the device and shares it;
  react-native-skia shares GrDirectContext/MTLDevice between Skia and native rendering.

DELIVERABLES:
1. One isolated compute experiment, such as:
   - offline FFT batch
   - spectrogram generation
   - batch convolution
2. Benchmark/report covering:
   - CPU vs GPU crossover points
   - upload/readback overhead
   - platform stability (macOS Metal vs Windows D3D12 vs Vulkan)
   - failure modes
3. Documentation truth pass so the repo no longer conflates:
   - Dawn/Skia GPU rendering
   - SkSL runtime effects
   - WebGPU compute for audio
4. A go/no-go recommendation for further GPU-audio work.

ACCEPTANCE:
- the result is benchmark-backed
- the phase ends with a recommendation, not hype
- no shipped docs imply Pulp already supports general compute-audio

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120
