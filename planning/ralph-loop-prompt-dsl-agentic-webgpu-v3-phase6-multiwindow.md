"Implement Phase 6: multi-window framework for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase1-contract.md
- core/view/include/pulp/view/window_host.hpp
- core/view/platform/mac/window_host_mac.mm
- core/view/platform/ios/window_host_ios.mm
- core/format/src/standalone.cpp
- examples/ui-preview/main.cpp
- examples/design-tool/main.cpp

GOAL:
Build a multi-window coordination framework on top of the existing WindowHost abstraction. Enable independent windows for floating palettes, inspectors, custom popup menus, and multi-monitor workflows.

DEPENDENCIES:
- Requires Phase 1 contract/truth pass

CAN RUN IN PARALLEL:
- yes, after Phase 1
- can run in parallel with Phases 2, 3, 8, 9, 10, 11
- Phase 7 (WebView) depends on this phase

NON-NEGOTIABLES:
- Build on the existing WindowHost abstraction. Do not replace it.
- Multi-window must work for both standalone apps and plugin editors (where the host provides the parent window).
- Platform differences must be handled honestly:
  - macOS: NSWindow with child/floating window levels
  - Windows: HWND with WS_EX_TOOLWINDOW for palettes
  - Plugin hosts: some hosts restrict window creation; document limitations
- Event loop ownership must be clear — plugins don't own the event loop.
- Window lifecycle must be explicit: who creates, who owns, who destroys.
- GPU rendering context sharing between windows must be addressed (shared Dawn device or per-window).

DELIVERABLES:
1. WindowManager coordination layer:
   - registry of active windows
   - window creation with type hints (main, palette, inspector, popup, dialog)
   - parent-child window relationships
   - window z-ordering and focus management
   - window close cascading (closing main closes child palettes)
2. Window types:
   - main editor window (existing, unchanged)
   - floating palette (stays on top of main, no taskbar entry)
   - inspector window (resizable, dockable to edge)
   - popup/dropdown (ephemeral, auto-dismiss on focus loss)
   - dialog (modal or modeless)
3. Multi-monitor support:
   - enumerate available screens
   - window placement on specific screens
   - DPI-aware rendering per monitor
   - window state restoration (position, size, screen)
4. Inter-window communication:
   - shared token/theme state (theme change propagates to all windows)
   - shared parameter state (parameter change visible in all windows)
   - message passing between windows for custom events
5. GPU context sharing across windows:
   - all windows MUST share the same Dawn wgpu::Device (do not create per-window devices)
   - each window gets its own surface/swapchain on the shared device
   - FORWARD COMPATIBILITY FOR PHASE 13 (Three.js bridge):
     Phase 13 needs a ThreeJSView widget that renders 3D into a Dawn texture on the shared
     device. The multi-window framework must ensure the shared device is accessible to any
     window, and that a 3D view in a floating palette uses the same device as the main editor.
5. Platform implementations:
   - macOS: NSWindow levels, child window ordering
   - Windows: HWND owner relationships, WS_EX_TOOLWINDOW
   - Stub/no-op for platforms that cannot support multi-window (iOS, embedded)
6. Tests:
   - window creation and destruction lifecycle
   - parent-child cascading close
   - theme propagation across windows
   - parameter sync across windows
   - graceful degradation when multi-window is not available

ACCEPTANCE:
- floating palette and inspector windows work in standalone apps
- plugin editors can create child windows (with documented host caveats)
- theme and parameter state are shared across all windows
- window state can be saved and restored
- docs describe multi-window as shipped with known platform limitations

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120