"Implement Phase 2: deterministic validation harness for the DSL / agentic-validation / WebGPU v2 track.

References:
- planning/dsl-agentic-validation-webgpu-audit.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v2-phase1-contract.md
- tools/design/pulp_design_debug.cpp
- tools/cli/pulp_cli.cpp
- core/view/include/pulp/view/screenshot.hpp
- core/view/include/pulp/view/screenshot_compare.hpp
- core/view/include/pulp/view/inspector.hpp
- core/format/include/pulp/format/headless.hpp
- .github/workflows/validate.yml
- docs/guides/testing-advanced.md

GOAL:
Turn the existing screenshot, diff, inspection, validator, and headless-processing pieces into one deterministic validation harness with stable artifacts.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phase 3 and Phase 4 if file ownership does not conflict

NON-NEGOTIABLES:
- Build on existing primitives; do not invent a fresh validation stack.
- Prefer deterministic bundles over “agent autonomy” features.
- Keep validator coverage honest:
  - preserve current CLAP/AU reality
  - add VST3/pluginval only when the code path actually exists
- No debugger integration in this phase.
- No screenshot annotation workflow in this phase.
- If a path is headless-only or not GPU-faithful, label it clearly.

DELIVERABLES:
1. One validation bundle/report format containing:
   - screenshots
   - diffs
   - crop/region metadata
   - inspector JSON
   - validator outputs
   - sanitizer metadata
2. One stable control surface for validation flows:
   - load preset
   - set/get parameter
   - send MIDI
   - process buffers/files
   - capture screenshots
   - emit report JSON
3. Honest CLI/MCP surfaces over those primitives.
4. Tests covering:
   - happy path bundles
   - missing validator tools
   - predictable failure artifact generation

ACCEPTANCE:
- an agent can run a short validation loop without bespoke parsing per command
- failure artifacts are deterministic and inspectable
- docs and status surfaces match the implemented harness"
