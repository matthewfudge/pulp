"Implement Phase 3: FAUST-first DSL enablement for the DSL / agentic-validation / WebGPU v2 track.

References:
- planning/dsl-agentic-validation-webgpu-audit.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase1-contract.md
- core/format/include/pulp/format/processor.hpp
- core/format/include/pulp/format/headless.hpp
- core/state/include/pulp/state/store.hpp
- tools/plugin-cli/plugin_cli.hpp
- tools/cmake/PulpUtils.cmake

GOAL:
Make one DSP DSL real in the narrowest credible way: FAUST offline code generation into Pulp's existing Processor model.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phase 2 and Phase 4

NON-NEGOTIABLES:
- FAUST only in this phase.
- No Cmajor work in this phase.
- No JSFX work in this phase.
- Offline codegen only; do not start with hot reload.
- Generated output must map cleanly into:
  - `Processor`
  - `StateStore`
  - existing headless validation flows
- Every shipped claim must be backed by buildable examples and tests.

DELIVERABLES:
1. A `pulp-dsl` contract for:
   - parameter reflection
   - state mapping
   - MIDI/bus mapping
   - generated-wrapper ownership
   - compile/build diagnostics
2. FAUST compile flow:
   - source `.dsp`
   - generated C++ wrapper
   - wrapper subclasses `Processor`
3. Small examples:
   - gain
   - filter
   - one simple synth/effect example
4. Headless validation:
   - compile
   - build
   - instantiate
   - process fixed buffers
   - verify output

ACCEPTANCE:
- FAUST support is real and test-backed
- docs say FAUST is supported only at the implemented scope
- nothing in this phase implies multi-DSL parity"
