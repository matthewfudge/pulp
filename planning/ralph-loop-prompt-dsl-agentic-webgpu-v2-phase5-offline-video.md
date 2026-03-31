"Implement Phase 5: offline video rendering exploration for the DSL / agentic-validation / WebGPU v2 track.

References:
- planning/dsl-agentic-validation-webgpu-audit.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase2-harness.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase4-webgpu.md
- tools/design/pulp_design_debug.cpp
- core/view/include/pulp/view/screenshot.hpp
- core/view/include/pulp/view/screenshot_compare.hpp

GOAL:
Explore offline video rendering only after the validation artifact pipeline and GPU exploration work have produced something stable enough to build on.

DEPENDENCIES:
- Requires Phase 2 validation harness outputs
- Requires Phase 4 WebGPU/audio feasibility outputs

CAN RUN IN PARALLEL:
- no; this is a later exploration phase

NON-NEGOTIABLES:
- This is not a baseline product feature.
- No promise of realtime video authoring.
- No promise of DAW/plugin-host integrated video workflows.
- Build on stable screenshot/frame artifact contracts first.
- If the underlying capture path is headless-only or not GPU-faithful, say so.

DELIVERABLES:
1. A narrow exploration scope:
   - frame sequence export
   - timeline-less before/after clips
   - validation replay capture
2. A write-up of what capture path is actually being used:
   - headless render
   - live window capture
   - or hybrid
3. An honest list of blockers:
   - performance
   - determinism
   - composited GPU capture fidelity
   - storage/encoding cost

ACCEPTANCE:
- the repo ends up with a clear exploration outcome and a decision
- docs do not describe offline video rendering as shipped unless real code lands and is tested"
