# Design Tool — Remaining TODO

**Date:** 2026-03-29

## Priority 1: Visual Polish (Fix what exists)

- [ ] **Input field corners** — TextEditor focus border has pixelated/rough corners. Need to check border-radius rendering in Skia canvas vs CoreGraphics path. May need anti-aliased rounded rect stroke.
- [ ] **Icon quality** — Send button icon is squished. Image upload icon looks rough. Need proper SVG/vector icon rendering or higher-resolution icon assets.
- [ ] **Gamut triangle edges** — Still pixelated from rect grid. Need ImageData-equivalent bridge OR higher-resolution grid (200x100+) with proper anti-aliasing.
- [ ] **State pills actually change preview** — Pills exist but only update status text. Need to apply hover/focus/disabled/error overrides to all preview components.
- [ ] **Chat context badge** — "Editing: All Components" needs accent background styling + visible clear (x) button when component is selected.
- [ ] **Scrollbar overlap** — Scrollbars in center and right panels can overlap content. Need padding-right on scroll content.

## Priority 2: Functional Gaps vs HTML

- [ ] **Generate Opposite Mode** — Button below palette rows that auto-generates light from dark (or vice versa)
- [ ] **Palette Save/Load** — Download/upload .colorsystem.json for palette configurations
- [ ] **Template selector** — "Audio Studio" / "Tailwind 4" dropdown for starting palettes
- [ ] **Contrast badges (AAA/AA)** — Show WCAG contrast ratios on shade swatches
- [ ] **"Aa" contrast preview button** — Info modal showing color on white/black backgrounds
- [ ] **"?" info buttons** — Help modals for Template, Harmony, Mode explanations
- [ ] **Token alpha channel** — UI for setting per-token opacity (0-1)
- [ ] **Export: C++ Header format** — VISAGE_THEME_COLOR macros
- [ ] **Export: C++ Palette format** — palette.setColor() calls
- [ ] **Chat export** — Export conversation history
- [ ] **Chat typing indicator** — Animated 3-dot bounce during Claude response

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
