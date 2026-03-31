"Implement Phase 12: offline video rendering exploration for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase2-harness.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase11-webgpu.md
- tools/design/pulp_design_debug.cpp
- core/view/include/pulp/view/screenshot.hpp
- core/view/include/pulp/view/screenshot_compare.hpp

GOAL:
Explore offline video rendering with a concrete feasibility assessment, proposed architecture, and honest pros/cons write-up. This is exploration only — not a shipped feature commitment.

DEPENDENCIES:
- Requires Phase 2 validation harness outputs (stable screenshot/frame artifact contracts)
- Requires Phase 11 WebGPU/audio feasibility outputs (GPU capture/rendering viability data)

CAN RUN IN PARALLEL:
- no; this is the last exploration phase

CONCEPT:
Offline video rendering mode renders high-quality audio-visual content on a frame-by-frame basis, decoupled from real-time playback. This enables:
- music visualization videos (album art animations, reactive visuals)
- plugin demo videos (knob turns, parameter sweeps, before/after)
- automated validation captures (UI state at specific processing points)
- educational content (step-by-step DSP visualization)

Unlike real-time screen recording, offline rendering:
- runs at arbitrary resolution (4K, 8K)
- renders every frame perfectly (no dropped frames)
- synchronizes audio and visual output exactly
- can take as long as needed per frame

NON-NEGOTIABLES:
- This is not a baseline product feature. It is an exploration.
- No promise of realtime video authoring.
- No promise of DAW/plugin-host integrated video workflows.
- Build on stable screenshot/frame artifact contracts from Phase 2.
- If the underlying capture path is headless-only or not GPU-faithful, say so.
- The exploration must end with a concrete recommendation, not open-ended possibility.

DELIVERABLES:
1. Feasibility investigation:
   - can the existing render pipeline produce deterministic per-frame output?
   - what is the per-frame render cost at 1080p, 4K?
   - can audio processing be stepped frame-by-frame in lockstep with rendering?
   - what encoding options are viable (FFmpeg piped output, native AVFoundation, raw frame sequence)?

2. Proposed architecture (document only, minimal code):
   - OfflineRenderer that drives Processor + View in lockstep:
     - advance audio by N samples (one video frame worth at target fps)
     - render UI to offscreen buffer
     - emit frame to encoder or frame sequence
   - configuration: resolution, fps, audio sample rate, duration
   - output formats: PNG sequence, MP4 (via FFmpeg), ProRes (via AVFoundation on macOS)

3. Narrow exploration scope (minimal working code):
   - frame sequence export: render N frames of a plugin UI to PNG sequence
   - audio-synced: audio processing advances in lockstep with frame rendering
   - basic timeline: start time, end time, fps

4. Pros/cons write-up:

   PROS:
   - perfect frame accuracy (no screen recording artifacts)
   - arbitrary resolution (not limited by display)
   - deterministic output (same input → same output)
   - audio-visual synchronization guaranteed
   - enables automated visual regression testing at scale
   - enables high-quality promotional/educational content
   - builds on existing headless render infrastructure

   CONS:
   - rendering time scales with resolution and complexity (not real-time)
   - GPU capture fidelity may differ from live rendering
   - encoding dependency (FFmpeg or platform-specific encoders)
   - storage cost for high-resolution frame sequences
   - UI state management complexity (animations, transitions must be deterministic)
   - limited utility if the primary use case is real-time performance
   - maintenance burden for a feature with narrow audience

   OPEN QUESTIONS:
   - is headless GPU rendering faithful enough, or does it need a real window context?
   - can Skia/Dawn render to offscreen framebuffer at arbitrary resolution?
   - what is the acceptable render time per frame? (target: <100ms at 1080p, <500ms at 4K)
   - should this be a CLI tool (pulp render-video) or a library API?
   - is there demand beyond demo videos and validation captures?
   - if Phase 13 (Three.js bridge) lands, can offline video render Three.js 3D scenes
     frame-by-frame at arbitrary resolution? This would enable high-quality 3D music
     visualization videos rendered offline — a compelling use case.

5. Honest blockers list:
   - performance
   - determinism (animations, timers)
   - composited GPU capture fidelity
   - storage/encoding cost
   - cross-platform encoder availability

6. Go/no-go recommendation:
   - based on the exploration results, recommend one of:
     a. proceed to implementation as a CLI tool (pulp render-video)
     b. proceed to implementation as a library API only
     c. park as interesting-but-not-worth-the-investment
     d. revisit after specific prerequisite work lands

ACCEPTANCE:
- the repo ends up with a clear exploration outcome and a decision
- the pros/cons document is honest and complete
- minimal working code demonstrates frame sequence export if feasible
- docs do not describe offline video rendering as shipped unless real code lands and is tested
- the recommendation is concrete and actionable

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120