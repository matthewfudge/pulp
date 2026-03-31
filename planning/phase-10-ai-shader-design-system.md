# Phase 10 — AI-Driven GPU Shader Design System

**Version:** 2026-03-29
**Status:** Planning
**Branch:** explore/shader-styles
**Goal:** Users describe visual styles via chat or inspector → Claude generates GPU shader code → real-time preview updates.

---

## Vision

A frontend developer or designer should be able to:

1. **Chat**: "analog Moog knob with brushed metal and LED ring" → preview updates
2. **Select + Modify**: Cmd+click a knob → "make this blue" → only that knob changes
3. **Multi-select**: Cmd+Shift+click or rubber-band → "warmer colors" → all selected change
4. **Upload Reference**: Drop a photo of a Buchla panel → Claude generates matching shader
5. **Save/Load**: Named style presets that can be applied across widget types
6. **Fallback**: If SkSL fails, auto-fall back to canvas 2D draw commands

---

## Architecture

### Layer 1: RuntimeEffectCache + CanvasWidget Shader Path (C++ core)

**Key architectural decisions (from Codex review):**

1. **Cache lives at renderer lifetime, not canvas** — SkiaSurface creates a new SkiaCanvas every frame, so a cache on SkiaCanvas dies every frame. The `RuntimeEffectCache` is a standalone class owned by the render context or WidgetBridge.

2. **Shader support on CanvasWidget first, not abstract Canvas** — The backend-neutral Canvas should not expose SkSL (Skia-specific). Instead, CanvasWidget gets a shader overlay mode. This lets us validate chat→compile→preview without touching the Canvas abstraction.

3. **Shader replaces the body/track/fill layer only, not the entire paint** — Knob::paint() also draws labels, value text, hover glow, thumb. A full early-return would lose those. Shader renders the knob body, then C++ draws labels/overlays on top.

4. **Use `layout(color)` in uniform declarations** — Raw SkV4 colors bypass Skia color management. Color uniforms use the `layout(color)` qualifier.

5. **Wire `ContextOptions::fShaderErrorHandler`** — SkSL compile success ≠ backend success. Graphite can fail during shader translation even if SkRuntimeEffect reports success.

```cpp
// Renderer-owned cache (lives for process lifetime)
class RuntimeEffectCache {
    std::unordered_map<size_t, sk_sp<SkRuntimeEffect>> effects_;
public:
    sk_sp<SkRuntimeEffect> get_or_compile(const std::string& sksl);
};
```

**ShaderUniforms struct:**
```cpp
struct ShaderUniforms {
    float value = 0;           // widget value (0-1)
    float time = 0;            // animation time in seconds
    Color accent_color;        // layout(color), from theme accent.primary
    Color bg_color;            // layout(color), from theme bg.primary
    Color track_color;         // layout(color), from theme control.track
    Color fill_color;          // layout(color), from theme control.fill
    Color thumb_color;         // layout(color), from theme control.thumb
};
// resolution derived from widget bounds (float2), not a separate scalar
```

**SkSL uniform contract:**
```glsl
uniform float2 resolution;           // widget size in pixels
uniform float value;                  // 0-1
uniform float time;                   // seconds (requires FrameClock subscription)
layout(color) uniform float4 accentColor;
layout(color) uniform float4 bgColor;
layout(color) uniform float4 trackColor;
layout(color) uniform float4 fillColor;
layout(color) uniform float4 thumbColor;
// Optional child shader:
uniform shader inputTexture;          // for reference image sampling

half4 main(float2 coord) { ... }
```

### Layer 2: Widget Shader Storage

```cpp
// In widgets.hpp
class Knob : public View {
    // ... existing members ...
    std::string custom_sksl_;          // SkSL source code (empty = use default paint)
    std::string canvas_2d_recipe_;     // JSON draw command fallback
};
```

In `Knob::paint()`:
```cpp
if (!custom_sksl_.empty()) {
    ShaderUniforms u;
    u.value = value_;
    u.size = std::min(bounds().width, bounds().height);
    u.time = frame_clock ? frame_clock->elapsed_seconds() : 0;
    u.accent_color = resolve_color("accent.primary", {});
    // ...
    canvas.draw_with_sksl(custom_sksl_, 0, 0, bounds().width, bounds().height, u);
    return; // skip default paint
}
// ... existing arc drawing ...
```

### Layer 3: JS Bridge

```js
// Set GPU shader on a widget (replaces paint with SkSL)
setWidgetShader(widgetId, skslCode)

// Set canvas 2D fallback recipe (JSON array of draw commands)
setWidgetRecipe(widgetId, recipeJSON)

// Compile and validate SkSL without applying
compileShader(skslCode)  // returns { success: bool, error: string }

// Clear custom shader (restore default paint)
clearWidgetShader(widgetId)

// Save/load style presets
saveStylePreset(name, { shader: skslCode, recipe: recipeJSON, tokens: {} })
loadStylePreset(name)
```

### Layer 4: Inspector-Driven Selection

**Current:** Cmd+click selects one widget for scoped style changes.

**Enhanced:**
- Cmd+click — toggle select single widget (existing)
- Cmd+Shift+click — add to selection (multi-select)
- Rubber-band drag — select all widgets in rectangular area (future)
- Escape — clear selection
- Click on empty space — clear selection

**Selection state visible to chat:**
```js
var selectedWidgets = [];  // array of widget IDs
// Chat prompt includes: "Selected: knob-1, fader-2 (Knob, Fader)"
// Claude scopes its response to the selected widgets
```

**Apply modes:**
- 0 selected → apply to all widgets of the affected type
- 1 selected → apply to just that widget (most common)
- N selected → apply to all N selected widgets

### Layer 5: Chat Prompt Engineering

**System prompt template:**
```
You are an SkSL shader designer for Pulp audio plugin knobs and faders.

Generate SkSL code that renders the requested visual style. The shader
runs per-pixel in a fragment shader context.

Standard uniforms available:
  uniform float2 resolution;   // widget size in pixels
  uniform float value;          // widget value 0-1
  uniform float size;           // widget diameter in pixels
  uniform float time;           // animation time in seconds
  uniform float4 accentColor;   // theme accent (r,g,b,a normalized)
  uniform float4 bgColor;       // theme background
  uniform float4 trackColor;    // control track
  uniform float4 fillColor;     // control fill
  uniform float4 thumbColor;    // control thumb

Output ONLY valid SkSL shader code between ```sksl and ``` markers.
The main function must be:
  half4 main(float2 coord) { ... }

For knobs: draw a circular control showing `value` as an arc, angle, or
fill level. Use `resolution` for sizing.

If you cannot generate SkSL, output a JSON fallback between ```json and
``` markers: an array of canvas 2D draw commands.
```

### Layer 6: Reference Image Pipeline

When user uploads/drops a photo:
1. Image saved to temp dir
2. Image loaded as SkImage via `SkData::MakeFromFileName` + `SkImages::DeferredFromEncodedData`
3. Passed to SkRuntimeShaderBuilder as `builder.child("inputTexture")`
4. Chat prompt updated: "User uploaded reference image at path X. Generate shader that approximates this visual style."
5. Claude generates SkSL that may sample `inputTexture` for color reference

---

## Three Rendering Strategies

The system supports three complementary strategies for AI-generated widget styles:

### Strategy 1: SkSL GPU Shaders
- **Input:** SkSL fragment shader code
- **Runtime:** Dawn/Skia SkRuntimeEffect
- **AI generates:** Procedural textures, gradients, glow effects, metallic surfaces
- **Best for:** Visual fidelity, GPU effects that can't be drawn with geometry
- **Limitation:** Claude generates good procedural shaders but pixel-level accuracy is hard

### Strategy 2: Canvas 2D Draw Recipes
- **Input:** JSON array of draw commands
- **Runtime:** QuickJS → Canvas API replay
- **AI generates:** Arcs, circles, rounded rects, lines, text — geometric shapes
- **Best for:** Clean geometric styles (arc knobs, tick marks, simple meters)
- **Limitation:** No GPU effects, no animated state transitions

### Strategy 3: Declarative Widget Schema (Rive-inspired)
- **Input:** Structured JSON describing geometry + value mapping + interaction states
- **Runtime:** Custom schema renderer (not Rive runtime — we keep our renderer)
- **AI generates:** Complete widget definition as structured data
- **Best for:** State-machine-driven widgets (idle→hover→active), scaling stylistic variation without code branches

**Why this matters:** Rive's insight is that **styles should be data, not code branches**. Rather than growing `if (style == "glossy")` chains in paint(), a declarative schema means:
```json
{
  "knob": {
    "track": { "type": "arc", "radius": "90%", "width": 3, "color": "control.track" },
    "value": { "type": "arc", "radius": "90%", "width": 3, "color": "control.fill",
               "startAngle": -135, "endAngle": { "map": "value", "range": [-135, 135] } },
    "thumb": { "type": "line", "from": "center", "to": "value_angle", "color": "control.thumb" },
    "states": {
      "hover": { "glow": { "radius": 4, "color": "accent.primary", "opacity": 0.3 } },
      "active": { "thumb.width": 3 }
    }
  }
}
```

Claude excels at generating this kind of structured data. The renderer interprets it without branching.

### Evaluation: Rive Runtime (MIT)

[rive-runtime](https://github.com/rive-app/rive-runtime) provides a real-time 2D animation engine with state machines. Key analysis:

**What Rive adds over our approach:**
- Mature state machine system (inputs, transitions, blend states)
- Constraint system for responsive layouts
- Binary format optimized for size and parse speed
- Community of existing animations

**What Rive CAN'T do that we need:**
- No Skia Graphite / Dawn backend (would need a parallel GPU context)
- No native integration with our theme token system
- Build system is premake, not CMake (friction)
- ~400KB-1.2MB binary overhead per platform

**Can AI generate Rive content?** YES — at the structural level:
- Vector shapes, paths, fills, strokes → structured primitives
- Timelines + keyframes → straightforward for LLMs
- State machines (hover, pressed, toggled) → well-structured data
- Value mapping (0-1 → rotation/arc) → simple math Claude handles well

**Recommendation:** Don't pull in the Rive runtime, but **adopt Rive's declarative model as a JSON schema** that our own renderer interprets. This gives us:
- Rive's "styles as data" philosophy
- Our existing Skia/Dawn renderer (no second GPU context)
- Theme token integration
- AI-generatable structured output

This is the Strategy 3 approach above. We can always add actual Rive runtime support later if the community .riv file ecosystem becomes compelling.

## Additional Research Findings (2026-03-29)

### Yoga Layout Engine (MIT) — ADOPT
Meta's C++20 flexbox/grid layout engine. Replaces our hand-rolled layout with:
- Correct flex-wrap, margin:auto centering, absolute positioning
- min-content / max-content / fit-content sizing keywords
- box-sizing: border-box
- Battle-tested by React Native (millions of devices)
- Clean C API, CMake-ready, ~35K lines

**Integration:** FetchContent dep, replace FlexStyle/GridStyle with Yoga nodes, replace layout_children() with YGNodeCalculateLayout(). Closes ALL Phase 8 Tier 3 layout bugs.

### Lottie JSON + Skottie — CONSIDER for animations
Lottie's JSON keyframe format is AI-generatable (mechanical structure). Skia has Skottie (native Lottie player) already in our dep tree. Claude can generate knob rotation/hover animations as Lottie JSON. Consider as Strategy 4 alongside SkSL/Canvas2D/Schema.

### Unity UI Toolkit Patterns — STUDY
Three patterns to adopt:
1. **UxmlTraits** — typed attribute declarations for widget schema
2. **CustomStyleProperty → render bridge** — CSS custom properties flow into draw code
3. **BaseField + SetValueWithoutNotify + MarkDirtyRepaint** — clean binding/dirty cycle

### shader-starter-kit Patterns — STUDY
WebGL lifecycle class with reactive uniforms. `WebGLManager` base class (onInitialize/onUpdate/onRender/onDispose) maps directly to our shader widget model. Shadow raymarching and rainbow gradient shaders are reference implementations.

### VCV Rack (GPL, study only)
- FramebufferWidget dirty/cache pattern for GPU widget caching
- Knob behavioral params: minAngle, maxAngle, smooth, snap, speed, forceLinear

### CSS Houdini — VALIDATES our approach
Paint Worklets = "register a paint callback that draws custom content" — this is exactly what our SkSL shader widget system does. The W3C validated this pattern. Properties and Values API = typed custom properties — matches our theme token system.

### Web Animations API — ADOPT API shape
`element.animate(keyframes, options)` is a better API than our current `animate()` bridge:
```js
element.animate([
  { transform: 'rotate(0deg)' },
  { transform: 'rotate(270deg)' }
], { duration: 300, easing: 'ease-out', fill: 'forwards' });
```
Implement over FrameClock. Returns Animation object with play/pause/cancel/finish.

### W3C Design Tokens Spec — CONSIDER for interop
Community spec format: `{ "$value": "#ff0000", "$type": "color", "$description": "..." }`. Adopting this for Theme export would enable Figma/design tool interoperability.

### Claude Integration — KEEP CLI, add streaming
- Agent SDK is TypeScript/Python only (no C/C++)
- Max subscription works with CLI (`claude --print`)
- Best path: `claude --print --output-format stream-json` for incremental token display
- Alternative: Direct Anthropic HTTP API via libcurl for true async
- Current `exec()` blocking is the #1 UX issue — switch to `execAsync()`

### CHOC Modules to Adopt

From the CHOC audit, adopt for the design tool:
- **choc::threading::TaskThread** — background async operations (exec, file I/O)
- **choc::threading::SpinLock** — render thread synchronization
- **choc::network::HTTPServer** — future: serve design tool as local web server

---

## Phased Implementation

### Phase 10.1 — Shader Engine (C++ core, ~250 lines)
**Goal:** SkSL shaders render on any widget via `draw_with_sksl()`

1. Add `ShaderUniforms` struct to canvas.hpp
2. Add `virtual void draw_with_sksl(sksl, x, y, w, h, uniforms)` to Canvas (no-op default)
3. Implement in SkiaCanvas: compile-and-cache by source hash, bind standard uniforms, drawRect
4. Wire existing `compileShader()` bridge to actually call `SkRuntimeEffect::MakeForShader`
5. Add `custom_sksl_` and `widget_schema_` (JSON) members to Knob, Fader, Toggle
6. Early-return in paint(): if custom_sksl_ set → draw_with_sksl; if widget_schema_ set → interpret schema
7. **Test:** Hardcoded SkSL arc shader renders on a Knob via XcodeBuildMCP screenshot

### Phase 10.2 — Declarative Widget Schema (JS + C++, ~300 lines)
**Goal:** Rive-inspired JSON schema that the renderer interprets without code branches

1. Define schema format: geometry (arc, rect, circle, line, path), value mapping, color tokens, states
2. Schema interpreter in widgets.cpp: read JSON → issue canvas draw calls
3. State machine: map widget state (idle/hover/active/disabled) to visual properties
4. Register `setWidgetSchema(id, schemaJSON)` bridge function
5. Built-in schemas: "arc" (default), "filled-pie", "notched-ticks", "glossy-3d"
6. **Test:** Schema-driven knob matches pixel-for-pixel with hardcoded Knob::paint()

### Phase 10.3 — Inspector Multi-Select + Chat Bridge (JS, ~250 lines)
**Goal:** Select one or many widgets, describe changes, see them applied in real-time

1. Enhance inspector selection:
   - Cmd+click: toggle select single widget (existing, harden)
   - Cmd+Shift+click: add/remove from multi-selection
   - Escape: clear all selection
   - Click empty space: clear all selection
   - (Future: rubber-band rectangle selection, Cmd+click to deselect one from group)
2. Selection state array: `selectedWidgets[]` with widget IDs and types
3. Chat prompt includes selection context: "Selected: knob-1 (Knob), fader-2 (Fader)"
4. Apply modes:
   - 0 selected → apply to ALL widgets of the affected type
   - 1 selected → apply to just that widget
   - N selected → apply to all N selected
5. Color-only changes → use applyTokenDiff (existing, fast)
6. Style changes → generate SkSL or schema depending on complexity
7. **Test:** Select 3 knobs → "make these red" → only those 3 change

### Phase 10.4 — Chat Prompt Engineering (JS, ~200 lines)
**Goal:** Claude generates valid SkSL or widget schemas from natural language

1. Three-tier system prompt:
   - **Color/token changes:** Claude returns JSON diff (existing pipeline)
   - **Geometric style changes:** Claude returns widget schema JSON
   - **GPU effects (glow, metal, texture):** Claude returns SkSL code
2. Parse response: detect ```sksl```, ```json```, or plain JSON diff
3. Compile SkSL → if success, apply via setWidgetShader
4. If SkSL fails, try schema → if success, apply via setWidgetSchema
5. If both fail, fall back to token diff (color-only)
6. Error display in chat: show compile errors, offer retry
7. Switch to execAsync for non-blocking chat
8. **Test:** "analog Moog knob" → valid SkSL or schema applied

### Phase 10.5 — Presets + Reference Images + Polish (~150 lines)
**Goal:** Save, load, and share named styles

1. Save/load named presets: { shader?, schema?, recipe?, tokens? }
2. Built-in presets: "default-arc", "filled-pie", "notched-ticks", "glossy-3d", "led-ring", "moog-analog"
3. Reference image pipeline: drop photo → save to temp → inject into chat prompt
4. Undo/redo for shader/schema changes (snapshot stack)
5. Apply preset across widget types: "apply Moog style to all knobs and faders"
6. **Test:** Save custom style → reload → matches original

---

## Testing Strategy

- **Unit tests:** SkSL compilation success/failure, uniform binding, shader cache
- **Headless screenshots:** Render knob with custom shader → compare PNG
- **XcodeBuildMCP:** Build and run in macOS, verify GPU rendering path works
- **Integration test:** Chat prompt → SkSL generation → compilation → preview update

---

## Key Files to Modify

| File | Change |
|------|--------|
| `core/canvas/include/pulp/canvas/canvas.hpp` | Add `ShaderUniforms`, `draw_with_sksl()` virtual |
| `core/canvas/src/skia_canvas.cpp` | Implement `draw_with_sksl()` with compile-and-cache |
| `core/canvas/include/pulp/canvas/skia_canvas.hpp` | Add shader cache member |
| `core/view/include/pulp/view/widgets.hpp` | Add `custom_sksl_` to Knob/Fader/Toggle |
| `core/view/src/widgets.cpp` | Early shader path in paint() |
| `core/view/src/widget_bridge.cpp` | Register setWidgetShader, clearWidgetShader, compileShader |
| `examples/design-tool/design-tool.js` | Chat prompt engineering, SkSL parsing, selection UI |

---

## Success Criteria

User types "brushed metal Moog knob with LED ring" in chat →
1. Claude generates valid SkSL (~300ms response)
2. Bridge compiles SkSL via SkRuntimeEffect (~1ms)
3. Knob preview updates to show metallic 3D knob with glowing ring (~16ms frame)
4. If SkSL compilation fails, canvas 2D fallback renders automatically
5. Style can be saved as "Moog LED" and applied to other knobs/faders
