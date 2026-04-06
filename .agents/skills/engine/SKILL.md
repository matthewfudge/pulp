---
name: engine
description: Query, recommend, and switch the Pulp JS engine backend (QuickJS, JavaScriptCore, V8). Handles "which JS engine", "switch to V8", "engine for Three.js".
requires:
  scripts: []
  tools: []
---

# JS Engine Skill

Manage the JavaScript engine backend used by Pulp's scripting layer. Three engines are available:

| Engine | Platform | Strengths | License |
|--------|----------|-----------|---------|
| **QuickJS** | All | Portable, small, zero dependencies. Default. | MIT |
| **JavaScriptCore** | Apple only | System framework, good JIT, zero-dep on macOS/iOS | LGPL-2.1 (system use OK) |
| **V8** | Desktop | Best JIT, ideal for heavy JS (Three.js), largest footprint | BSD-3-Clause |

## Commands

### `status` — Show current engine configuration

1. Read `CMakeCache.txt` in the build directory to find `PULP_JS_ENGINE`:
   ```bash
   grep PULP_JS_ENGINE build/CMakeCache.txt 2>/dev/null || echo "Not configured (default: QuickJS)"
   ```
2. Report which engines are available on this platform:
   - QuickJS: always
   - JSC: only on macOS/iOS
   - V8: only if `V8_INCLUDE_DIR` and `V8_LIB_DIR` are set
3. Show the current default engine for this build.

### `recommend <workload>` — Suggest the best engine

Based on the workload description:

- **Three.js / heavy 3D scenes**: Recommend **V8** (JIT compilation makes ~1MB library parse viable, complex scene graphs need fast execution). If V8 not available, warn that QuickJS will work but parse time may exceed 1 second.
- **Standard plugin UIs**: Recommend **QuickJS** (portable, proven, all existing code tested against it).
- **Apple-only shipping**: Recommend **JSC** (zero dependency, good performance, system framework).
- **Cross-platform shipping**: Recommend **QuickJS** (works identically everywhere).

### `switch <engine>` — Change the JS engine

**IMPORTANT: Always confirm with the user before switching.**

1. Determine the requested engine (quickjs, jsc, v8).
2. Check availability:
   - If `jsc` on non-Apple: explain it's not available, suggest alternatives.
   - If `v8` without V8 libs: explain V8 must be built/installed separately, link to docs.
3. **Use AskUserQuestion** to confirm the change:
   - Show the current engine
   - Show what will change
   - Warn about any implications (e.g., reconfigure + full rebuild required)
   - Ask "Switch JS engine to X?" with Yes/No options
4. If confirmed, run:
   ```bash
   cd <project_root>
   cmake -S . -B build -DPULP_JS_ENGINE=<engine>
   cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
   ```
   Or via the CLI:
   ```bash
   pulp build --js-engine=<engine>
   ```
5. After build succeeds, run tests to verify nothing broke:
   ```bash
   ctest --test-dir build --output-on-failure -E "AudioWorkgroup|GpuSurface"
   ```

### Auto-detection hint

When reviewing or loading JS code, if you see any of these patterns, proactively suggest an engine:

- `THREE.Scene`, `THREE.WebGLRenderer`, `import * as THREE` → suggest V8
- Large JS files (>500KB) → suggest V8 for parse performance
- Apple-only target (iOS app, AU-only plugin) → mention JSC as an option

Use `recommend` logic above, but **never auto-switch** — always confirm first.

## Engine Selection Semantics

- `auto` (default): QuickJS everywhere. Backward compatible. Safe.
- `quickjs`: Explicit QuickJS. Same as auto today.
- `jsc`: JavaScriptCore on Apple. Build fails on non-Apple.
- `v8`: V8 on desktop. Requires V8 headers/libs. Build fails without them.

The engine choice is a **build-time** CMake option. Changing it requires reconfigure + rebuild. The abstraction ensures all JS bridge code works identically across engines — the switch is invisible to UI scripts.
