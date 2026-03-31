"Implement Phase 7: native WebView integration (iPlug2-style) for the v3 track.

References:
- planning/dsl-agentic-validation-webgpu-audit-v3.md
- planning/ralph-loop-prompt-dsl-agentic-webgpu-v3-phase6-multiwindow.md
- core/view/include/pulp/view/web_view.hpp
- core/view/src/web_view.cpp
- core/view/CMakeLists.txt
- https://github.com/iPlug2/iPlug2/tree/master/IPlug/Extras/WebView (architecture reference)

GOAL:
Upgrade the existing optional CHOC-based WebView into an iPlug2-style native WebView integration. This means using platform-native rendering engines (WKWebView on macOS/iOS, WebView2 on Windows) — NOT embedding a full browser like Chromium/Electron.

POSITIONING:
WebView is an optional compatibility layer — supported but not encouraged. The primary Pulp UI path is native Dawn/Skia rendering with the JS-authored widget system. WebView exists as an escape hatch for specific use cases where existing web content needs to be embedded (Monaco editor, HTML documentation viewers, third-party web-based tools). Developers should default to native Pulp UI for all new work. WebView is for interop, not for building plugin UIs.

DEPENDENCIES:
- Requires Phase 6 (multi-window framework) — WebView windows need coordination

CAN RUN IN PARALLEL:
- can run in parallel with Phases 2, 3, 4, 8, 9, 10, 11

ARCHITECTURE MODEL (from iPlug2):
- Native WebView, not embedded browser
  - macOS/iOS: WKWebView (WebKit, ships with OS)
  - Windows: WebView2 (Chromium-based Edge engine, ships with Windows 10+)
- Bidirectional message bridge:
  - Native → WebView: evaluate_js() for async JS execution
  - WebView → Native: JSON message handler via platform API
    (WKScriptMessageHandler on macOS, ICoreWebView2WebMessageReceivedEventHandler on Windows)
- Custom URL scheme handler for local resource serving
  (serve bundled HTML/CSS/JS without an HTTP server)
- DevTools toggle for development
- Resource bundling for offline use in plugin distributions

NON-NEGOTIABLES:
- Do NOT embed Chromium, CEF, Electron, or any full browser engine.
- Use the OS-provided WebView (WKWebView, WebView2).
- The primary Pulp UI remains native Dawn/Skia. WebView is opt-in per window/panel.
- WebView panels must participate in the multi-window framework from Phase 6.
- The bridge must be simple: async JSON messages + JS evaluation. No complex RPC.
- WebView content must be servable from bundled resources (no internet dependency for plugin UIs).
- Build flag: PULP_BUILD_WEBVIEW (already exists, keep it opt-in).

DELIVERABLES:
1. Upgraded WebView integration:
   - WKWebView backend on macOS/iOS
   - WebView2 backend on Windows
   - Common interface matching current web_view.hpp API + extensions
2. Bidirectional bridge:
   - evaluate_js(code, callback) — native calls JS, optionally receives result
   - bind(name, handler) — JS calls native with JSON args, receives JSON response
   - Structured message protocol (JSON envelope with type/payload)
3. Custom URL scheme handler:
   - Register pulp:// or custom scheme
   - Serve bundled HTML/CSS/JS/assets from plugin resources
   - No HTTP server needed
4. Resource bundling:
   - CMake helper to embed web assets in plugin bundle
   - Asset loading from bundle at runtime
5. DevTools support:
   - Enable/disable browser developer tools per WebView instance
   - macOS: Safari Web Inspector attachment
   - Windows: Edge DevTools
6. Examples:
   - Minimal WebView panel showing HTML content
   - Monaco editor integration example (load Monaco from bundled resources)
   - Hybrid example: native Pulp controls + WebView panel in same window
7. Tests:
   - WebView creation and destruction lifecycle
   - Bidirectional message passing (native→JS and JS→native)
   - Resource loading from bundle
   - WebView in multi-window context (palette with WebView content)
   - Graceful fallback when WebView is not available

ACCEPTANCE:
- WebView works on macOS (WKWebView) and Windows (WebView2)
- Monaco editor or equivalent rich web content can be embedded in a Pulp plugin
- No browser engine is bundled — only OS-native WebView
- WebView panels integrate with the multi-window framework
- docs describe WebView as optional native integration, not embedded browser

Use repo prompt when searching the codebase

ONLY WHEN ALL CONDITIONS ARE MET:
Output exactly: <promise>DONE</promise>

IF STUCK:
- After 20 iterations, document in LEARNINGS.md:
- What is blocked
- Why
- What was attempted
- What assumption may be wrong" --completion-promise "DONE" --max-iterations 120