"Implement Phase 2: deterministic validation harness + sanitizer/validator expansion for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- tools/design/pulp_design_debug.cpp
- tools/cli/pulp_cli.cpp
- tools/cmake/Sanitizers.cmake
- core/view/include/pulp/view/screenshot.hpp
- core/view/include/pulp/view/screenshot_compare.hpp
- core/view/include/pulp/view/inspector.hpp
- core/format/include/pulp/format/headless.hpp
- .github/workflows/validate.yml
- .github/workflows/sanitizers.yml
- docs/guides/testing-advanced.md

GOAL:
Turn the existing screenshot, diff, inspection, validator, and headless-processing pieces into one deterministic validation harness with stable artifacts. Expand sanitizer and validator coverage to include RTSan, vstvalidator, and pluginval in pulp validate.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 3, 6, 8, 9, 10, 11

NON-NEGOTIABLES:
- Build on existing primitives; do not invent a fresh validation stack.
- Prefer deterministic bundles over 'agent autonomy' features.
- Keep validator coverage honest — add tools only when the code path actually exists and is tested.
- No debugger integration in this phase.
- No screenshot annotation workflow in this phase.
- If a path is headless-only or not GPU-faithful, label it clearly.
- Short feedback loops: build → sanitize → validate → report must complete quickly enough for iterative development.

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
3. Sanitizer expansion:
   - RTSan integration in CMake (PULP_SANITIZER=realtime)
   - RTSan CI workflow
   - Documentation of RTSan requirements (Clang 18+, platform support)
   - Reference: https://clang.llvm.org/docs/RealtimeSanitizer.html
4. Validator expansion:
   - pluginval integration in pulp validate for VST3
   - vstvalidator evaluation and integration if viable
   - Updated validator coverage matrix in docs
   - pulp validate --all runs every available validator
   - Graceful skip when tools are not installed, with clear reporting
5. Honest CLI/MCP surfaces over those primitives.
6. Tests covering:
   - happy path bundles
   - missing validator tools (graceful degradation)
   - predictable failure artifact generation
   - sanitizer detection of known-bad patterns

ACCEPTANCE:
- an agent can run a short validation loop without bespoke parsing per command
- failure artifacts are deterministic and inspectable
- RTSan is integrated and documented (even if platform-limited)
- vstvalidator is evaluated with a go/no-go decision documented
- pluginval is callable from pulp validate
- docs and status surfaces match the implemented harness

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120