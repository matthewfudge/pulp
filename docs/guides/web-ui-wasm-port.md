# Pulp Native UI → Web (Emscripten + Canvas2D) — Feasibility & Build Plan

Date: 2026-06-28
Status: design / feasibility spike (no implementation merged yet)
Companion to `docs/guides/web-plugins.md` (which covers DSP-to-web via
WAMv2/WebCLAP). This doc covers the **other half**: running a Pulp plugin's
native C++ **editor** (a `View` with `paint()` + mouse handlers, including
`DesignFrameView`) in the browser.

## Why this exists

Concrete driver: take a plugin authored with a rich Pulp editor (the
`knob-playground` / DDFX editor: `DesignFrameView` over an exported SVG, with
macro knobs, dropdowns, FX menus, reorder, hover) and render it on a website as
an **interactive product manual** — the real UI, clickable, kept in sync with
ongoing native UI work, with **no per-element web re-authoring**.

The naive route (hand-port each control to HTML/Canvas2D in JavaScript) does not
scale: the editor is imperative C++ Canvas drawing (120+ paint calls in
`knob-playground/src/view.cpp`) plus interaction state machines, and the JS port
permanently lags the C++. The sustainable route is to compile the **same C++
view** to WebAssembly and give it a browser-canvas backend, so every element the
author adds in Pulp appears on the web for free.

## Bottom Line

Feasible, and the architecture is favourable — but it is real engineering, not a
build flag. Three findings make it tractable:

- **`Canvas` is already an abstract interface with pluggable backends.** Adding a
  browser backend is the intended extension point, not a hack. It has only **22
  pure-virtual methods** (`core/canvas/include/pulp/canvas/canvas.hpp`), plus a
  handful of non-pure ones a real UI needs (`fill_path`, `stroke_path`, text
  align, rounded rects, gradients, clip).
- **The full Canvas API is already mapped 1:1 to Canvas2D operations.**
  `core/view/src/widget_bridge/canvas2d_api.cpp` + `core/view/js/web-compat-canvas.js`
  already translate every `fill_rect` / `fill_path` / `fill_text` / gradient /
  blend op to HTML5 Canvas2D. That bridge currently runs **JS → C++** (recording
  commands to replay on native Skia). We need the **reverse** (C++ → browser
  canvas), but the hard part — the per-operation mapping — already exists and can
  be mirrored.
- **The SVG backdrop does not need WASM at all.** The single biggest blocker —
  `draw_svg()` is Skia `SkSVGDOM`-only and will not compile under Emscripten —
  simply goes away: the **browser renders the design SVG natively** (inline
  `<svg>`/`<img>` in the DOM), and the WASM canvas draws only the **dynamic
  overlays** on top. A faithful interactive preview of the DDFX editor already
  works this way as a pure static page (see "Prior art" below).

The centerpiece deliverable is a new Canvas backend, `Html5Canvas2DCanvas`,
which is **reusable for every Pulp UI** — that reusability is the whole point.

Honest cost: a focused **2–4 week** effort to get `knob-playground` fully
rendering and interactive in the browser. After the backend + web host land, any
`View` ports with no per-element work.

## Current Repo State (grounded)

- `core/canvas/include/pulp/canvas/canvas.hpp` — `Canvas` abstract interface.
  22 pure virtuals; rich non-pure API (gradients, clip, blend modes, layers,
  text metrics, images).
- Concrete backends today are all native:
  - `core/canvas/.../skia_canvas.*` (Skia/Graphite GPU; the only `draw_svg`).
  - `core/canvas/.../cg_canvas.*` (CoreGraphics, macOS, reduced — no SVG).
  - `RecordingCanvas` (test-only; records `DrawCommand`s).
  - **No HTML5/WebGL/WebGPU backend exists.**
- `core/view/src/widget_bridge/canvas2d_api.cpp` (~1300 lines) +
  `core/view/js/web-compat-canvas.js` — a complete Canvas2D ⇄ Pulp op mapping,
  direction JS → C++. This is the reference for the reverse backend.
- `core/view/include/pulp/view/view.hpp` — `View::paint(Canvas&)` /
  `paint_all()`; `on_mouse_event` + legacy `on_mouse_down/drag/up`; `on_wheel`;
  `on_key_event`; `request_repaint()` routes to a host. The host always supplies
  the Canvas; the View never owns it.
- `core/view/include/pulp/view/design_frame_view.hpp` — renders an SVG via
  `canvas.draw_svg()` (Skia `SkSVGDOM`) and overlays interactive
  `DesignFrameElement`s; patches needle/thumb SVG paths on drag.
- `core/format/.../standalone.hpp` + `core/view/.../window_host.hpp` +
  `platform/mac/window_host_mac.mm` — the native host: NSWindow / CAMetalLayer /
  Metal / `CVDisplayLink` render loop / NSEvent dispatch. This is what a browser
  entry point must **replace**.
- `tools/cmake/PulpWasm.cmake` — builds **DSP/Processor** to WASM only
  (`-sENVIRONMENT=web`, `MODULARIZE`, `EXPORT_ES6`, single-thread, no
  exceptions, optional `USE_WEBGPU`). It does **not** touch the view layer.
- Emscripten support already exists for **audio + MIDI**
  (`core/audio/src/web_audio.cpp`, `core/midi/src/web_midi.cpp`:
  `__EMSCRIPTEN__`, `EMSCRIPTEN_KEEPALIVE`, embind). The view layer has none.

## Architecture — What To Build

### 1. `Html5Canvas2DCanvas : public canvas::Canvas` (the centerpiece)

A new backend under `core/canvas/` that emits to a browser
`CanvasRenderingContext2D`.

- Implement the 22 pure virtuals + `fill_path`/`stroke_path` + text. The op
  mapping is already enumerated in `canvas2d_api.cpp` (mirror it in reverse):
  `set_fill_color` → `ctx.fillStyle`, `fill_rect` → `ctx.fillRect`,
  `stroke_circle` → `ctx.beginPath/arc/stroke`, `fill_path` →
  `beginPath/moveTo/lineTo/fill`, gradients → `createLinearGradient` etc.
- **Performance: do not cross the JS boundary per draw call.** Pulp UIs issue
  hundreds of ops per frame; per-op `emscripten::val` calls would dominate.
  Encode draw calls into a **typed-array command buffer** in WASM and flush it
  **once per frame** to a small JS replayer that walks it against the canvas —
  mirroring the existing `CanvasDrawCmd` recording model. Colors/strings go in a
  side table. This is the make-or-break design choice for 60fps.
- Text: map `fill_text`/`set_font`/`set_text_align`/`measure_text` to
  `ctx.fillText`/`ctx.font`/`ctx.textAlign`/`ctx.measureText`. This drops Skia's
  HarfBuzz/SkParagraph shaping (kerning/bidi/OpenType) — acceptable for the DDFX
  label set; revisit only if a UI needs complex shaping.

### 2. Web host / entry point (replaces `StandaloneApp` + `WindowHost`)

A new `core/format` (or `core/view`) entry, e.g. `web_host.cpp`, behind
`__EMSCRIPTEN__`:

- Create the processor + `create_view()`; own the root `View`.
- A `requestAnimationFrame` loop that calls `view.paint_all(html5_canvas)` when
  dirty (honour `request_repaint()` / `set_continuous_repaint`).
- Marshal DOM events → Pulp: `pointerdown/move/up` → `MouseEvent` (with the
  funnel/dedup the views already expect), `wheel` → `on_wheel`, `keydown` →
  `KeyEvent`. Handle devicePixelRatio and canvas resize.
- A minimal `WindowHost`/`PluginViewHost` shim so `request_repaint()` has a
  back-reference to schedule a frame.

### 3. SVG backdrop in the DOM (sidesteps the Skia blocker)

- For `DesignFrameView`, render the design SVG **in the browser DOM** (inline
  `<svg>` behind a transparent `<canvas>`), not through WASM `draw_svg()`.
- Route `DesignFrameView`'s SVG-needle patching to either (a) canvas-drawn
  overlays (the WASM canvas already draws needles in `view.cpp`), or (b) DOM SVG
  element manipulation from JS. Decide one; the knob-playground already draws
  needles on the Canvas, so (a) is the lower-friction path.

### 4. CMake / Emscripten guards + assets

- Add Emscripten config to compile the **portable** view + canvas + the plugin's
  `view.cpp`, excluding native-only backends (Skia, CoreGraphics, Metal, X11)
  and platform window hosts. Extend `PulpWasm.cmake` or add a sibling
  `PulpWasmUi.cmake`.
- Bundle assets: the design SVG, effect icons, and the **Inter** font
  (`@font-face`) so `ctx.font = "… Inter"` matches native. Use the Emscripten FS
  or `fetch()`; replace any `read_file()` of bundled assets accordingly.

## Blockers & Open Questions

- **`draw_svg` (Skia-only):** resolved by the DOM-backdrop strategy; do not try
  to port `SkSVGDOM`.
- **Per-frame perf:** must use the command-buffer flush, not per-op val calls.
- **Text fidelity:** browser `fillText` ≠ SkParagraph; fine for DDFX, flagged
  for shaping-heavy UIs.
- **Fonts:** must ship Inter via `@font-face` or metrics drift.
- **File I/O:** `read_file()` of SVG/icons → Emscripten FS or fetch.
- **Image pipeline:** `draw_image(void* native_handle, …)` uses backend-specific
  handles (CGImage/SkImage); a web image path (ImageBitmap) is needed only if a
  UI draws raster images. DDFX editor does not, so defer.
- **`DesignFrameView` overlays:** native overlay child widgets (TextEditor,
  ComboBox) need web equivalents or canvas-drawn versions for full parity; the
  first milestone only needs knobs/needles.

## Staged Plan

### Phase W0 — Toolchain + single-knob proof (the spike)
- Get a working Emscripten toolchain (see Environment note).
- Minimal `Html5Canvas2DCanvas` covering only what a knob needs
  (`set_fill_color`, `set_stroke_color`, `set_line_width`, `fill_rect`,
  `stroke_circle`, `fill_path`, `set_font`, `fill_text`).
- Minimal web host: rAF loop + pointer events.
- Render the `KnobUi` (`knob-playground/src/ui.hpp`) — one draggable knob —
  drawn by the real C++ `paint()` in the browser.
- **Exit:** one knob renders and drags in a browser, painted by C++→WASM, value
  echoed. Proves backend + host + event marshaling end-to-end.

### Phase W1 — Full Canvas2D backend coverage
- Implement remaining ops: rounded rects, gradients (linear/radial/conic), clip,
  blend modes, opacity/layers, transforms, text align/baseline, dashes.
- Switch to the command-buffer flush model; benchmark a full-editor frame.

### Phase W2 — Web host completeness
- devicePixelRatio, resize, wheel, keyboard, hover; dirty-rect / continuous
  repaint honouring `request_repaint()`.

### Phase W3 — DesignFrameView web mode
- SVG-as-DOM backdrop + overlay routing; asset bundling (SVG, icons, Inter).

### Phase W4 — Full editor + generalize
- `knob-playground` full editor (dropdowns, FX menus, reorder) in the browser.
- Promote to a reusable path: a `pulp_add_web_ui()` helper + docs so any plugin
  opts in, and an example under `examples/`.

## Effort & Payoff

- ~2–4 weeks to W4 for the DDFX editor.
- The backend (W1) + host (W2) are **reusable infrastructure**: once they exist,
  any Pulp `View` renders on the web with no per-element work. That is the
  strategic return — a web "interactive manual" that tracks native UI changes
  automatically.

## Environment Note (current blocker for W0)

`emsdk install latest` (and pinned `3.1.74`) currently fail in this environment
with `error: tool or SDK not found: 'sdk-releases-<hash>-64bit'` on arm64 — the
prebuilt SDK packages are not resolving/downloading (network/sandbox
limitation). A working Emscripten toolchain is the prerequisite for W0; resolve
the install (or build emsdk from source / use a provisioned CI image) before the
code spike.

## Prior Art In This Tree

A pure-static, no-WASM interactive preview of the DDFX editor already exists and
validates the DOM-SVG strategy and the look: the exported `ddfx-editor.svg`
rendered in the browser with the 8 macro-knob needles wired to drag (needle
rotation around each disc centre = `value × 270°`). That is the "frozen
snapshot" tier; this plan is the "live, tracks-your-C++" tier built on the same
DOM-SVG-backdrop idea.

## Files To Touch / Inspect First

- `core/canvas/include/pulp/canvas/canvas.hpp` — interface to implement.
- `core/view/src/widget_bridge/canvas2d_api.cpp` — the op mapping to mirror.
- `core/view/js/web-compat-canvas.js` — the JS Canvas2D vocabulary.
- `core/view/include/pulp/view/view.hpp` — paint/event contract.
- `core/view/include/pulp/view/design_frame_view.hpp` — SVG + overlays.
- `core/format/include/pulp/format/standalone.hpp`,
  `core/view/include/pulp/view/window_host.hpp`,
  `core/view/platform/mac/window_host_mac.mm` — the native host to replace.
- `tools/cmake/PulpWasm.cmake` — extend for a UI WASM target.
- `core/audio/src/web_audio.cpp`, `core/midi/src/web_midi.cpp` — existing
  Emscripten patterns to follow.
- `knob-playground/src/ui.hpp` (single knob) and `knob-playground/src/view.cpp`
  (full editor) — the W0 and W4 targets.
