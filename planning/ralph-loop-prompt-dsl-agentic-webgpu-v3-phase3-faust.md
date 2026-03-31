"Implement Phase 3: FAUST-first DSL enablement for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/format/include/pulp/format/processor.hpp
- core/format/include/pulp/format/headless.hpp
- core/state/include/pulp/state/store.hpp
- tools/plugin-cli/plugin_cli.hpp
- tools/cmake/PulpUtils.cmake

GOAL:
Make one DSP DSL real in the narrowest credible way: FAUST offline code generation into Pulp's existing Processor model. This phase proves the pulp-dsl contract that Cmajor (Phase 4) and JSFX (Phase 5) will build on.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 6, 8, 9, 10, 11
- Phase 4 (Cmajor) cannot start until this phase lands

NON-NEGOTIABLES:
- FAUST only in this phase.
- No Cmajor work in this phase.
- No JSFX work in this phase.
- Offline codegen only; do not start with hot reload.
- Generated output must map cleanly into:
  - Processor
  - StateStore
  - existing headless validation flows
- Every shipped claim must be backed by buildable examples and tests.
- The pulp-dsl contract defined here must be general enough for Cmajor and JSFX to reuse without being so abstract it invents unused machinery.

DELIVERABLES:
1. A pulp-dsl contract for:
   - parameter reflection into StateStore
   - MIDI/bus mapping
   - state/preset serialization
   - generated-wrapper ownership boundaries
   - compile/build error reporting
2. FAUST compile flow:
   - source .dsp
   - generated C++ wrapper
   - wrapper subclasses Processor
   - FAUST metadata mapped into ParamInfo
3. Small examples:
   - gain
   - filter
   - one simple synth/effect example
4. Headless validation:
   - compile
   - build
   - instantiate
   - process fixed buffers
   - verify output against known-good reference
5. Tests that validate:
   - parameter count and names match FAUST metadata
   - audio output matches expected values for known inputs
   - state serialization round-trips correctly
   - build errors produce clear diagnostics

ACCEPTANCE:
- FAUST support is real and test-backed
- the pulp-dsl contract is documented and ready for Phase 4 to consume
- docs say FAUST is supported only at the implemented scope
- nothing in this phase implies multi-DSL parity

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120