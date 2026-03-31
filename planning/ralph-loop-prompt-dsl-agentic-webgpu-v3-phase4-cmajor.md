"Implement Phase 4: Cmajor adapter for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase3-faust.md
- The pulp-dsl contract defined in Phase 3
- core/format/include/pulp/format/processor.hpp
- core/format/include/pulp/format/headless.hpp
- core/state/include/pulp/state/store.hpp
- https://cmajor.dev/docs

GOAL:
Add Cmajor as the second DSL, reusing the pulp-dsl contract proven by FAUST in Phase 3.

DEPENDENCIES:
- Requires Phase 3 (FAUST) — the pulp-dsl contract must be proven and landed
- Phase 5 (JSFX) waits for this phase

CAN RUN IN PARALLEL:
- can run in parallel with Phases 2, 6, 7, 8, 9, 10, 11 (but NOT Phase 5)

NON-NEGOTIABLES:
- Use the pulp-dsl contract from Phase 3 without major contract changes.
  - If the contract needs adjustment, document what changed and why.
  - Any contract change must not break existing FAUST support.
- No JSFX work in this phase.
- Cmajor has its own compilation model (JIT or AOT). Choose the narrowest credible first step.
- Every shipped claim must be backed by buildable examples and tests.

DELIVERABLES:
1. Cmajor adapter that maps into the pulp-dsl contract:
   - parameter reflection into StateStore
   - MIDI/bus mapping
   - state/preset serialization
   - compile/build error reporting
2. Cmajor compile/load flow:
   - source .cmajor file(s)
   - compiled/loaded into Processor interface
   - Cmajor endpoints mapped to ParamInfo
3. Small examples:
   - gain
   - filter
   - one example that exercises Cmajor-specific features (e.g., graph composition, event endpoints)
4. Headless validation:
   - compile
   - load
   - instantiate
   - process fixed buffers
   - verify output against known-good reference
5. Tests that validate:
   - parameter count and names match Cmajor endpoint metadata
   - audio output matches expected values for known inputs
   - state serialization round-trips correctly
   - build/load errors produce clear diagnostics
   - FAUST examples still pass (regression check)

ACCEPTANCE:
- Cmajor support is real and test-backed
- FAUST support is not broken
- the pulp-dsl contract is stable or changes are documented
- docs say Cmajor is supported only at the implemented scope

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120