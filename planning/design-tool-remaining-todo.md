# Design Tool — Remaining TODO

**Date:** 2026-03-31

## Burn-Down Order

### PR Prep Only

- [ ] **Windows test asset discovery tightening** — Only address this when the design-tool branch is otherwise ready for PR/merge. The current soft-skip path in `test/test_design_tool_layout.cpp` is intentional: tests call `SKIP("design-tool.js not found")` when `examples/design-tool/design-tool.js` cannot be located. Before PR, tighten Windows asset discovery/reporting so that this bucket of tests either runs or skips with a more explicit reason, but do not change that behavior during active feature work.

### Now

- [ ] **Hue slider responsiveness** — Master Hue / H drag path now has cache/throttle passes plus redundant live-ramp repaint suppression, but still needs real runtime feel verification in the native app.
- [ ] **Preview parity cleanup** — Cards, overlays, tabs, waveform/spectrum token mapping, the compact preview dropdown shell, effect swatches, and uppercase section-header treatment are now aligned. Remaining work is the last HTML-reference cleanup on spacing/details.
- [x] **Token popup reliability** — Close button, Escape dismissal, expanded custom-picker positioning, on-screen fit, pointer-driven custom gamut selection, stronger selection marker, denser gamut edge rendering, throttled H/C/L slider previews, and taller popup slider geometry now behave consistently.
- [x] **Modal dismissal parity** — Token, help, contrast, and export popups now close via Escape and outside click with regression coverage.
- [x] **Opposite-mode parity** — Generate Opposite Mode, preset selection, and palette mode selection now share the same light/dark state, and mode changes now reuse saved generated variants instead of blindly re-deriving them.
- [x] **Text-input visibility** — Chat, token search, and sample inputs now set explicit editor foreground colors and have regression coverage.

### Next

- [x] **Palette Save/Load** — Save/load palette configurations through the native dialog path where available, with fallback file handling elsewhere.
- [x] **Token alpha persistence** — Popup alpha now writes real token values and survives palette picks/reopen with regression coverage.
- [x] **Token override polish** — Inline modified markers and reset affordances now surface overrides directly in the token list.
- [x] **Chat UX** — Timeout recovery, per-request callback isolation, fast CLI/provider failure handling, explicit cancel/stop affordance, and Escape cancellation are now covered with regression tests.
- [x] **Template selector** — "Audio Studio" / "Tailwind 4" now apply real starting palettes with tests.
- [x] **Contrast tooling** — Shade badges plus an `"Aa"` contrast preview modal now exist in the expanded palette editor.

### Later

- [ ] **Export parity** — W3C Design Tokens and style-preset JSON exports now exist. Remaining work is final shortcut/platform polish.
- [ ] **Full token registry expansion** — Move toward the HTML tool’s broader token taxonomy
- [ ] **AI style-system expansion** — Richer widget-style generation and shader/recipe authoring flow
- [ ] **Hot reload / developer experience** — More reliable rebuilds and authoring ergonomics

## Priority 1: Visual Polish (Fix what exists)

- [ ] **Input field corners** — TextEditor now renders a filled border shell plus inset fill instead of a stroked rounded rect. Needs visual verification/tuning if corners still look rough on-device.
- [ ] **Icon quality** — Send/upload icon geometry was redrawn in the native widget renderer; keep open only for final on-device visual tuning against the HTML reference.
- [x] **Gamut triangle edges** — Popup gamut renderer now uses a higher-resolution strip grid; bridge-level ImageData would still be a future quality upgrade.
- [x] **State pills actually change preview** — Pills now drive preview state rendering with regression coverage.
- [x] **Chat context badge** — Accent styling and clear affordance are in place with regression coverage.
- [x] **Scrollbar overlap** — Center/right scroll content now keeps clearance from the scrollbar with regression coverage.

## Priority 2: Functional Gaps vs HTML

- [x] **Generate Opposite Mode** — Button below palette rows now keeps mode/preset state in sync.
- [x] **Palette Save/Load** — Palette config can now serialize/round-trip with regression coverage; dialog-backed on hosts that support it.
- [x] **Template selector** — "Audio Studio" / "Tailwind 4" dropdown now applies real starting palettes.
- [x] **Contrast badges (AAA/AA)** — Shade swatches now show direct WCAG contrast ratios.
- [x] **"Aa" contrast preview button** — Info modal now shows color on white/black backgrounds.
- [x] **"?" info buttons** — Template, Harmony, and Mode help modals are implemented and tested.
- [x] **Token alpha channel** — Popup alpha slider now persists real token alpha values.
- [x] **Export: CSS Vars format** — Export helper emits `:root { --pulp-... }` output in the modal.
- [x] **Export: OKLCH format** — Export helper emits `oklch(...)` CSS variables in the modal.
- [x] **Export: C++ Header format** — Export helper now emits `PULP_THEME_COLOR(...)` output and saves with `.hpp`.
- [x] **Export: C++ Palette format** — Export helper now emits `palette.setColor(...)` output and saves with `.cpp`.
- [x] **Chat export** — Conversation history now serializes and exports through the save-dialog path.
- [x] **Chat typing indicator** — Pending requests now use transient UI instead of leaving `"..."` in history.
- [x] **Preview cards / overlays / tabs track theme tokens** — Ready/error cards now use semantic borders, overlay surfaces honor overlay/tooltip/modal tokens, and preview tabs have active/hover treatment with regression coverage.

## Priority 3: AI-Driven Style System (THE BIG ONE)

### Vision
The user says "analog Moog knob" or uploads a photo of a Buchla knob, and Claude returns a **GPU shader or drawing recipe** that renders that knob style in the preview. Not just colors — actual visual designs.

### Architecture Required

**Current state:** Knobs are drawn via canvas arc commands (simple arc + thumb dot). There is no "knob style" system — the shape is hardcoded in C++ `Knob::paint()`.

**HTML reference has 4 knob styles:** Default (arc), Filled (pie wedge), Notched (tick marks), Glossy (radial gradient). These are selected via a `--st-knob-style` CSS variable.

**What we need for AI-generated styles:**

1. **SkSL Shader System** — Pulp already has `compileShader()` bridge and Skia Graphite rendering. We need:
   - A `setWidgetShader(widgetId, skslCode)` bridge function that compiles SkSL and applies it to a widget's paint
   - The shader receives uniforms: `value` (0-1), `size`, `time`, `accentColor`, `backgroundColor`
   - Claude generates SkSL code based on the user's description or reference image

2. **Canvas 2D Recipe System** — Alternative to shaders:
   - Claude generates a JSON "draw recipe" — an array of canvas commands (arcs, gradients, fills, strokes)
   - A `setWidgetRecipe(widgetId, recipeJSON)` bridge function that replaces the widget's paint with the recipe
   - Recipes are parameterized by `value`, `size`, colors

3. **AI Pipeline:**
   - User describes style or uploads reference image
   - Prompt includes: current widget type, available drawing primitives, size constraints
   - Claude returns either SkSL shader code OR canvas recipe JSON
   - Bridge compiles/applies, preview updates immediately
   - If it looks wrong, user refines via chat

4. **Style Presets:**
   - Built-in presets: "Minimal Arc", "Filled Pie", "Notched Ticks", "Glossy 3D", "LED Ring"
   - User can save custom styles
   - Styles are parameterized and recolorable

### Files Involved
- `core/view/src/widgets.cpp` — Knob::paint(), Fader::paint() need shader/recipe override
- `core/view/src/widget_bridge.cpp` — New bridge: setWidgetShader, setWidgetRecipe
- `core/canvas/src/skia_canvas.cpp` — SkRuntimeEffect integration (already has waveform shader)
- `examples/design-tool/design-tool.js` — Chat prompt engineering for style generation

### Feasibility
- SkSL shaders: **HIGH** — Pulp already uses SkRuntimeEffect for waveform rendering
- Canvas recipes: **MEDIUM** — CanvasWidget already has 25+ draw commands
- AI generation: **HIGH** — Claude can generate SkSL/GLSL from descriptions
- Image → style: **MEDIUM** — Claude can analyze reference images and generate approximate shaders

## Priority 4: Hot Reload & Developer Experience

- [ ] **Hot reload reliability** — Currently breaks when widget tree changes. Need clean teardown/rebuild.
- [ ] **Xcode project generation** — Offer `cmake -G Xcode` as option for IDE debugging
- [ ] **`__requestFrame__` proper implementation** — Currently only flushes on load_script, not continuous
