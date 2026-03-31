"Implement Phase 10: JS engine abstraction for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/view/include/pulp/view/script_engine.hpp
- core/view/src/script_engine.cpp

GOAL:
Abstract the JS engine layer so the best engine can be used per platform: V8 on desktop for performance, JavaScriptCore on Apple platforms for zero-dependency integration, and QuickJS as the portable fallback everywhere.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 3, 6, 8, 9, 11

EXISTING FOUNDATION:
- ScriptEngine wraps choc::javascript::Context, explicitly constructs createQuickJSContext()
- the abstraction seam exists but no alternate backends are implemented
- the JS bridge (web-compat.js, widget creation, layout, event callbacks) runs on this engine

NON-NEGOTIABLES:
- The abstraction must not degrade QuickJS performance or correctness. QuickJS remains the default and must work identically.
- The JS bridge layer (web-compat.js, widget creation, theme/token APIs, event callbacks) must work unchanged across all backends. The engine swap must be invisible to JS code.
- Engine selection can be build-time (CMake option) or runtime per-context.
- Do not vendor V8 or JSC source. Use system JSC on Apple; for V8, document the dependency and build integration.
- CHOC's JavaScript abstraction may be a useful starting point, but the Pulp abstraction owns its own contract.
- Circular reference safety (the ;void 0 pattern for load_script evals) must be preserved across all backends.
- License compatibility: V8 is BSD-3-Clause, JSC is LGPL-2.1 (system framework use is fine on Apple). Document this.
- FORWARD COMPATIBILITY FOR PHASE 13 (Three.js bridge):
  Phase 13 requires a fast native-object binding API so C++ HostObjects (wrapping Dawn GPU objects)
  can be exposed to JS with minimal overhead. This is the pattern react-native-webgpu uses (JSI
  HostObjects). Each engine backend MUST support:
  - creating opaque C++ objects accessible from JS (get/set properties, call methods)
  - passing TypedArrays (Float32Array, Uint8Array, etc.) between JS and C++ without copying
  - returning Promises from C++ to JS (for async GPU operations like buffer.mapAsync())
  - destructor callbacks when JS objects are garbage collected (to release Dawn refs)
  V8's JIT makes it viable for real Three.js scenes (~1MB library parse + complex scene graphs).
  QuickJS will work for simple 3D scenes but Three.js library parse time may exceed 1s.
  Design the engine abstraction with these requirements in mind even if Phase 13 runs later.

DELIVERABLES:
1. Engine abstraction interface:
   - create context (with engine preference or auto-select)
   - evaluate script (with error reporting)
   - call function
   - bind native function
   - get/set global values
   - garbage collection hints
   - context lifecycle (create, reset, destroy)
2. QuickJS backend (already exists, refactor to new interface):
   - verify all existing tests pass after refactor
   - verify circular reference safety
   - verify CHOC bridge functions work
3. JavaScriptCore backend:
   - Apple platforms only (macOS, iOS)
   - uses system JSC (JavaScriptCore.framework)
   - no additional dependency
   - passes the same test suite as QuickJS
4. V8 backend:
   - desktop platforms (macOS, Windows, Linux)
   - document V8 integration (build from source or pre-built)
   - passes the same test suite as QuickJS
   - isolate/context lifecycle management
5. Engine selection:
   - CMake option: PULP_JS_ENGINE=auto|quickjs|jsc|v8
   - auto: JSC on Apple, V8 on desktop if available, QuickJS otherwise
   - runtime query: which engine is active
6. Performance benchmarks:
   - widget creation/layout benchmark across engines
   - script evaluation benchmark
   - memory usage comparison
   - startup time comparison
7. Tests:
   - shared test suite that runs against all available backends
   - bridge function compatibility (every web-compat.js function)
   - circular reference safety on all backends
   - error reporting consistency
   - garbage collection behavior
   - engine switch does not change JS code behavior

ACCEPTANCE:
- QuickJS continues to work identically (no regression)
- JSC works on macOS/iOS with the full JS bridge
- V8 works on desktop with the full JS bridge
- engine selection is configurable at build time
- performance benchmarks are documented
- docs describe the engine abstraction and per-platform defaults


Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120
