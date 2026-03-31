"Implement Phase 5: JSFX adapter for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase3-faust.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase4-cmajor.md
- The pulp-dsl contract defined in Phase 3 (possibly refined in Phase 4)
- core/format/include/pulp/format/processor.hpp
- core/state/include/pulp/state/store.hpp
- https://www.reaper.fm/sdk/js/js.php (JSFX reference)

GOAL:
Add JSFX as the third DSL, completing the DSL trilogy. JSFX is the most scope-risky of the three — this phase must be explicitly bounded.

DEPENDENCIES:
- Requires Phase 4 (Cmajor) — the pulp-dsl contract must be proven across two DSLs

CAN RUN IN PARALLEL:
- can run in parallel with Phases 2, 6, 7, 8, 9, 10, 11, 12

NON-NEGOTIABLES:
- Use the pulp-dsl contract from Phases 3/4 without major contract changes.
- Scope control is critical. JSFX has a large surface area (EEL2 language, graphics section, file I/O, MIDI, sysex, serialize). Start with audio-only — no @gfx section in this phase.
- Do not attempt full REAPER JSFX compatibility. Document explicitly what subset is supported.
- Every shipped claim must be backed by buildable examples and tests.
- If the contract needs changes, document what and why, and verify FAUST and Cmajor still work.

DELIVERABLES:
1. JSFX adapter that maps into the pulp-dsl contract:
   - parameter reflection (slider1..slider64 → StateStore)
   - MIDI mapping (@block MIDI access)
   - state/preset serialization (@serialize section or equivalent)
   - compile/load error reporting
2. JSFX interpreter or transpiler:
   - EEL2 core language support (arithmetic, conditionals, loops, memory access)
   - @init, @slider, @block, @sample sections
   - No @gfx in this phase (documented as planned/future)
   - No file I/O in this phase
3. Scope boundary document:
   - what JSFX features are supported
   - what is explicitly out of scope
   - what REAPER-specific behaviors are intentionally different
4. Small examples:
   - gain
   - filter
   - one example exercising JSFX-specific idioms (e.g., memory-based delay line)
5. Tests that validate:
   - parameter count and names match JSFX slider definitions
   - audio output matches expected values for known inputs
   - state serialization round-trips correctly
   - unsupported features produce clear error messages, not silent failures
   - FAUST and Cmajor examples still pass (regression check)

ACCEPTANCE:
- JSFX support is real, test-backed, and scope-bounded
- FAUST and Cmajor support are not broken
- the pulp-dsl contract is stable across all three DSLs
- docs clearly state what JSFX subset is supported
- no claim of full REAPER JSFX compatibility

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120