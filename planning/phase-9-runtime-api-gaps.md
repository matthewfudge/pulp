# Phase 9 — Runtime API Gaps (Beyond W3C CSS/DOM)

**Version:** 2026-03-28
**Status:** Proposed — bugs filed
**Goal:** Close every runtime API gap a frontend developer hits when bringing a Figma design to Pulp.

---

## Why This Exists

Phases 5-8 closed W3C CSS/DOM/Events gaps systematically. But a real Figma-to-code workflow also needs **runtime APIs** that aren't W3C specs — clipboard, canvas gradients, image loading, timers, storage, drag-and-drop. These come from the HTML Living Standard, Web APIs, and platform conventions.

This was identified when implementing a real design: 4 C++ bridge functions were missing (gradient fill, drag-to-JS, async exec, clipboard) despite the W3C audit saying "complete."

**Root cause:** W3C CSS/DOM specs don't cover runtime APIs. We need a second checklist: the **Web API surface** that frontend devs actually call.

---

## Critical (P0) — Broken Right Now

| # | Bug | Impact | Fix |
|---|-----|--------|-----|
| 1 | **`__requestFrame__` never registered** — requestAnimationFrame, setTimeout, setInterval all silently return 0 and callbacks never fire | Every animation loop, every timer, every debounce is dead | Register `__requestFrame__` in widget_bridge via FrameClock::on_frame callback |
| 2 | **`performance.now()` absent** | Can't compute frame deltas, can't measure timing | Bridge: return std::chrono::steady_clock high-res time |

## High (P1) — Blocks Common Design Patterns

| # | Bug | Impact | Fix |
|---|-----|--------|-----|
| 3 | **Clipboard not exposed to JS** — navigator.clipboard.readText/writeText absent despite C++ Clipboard class existing | Can't copy/paste from JS | Bridge: readClipboard/writeClipboard wrapping platform::Clipboard |
| 4 | **Canvas gradient fills absent** — no createLinearGradient/createRadialGradient on canvas | Can't draw gradient shapes in canvas | Add CanvasDrawCmd::set_gradient + JS canvasCreateLinearGradient/canvasCreateRadialGradient |
| 5 | **Canvas drawImage() absent** | Can't render images inside canvas drawing | Add CanvasDrawCmd::draw_image + JS canvasDrawImage |
| 6 | **Canvas arc()/ellipse() absent** | Can't draw arcs, pie charts, circular progress indicators | Add CanvasDrawCmd::arc + JS canvasArc/canvasEllipse |
| 7 | **Canvas textAlign/textBaseline hardcoded to left** | Text centering on canvas broken | Add textAlign/textBaseline fields to CanvasDrawCmd, wire in canvasFillText |
| 8 | **Canvas clearRect() absent** | Can't partially repaint a canvas region | Add CanvasDrawCmd::clear_rect + JS canvasClearRect |
| 9 | **Canvas clip() (path-based) absent** — only clipRect exists but isn't registered | Can't clip to non-rectangular regions | Register canvasClipRect, add path-based canvasClip |
| 10 | **Drag-and-drop events not bridged to JS** — C++ DropTarget exists but JS gets no events | File drops and drag gestures impossible from JS | Wire DropTarget callbacks to JS __dispatch__ |
| 11 | **`new Image()` constructor absent** | Standard image preloading pattern broken | JS: Image class wrapping createImage + onload/onerror |
| 12 | **Image onload/onerror callbacks absent** | Can't sequence rendering around image loading | Bridge: fire callback when image decode completes |

## Medium (P2) — Quality of Life

| # | Bug | Impact | Fix |
|---|-----|--------|-----|
| 13 | **localStorage absent** | Can't persist preferences between sessions | Bridge: readFile/writeFile to plugin data dir, or key-value store |
| 14 | **window.onerror absent** | Global error handler missing | JS: add onerror property to window |
| 15 | **console.time/timeEnd absent** | Performance profiling from JS missing | JS: implement using performance.now() |
| 16 | **FontFace / document.fonts absent** | Can't load custom fonts from JS | Bridge: loadFont(path) + callback |
| 17 | **Canvas fillRoundedRect/strokeRoundedRect/strokeCircle not registered** — C++ types exist but no JS bridge | Can't call these canvas operations from JS | Register canvasFillRoundedRect, canvasStrokeRoundedRect, canvasStrokeCircle |
| 18 | **Canvas globalAlpha absent** | Can't set canvas-level opacity | Add alpha state to canvas command list |
| 19 | **Data URL support for images absent** | Can't use inline base64 images | Parse data: URLs in setImageSource |
| 20 | **measureText returns approximations** | Precision text layout fails | Wire to Skia text shaper when available |
| 21 | **Microtask queue pump unconfirmed** | async/await and .then() chains may silently stall | Verify JS_ExecutePendingJob is called after evaluate() |

## Low (P3) — Niche/Advanced

| # | Bug | Impact | Fix |
|---|-----|--------|-----|
| 22 | **fetch() / XMLHttpRequest absent** | Can't query license servers or remote APIs | Bridge: httpGet/httpPost via platform HTTP client |
| 23 | **Canvas getImageData/putImageData absent** | Can't do pixel manipulation | Structurally hard with command-list architecture |
| 24 | **Canvas globalCompositeOperation absent** | Can't do blending modes on canvas | Skia blend modes needed |
| 25 | **Canvas createPattern() absent** | Can't tile images on canvas | Skia shader pattern needed |
| 26 | **console.table() absent** | Debugging convenience | JS: format as aligned columns |

---

## Implementation Plan

### Phase 9.1 — Fix What's Broken (P0, ~50 lines)
1. Register `__requestFrame__` in widget_bridge.cpp via FrameClock callback
2. Register `performance.now()` via std::chrono::steady_clock

### Phase 9.2 — Canvas Completions (P1 items 4-9, ~200 lines)
1. Canvas gradient state (linear + radial)
2. drawImage command
3. arc/ellipse path commands
4. textAlign/textBaseline on fillText
5. clearRect command
6. clip() path and register clipRect

### Phase 9.3 — Platform APIs (P1 items 3, 10-12, ~150 lines)
1. Clipboard bridge (readClipboard, writeClipboard)
2. Drag-and-drop event bridging
3. Image constructor + onload/onerror

### Phase 9.4 — Quality of Life (P2, ~200 lines)
1. localStorage equivalent (file-based key-value)
2. window.onerror, console.time/timeEnd
3. Missing canvas registrations (rounded rect, stroke circle)
4. Canvas globalAlpha
5. Data URL support

---

## How We Got Here (Avoiding Future Surprises)

The piecemeal discovery problem happened because we audited against W3C specs but missed the **Web API surface**. For future completeness, check against:

1. **W3C CSS specs** (16 specs) — ✅ Done in Phase 8
2. **DOM Living Standard** — ✅ Done in Phases 5-8
3. **HTML Living Standard: Canvas 2D Context** — ⚠️ This phase
4. **Clipboard API** — ⚠️ This phase
5. **Drag and Drop API** — ⚠️ This phase
6. **Web Animations API** — Partial (FrameClock covers it differently)
7. **Intersection/Resize/MutationObserver** — Planned
8. **Storage API** — ⚠️ This phase
9. **High Resolution Time** — ⚠️ This phase (performance.now)
10. **Console Standard** — ⚠️ This phase

The definitive checklist is now: W3C CSS (16 specs) + Web APIs (10 specs) = 26 specs total. docs/reference/w3c-coverage.md should be expanded to cover all 26.
