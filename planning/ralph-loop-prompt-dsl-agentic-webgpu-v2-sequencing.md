"Execute the DSL / agentic-validation / WebGPU v2 track for Pulp.

References:
- planning/dsl-agentic-validation-webgpu-audit.md (v2 truth pass; source of sequencing)
- planning/phase-10-ai-shader-design-system.md (prompt style reference)
- planning/ralph-loop-prompt-phase10-shader-design.md (prompt structure reference)
- CLAUDE.md (build, test, architecture)
- planning/STATUS.md (use carefully; some entries are stale and must be verified against code)

GOAL:
Run the new track in the correct dependency order without overclaiming shipped support.

SEQUENCING RULES:
- Phase 1 is the contract/truth pass. It must land first.
- After Phase 1 lands, Phase 2, Phase 3, and Phase 4 may run in parallel if different people own them.
- Phase 5 cannot start until both the validation-harness work and the WebGPU/audio exploration work have produced stable artifacts.

PHASES:
1. planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase1-contract.md
   - Must run first
   - Stabilizes the truthful agent-validation/report contract

2. planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase2-harness.md
   - Depends on Phase 1
   - Can run in parallel with Phase 3 and Phase 4 after Phase 1

3. planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase3-faust.md
   - Depends on Phase 1
   - Can run in parallel with Phase 2 and Phase 4 after Phase 1

4. planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase4-webgpu.md
   - Depends on Phase 1
   - Can run in parallel with Phase 2 and Phase 3 after Phase 1

5. planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase5-offline-video.md
   - Depends on Phase 2 and Phase 4
   - Exploration only; do not pull this earlier

NON-NEGOTIABLES:
- Keep docs and prompts honest about current code:
  - QuickJS only today
  - optional WebView only, off by default
  - no shipped multi-window framework
  - metering ballistics + FFT + TripleBuffer publication are real
  - STFT abstractions are not verified in current code
  - ASan/TSan/UBSan are real; RTSan is not
  - `pluginval` exists in CI, but `pulp validate` does not yet run it
  - no `vstvalidator` claim
  - WebGPU/audio is exploration, not shipped compute support
- External references such as `gpu.cpp` / AnswerDotAI `gpu.cpp` are research inputs only, never source of truth.
- Do not start Cmajor, JSFX, realtime GPU-audio, or offline video as if they are baseline work.

SUCCESS CRITERIA:
- the first three implementation phases produce repo-backed changes or reports without inflated claims
- the WebGPU/audio phase ends with a benchmark/result write-up, not a promise
- later exploration phases stay explicitly gated behind earlier proof"
