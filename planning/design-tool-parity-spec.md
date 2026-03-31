# Design Tool Parity Spec

**Date:** 2026-03-28
**Reference:** `ai-style-designer/Tools/theme-designer.html` (5,724 lines, 120 tokens, 7 export formats)
**Target:** `pulp/examples/design-tool/` (1,489 lines JS, 16 tokens, JSON-only export)

---

## Executive Summary

The HTML reference is a production-grade design token editor. The Pulp desktop app has the skeleton (3-panel layout, shade ramps, chat, undo) but is missing the core editing UX that makes it actually useful. The biggest gaps are: no token editing, no palette picker popup, no gamut triangle, broken chat UX, and only 16 of 120 tokens.

---

## Complete Gap Inventory

### A. Color System Panel (Left Sidebar Top)

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| A1 | Template selector | Audio Studio + 22 Tailwind colors | None | **Missing** |
| A2 | Palette rows with base dot + 11-shade mini ramp | 5 rows, clickable to expand | 5 ramps, flat layout, no expand | **Partial** — ramps exist but no expand/collapse |
| A3 | Gamut triangle canvas | Full OKLCH gamut with pixel-fill, boundary line, crosshair | None — only H/C/L fader sliders | **Missing** |
| A4 | Gamut triangle drag | Drag dot, clamped to sRGB boundary via binary search | N/A | **Missing** |
| A5 | H slider with OKLCH hue gradient | Rainbow gradient thumb | Basic fader | **Partial** — fader exists, no gradient styling |
| A6 | C slider with dynamic chroma gradient | Updates per hue | None | **Missing** |
| A7 | OKLCH numeric inputs (L, C, H) | 3 text inputs + format selector (OKLCH/HEX/RGB) | L/C/H text labels (read-only) | **Missing** — inputs not editable |
| A8 | Large shade swatches (32px) with contrast badges | 11 shades, hover scale, AAA/AA badges, WCAG ratios | None | **Missing** |
| A9 | Shade override (click shade → native color picker) | Gold border on overridden shades | None | **Missing** |
| A10 | Generate Opposite Mode button | One-click light↔dark generation | None | **Missing** |
| A11 | Palette Save/Load | Download/upload .colorsystem.json | None | **Missing** |
| A12 | Contrast "Aa" preview button | Info modal showing on-white + on-black ratios | None | **Missing** |
| A13 | Collapsible color system section | Arrow toggle, smooth expand | Always visible | **Missing** |

### B. Token Palette Picker Popup

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| B1 | Click token swatch → floating popup | Fixed-position overlay, 230px wide, z-index 200 | No click handler on swatches | **Missing entirely** |
| B2 | 5 palette shade grids in popup | ACCENT/NEUTRAL/SUCCESS/WARNING/ERROR, 11 shades each | N/A | **Missing** |
| B3 | Shade swatch hover: scale(1.3) | Smooth 0.1s transition | N/A | **Missing** |
| B4 | Click shade → apply to token | setColor + CSS sync + canvas redraw | N/A | **Missing** |
| B5 | Custom color picker (expandable) | Gamut triangle + hue slider inside popup | N/A | **Missing** |
| B6 | Eyedropper tool | Chrome/Edge EyeDropper API | N/A | **Missing** (not feasible on native) |
| B7 | Popup positioning (below swatch, flip if overflow) | getBoundingClientRect + clamping | N/A | **Missing** |
| B8 | Click-outside dismissal | document click listener with setTimeout(10) guard | N/A | **Missing** |

### C. Per-Token Undo/Reset

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| C1 | Undo/Redo buttons in popup | SVG icons + "undo"/"redo" text, show/hide per state | None | **Missing** |
| C2 | Reset button | Reverts to original generated color | None | **Missing** |
| C3 | Per-token history tracking | `TokenHistory` object: original + changes[] + index | None — global undo only | **Missing** |
| C4 | Modified indicator (* after name, accent color) | `.has-change` class + `::after` pseudo | None | **Missing** |
| C5 | Flash animation on token change | `@keyframes token-flash`: box-shadow pulse 0.6s | None | **Missing** |

### D. Token System

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| D1 | Token count | 120 tokens (36 colors + 12 values + 50 custom + 22 JUCE) | 16 tokens in 4 groups | **92% missing** |
| D2 | Token sections | 4 sections: Visage Tokens, Values, Custom, JUCE | 4 groups: Background, Text, Accent, Controls | **Different taxonomy** |
| D3 | Hex input per token | Editable monospace input, uppercase, focus→accent border | None — name label only | **Missing** |
| D4 | Value token sliders | Slider + number input for geometry tokens | None | **Missing** |
| D5 | Token group expand/collapse | Arrow rotation 0.15s, count badge | No collapse | **Missing** |
| D6 | Section headers (sticky) | Uppercase, accent color, sticky top | None | **Missing** |
| D7 | Token modification count in status bar | "N token(s) modified" updates live | "0 tokens modified" static | **Broken** |
| D8 | Alpha channel per token | Stored in JSON, preserved on color pick | None | **Missing** |

### E. State Scrubber

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| E1 | State pills in toolbar | Default/Hover/Focus/Disabled/Error — 5 clickable pills | None — states shown as static preview section | **Missing** |
| E2 | Live state switching | CSS `[data-preview-state]` attribute selectors override styles | N/A | **Missing** |
| E3 | Hover state: buttons get hover bg, borders accent | Pure CSS via `!important` overrides | N/A | **Missing** |
| E4 | Focus state: focus ring on interactive elements | Via CSS override | N/A | **Missing** |
| E5 | Disabled state: opacity 0.4, pointer-events none | Via CSS override | N/A | **Missing** |

### F. Preview Components

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| F1 | Knob styles: default/filled/notched/glossy | 4 canvas-drawn styles selectable | Default arc only | **3 styles missing** |
| F2 | Animated spinner | CSS `spin` animation 0.8s infinite | Static "loading" label | **Missing** |
| F3 | Card badges (OK, !) | Top-right positioned badges on Ready/Error cards | None | **Missing** |
| F4 | Card loading state | Spinner inside card | None | **Missing** |
| F5 | VU meter gradient | Green→yellow→red linear gradient | Flat color meters | **Missing** (Pulp has meters but no gradient) |
| F6 | Waveform playhead | Dashed vertical line at 60% | None | **Missing** |
| F7 | Waveform fill | Semi-transparent fill below line | Line only | **Missing** |
| F8 | Plugin chrome title bar | Traffic lights + "Plugin Preview" title | None | **Missing** |
| F9 | Effects: gradient, shadow, bloom, blur cards | 4 effect demonstration cards | Placeholder labels | **Partial** |
| F10 | Context menu hover state | Paste row highlighted with PopupMenuSelection | Static | **Missing** |
| F11 | Dropdown visual preview | Non-interactive styled dropdown | Basic combo | **Partial** |

### G. Export System

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| G1 | Export modal with tabs | 7 format tabs in overlay | None — direct JSON file write | **Missing** |
| G2 | JSON format | Full theme JSON | Exists via getThemeJson() | **OK** |
| G3 | CSS Variables format | `:root { --vt-Name: rgba(...) }` | None | **Missing** |
| G4 | C++ Header format | `VISAGE_THEME_COLOR` macros | None | **Missing** |
| G5 | C++ Palette format | `palette.setColor()` calls | None | **Missing** |
| G6 | OKLCH CSS format | `oklch(L% C H)` variables | None | **Missing** |
| G7 | Color System JSON | Full palette + shade + role data | None | **Missing** |
| G8 | Style System JSON | Geometry + effects + gradients | None | **Missing** |
| G9 | Code preview | Monospace text display | None | **Missing** |
| G10 | Copy to Clipboard | Clipboard API + fallback | None | **Missing** |
| G11 | Download button | Blob URL + programmatic link | Exists (JSON only) | **Partial** |
| G12 | Cmd+E shortcut | Opens export | None | **Missing** |

### H. Chat / AI

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| H1 | Chat sends to Claude | Via Tauri invoke / mock mode | Via synchronous `exec("claude --print")` | **Works but blocks UI** |
| H2 | Component-scoped editing | Cmd+click → chat edits only that component's tokens | Chat edits all tokens | **Missing** |
| H3 | Image upload | File input → base64 → in prompt | Upload icon exists, no implementation | **Missing** |
| H4 | Change summary in response | Lists modified tokens | Shows "Applied N token changes" | **Partial** |
| H5 | Typing indicator | 3-dot bounce animation | None | **Missing** |
| H6 | Thumbnail capture | 200x80px canvas preview in each message | None | **Missing** |
| H7 | Chat export | Export conversation history | None | **Missing** |
| H8 | Model selector persistence | localStorage | None — hardcoded model | **Missing** |
| H9 | Style snapshots with restore | Full theme state per message | Exists — restore button works | **OK** |
| H10 | Async execution | Non-blocking (Tauri async invoke) | Blocking popen() freezes UI | **Broken UX** |

### I. Visual Polish

| # | Feature | HTML | Pulp | Gap |
|---|---------|------|------|-----|
| I1 | Toast notifications | Fixed bottom-center, slide-up + fade, auto-dismiss 2.5s | None | **Missing** |
| I2 | Info/help modals | "?" buttons → contextual help overlays | None | **Missing** |
| I3 | Token row hover | rgba(255,255,255,0.025) background, 0.08s transition | None | **Missing** |
| I4 | Button hover states | Border → accent, background raise, 0.15s transition | None — static buttons | **Missing** |
| I5 | Custom scrollbar styling | 6px wide, rounded thumb | Default OS scrollbar | **Missing** |
| I6 | Focus states: border → accent | On all inputs | None | **Missing** |
| I7 | Cursor changes | pointer/crosshair per element type | Default cursor everywhere | **Missing** |

### J. Infrastructure Gaps (C++ bridge additions needed)

| # | Gap | What's Needed |
|---|-----|---------------|
| J1 | Canvas gradient fill | Add `fill_gradient_linear` to CanvasDrawCmd + JS bridge |
| J2 | Drag-to-JS forwarding | Forward `on_mouse_drag` to JS as `pointermove` events |
| J3 | Canvas pixel fill (ImageData) | For gamut triangle: need per-pixel color fill or gradient approximation |
| J4 | Async exec | Non-blocking shell command execution for Claude CLI |

---

## Proposed Implementation Phases

### Phase D1: Token Editing Core
**Goal:** Make the design tool actually editable.
**Scope:** B1-B4, B7-B8, C1-C5, D3, D7

- Implement palette picker popup using overlay paint queue (ComboBox pattern)
- 5 palette shade grids (reuse existing shade ramp data)
- Click shade → apply to token via `applyTokenDiff`
- Per-token undo/redo/reset in JS
- Modified indicator (* on token name)
- Flash animation (box-shadow pulse via `setBoxShadow` + `animate`)
- Token hex input (editable text, apply on return)
- Status bar modification count
- Click-outside popup dismissal

### Phase D2: Gamut Triangle & Color Picker
**Goal:** Professional color picking UX.
**Scope:** A3-A6, B5, J1-J3
**Depends on:** D1

- Add gradient fill to CanvasDrawCmd (C++ bridge change)
- Add drag-to-JS forwarding (C++ bridge change)
- Render gamut triangle via CanvasWidget (pixel-fill approximation using many thin gradient strips)
- Drag interaction with sRGB boundary clamping
- H/C sliders with gradient backgrounds
- Mini gamut picker inside palette popup
- OKLCH numeric inputs (editable)

### Phase D3: State Scrubber & Preview Polish
**Goal:** Live state visualization + missing preview components.
**Scope:** E1-E5, F1-F4, F6-F8

- State pills in toolbar (Default/Hover/Focus/Disabled/Error)
- Live state switching via theme overrides
- Animated spinner (CSS spin via `animate` bridge)
- Card badges (OK/!) via positioned labels
- Card loading state with spinner
- Waveform playhead + fill
- Plugin chrome title bar

### Phase D4: Export System
**Goal:** Professional multi-format export.
**Scope:** G1-G12

- Export modal overlay (overlay paint queue)
- 7 format tabs: JSON, CSS Vars, C++, Palette, OKLCH, Color System, Style System
- Code preview display
- Copy to clipboard (bridge: `exec("pbcopy")`)
- Download (bridge: file write)
- Cmd+E shortcut

### Phase D5: Full Token Registry
**Goal:** 120 tokens matching HTML reference.
**Scope:** D1-D2, D4-D6, D8, A1

- Expand from 16 to 120 tokens (36 colors + 12 values + 50 custom + 22 JUCE)
- 4 token sections with sticky headers
- Token group expand/collapse with count badges
- Value token sliders for geometry tokens
- Alpha channel support
- Template selector (Audio Studio + Tailwind)

### Phase D6: AI & Chat Enhancement
**Goal:** Non-blocking AI with component scoping.
**Scope:** H1-H8, H10, J4

- Async exec (non-blocking Claude CLI call)
- Typing indicator (3-dot bounce)
- Component-scoped editing via inspector context
- Image upload (base64 in prompt)
- Change summary display
- Thumbnail capture per message
- Model selector persistence

### Phase D7: Visual Polish
**Goal:** Production-quality micro-interactions.
**Scope:** I1-I7, A10, A12, A13, F9-F11

- Toast notification system
- Info/help modals with "?" buttons
- Token row hover states
- Button hover transitions
- Custom scrollbar styling
- Focus state: border → accent
- Cursor changes per element type
- Collapsible color system section
- Generate Opposite Mode button
- Contrast preview modal
- Effect cards (gradient, shadow, bloom, blur)
- Context menu hover state

---

## Priority Recommendation

**D1 → D2 → D3** is the critical path. Without token editing (D1), the tool is a viewer. Without the gamut triangle (D2), color picking is primitive. Without state scrubbing (D3), the preview is static.

D4-D7 are high-value but independent — can be parallelized or deferred.

---

## Bridge Additions Summary

| Addition | Files | Complexity |
|----------|-------|-----------|
| Canvas gradient fill | canvas_widget.hpp/cpp, widget_bridge.cpp | Medium — add CanvasDrawCmd type + replay |
| Drag-to-JS | widget_bridge.cpp, window_host_mac.mm | Low — forward on_mouse_drag to JS dispatch |
| Async exec | widget_bridge.cpp | Medium — background thread + callback |
| Clipboard write | widget_bridge.cpp | Low — `exec("pbcopy")` |
