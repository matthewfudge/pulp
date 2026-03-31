"Implement Phase 4: WebGPU/audio feasibility for the DSL / agentic-validation / WebGPU v2 track.

References:
- planning/dsl-agentic-validation-webgpu-audit.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase1-contract.md
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
- can run in parallel with Phase 2 and Phase 3

NON-NEGOTIABLES:
- Treat this as exploration, not productization.
- Keep SkSL UI shaders and WebGPU compute clearly separate.
- Do not run GPU work directly in the audio callback.
- Current audio-visualization reality must stay explicit:
  - MeterBallistics real
  - TripleBuffer publication real
  - waveform/spectrum widgets real
  - FFT real
  - no verified STFT framework yet
- QuickJS is the current scripting reality; do not design this phase around a hypothetical engine swap.
- External references such as `gpu.cpp` / AnswerDotAI `gpu.cpp` may be consulted only as research candidates, never as assumed architecture.

DELIVERABLES:
1. One isolated compute experiment, such as:
   - offline FFT batch
   - spectrogram generation
   - batch convolution
2. Benchmark/report covering:
   - CPU vs GPU crossover points
   - upload/readback overhead
   - platform stability
   - failure modes
3. Documentation truth pass so the repo no longer conflates:
   - Dawn/Skia GPU rendering
   - SkSL runtime effects
   - WebGPU compute for audio
4. A go/no-go recommendation for further GPU-audio work.

ACCEPTANCE:
- the result is benchmark-backed
- the phase ends with a recommendation, not hype
- no shipped docs imply Pulp already supports general compute-audio"
