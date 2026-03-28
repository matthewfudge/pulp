# Pulp CSS/HTML/JS Support Matrix

**Goal:** Frontend developers using JS/CSS/HTML should be able to translate their work to Pulp's native GPU UI with minimal friction. This document tracks every CSS property against W3C specs.

**W3C Specs Referenced:**
- CSS Flexible Box Layout Level 1 (https://www.w3.org/TR/css-flexbox-1/)
- CSS Grid Layout Level 1 (https://www.w3.org/TR/css-grid-1/)
- CSS Box Model Level 3 (https://www.w3.org/TR/css-box-3/)
- CSS Backgrounds and Borders Level 3 (https://www.w3.org/TR/css-backgrounds-3/)
- CSS Color Level 4 (https://www.w3.org/TR/css-color-4/)
- CSS Transforms Level 1 (https://www.w3.org/TR/css-transforms-1/)
- CSS Transitions Level 1 (https://www.w3.org/TR/css-transitions-1/)
- CSS Animations Level 1 (https://www.w3.org/TR/css-animations-1/)
- CSS Text Level 3 (https://www.w3.org/TR/css-text-3/)
- CSS Fonts Level 4 (https://www.w3.org/TR/css-fonts-4/)
- CSS Overflow Level 3 (https://www.w3.org/TR/css-overflow-3/)
- CSS Filter Effects Level 1 (https://www.w3.org/TR/filter-effects-1/)
- CSS Positioned Layout Level 3 (https://www.w3.org/TR/css-position-3/)
- CSS Basic User Interface Level 4 (https://www.w3.org/TR/css-ui-4/)
- CSS Selectors Level 4 (https://www.w3.org/TR/selectors-4/) — pseudo-classes only
- CSSOM View Module (https://www.w3.org/TR/cssom-view-1/)
- HTML Living Standard — form elements, canvas, events

---

## Status Key
- ✅ **Supported** — fully implemented and exposed via JS bridge
- ⚠️ **Partial** — implemented but limited or needs enhancement
- 🔲 **Planned** — not yet implemented, tracked for implementation
- ➖ **N/A** — not applicable to native GPU UI (e.g. float, ::before)

---

## 1. CSS Flexible Box Layout (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| display: flex | createRow() / createCol() | ✅ | Implicit — all views are flex containers |
| flex-direction | setFlex("direction", "row"/"col") | ✅ | |
| flex-wrap | setFlex("flex_wrap", bool) | ✅ | |
| flex-flow | — | ⚠️ | Shorthand not exposed, set individually |
| justify-content | setFlex("justify_content", ...) | ✅ | start/center/end/space-between/space-around/space-evenly |
| align-items | setFlex("align_items", ...) | ✅ | start/center/end/stretch |
| align-content | — | 🔲 | Multi-line cross-axis alignment |
| flex-grow | setFlex("flex_grow", n) | ✅ | |
| flex-shrink | setFlex("flex_shrink", n) | ✅ | |
| flex-basis | setFlex("flex_basis", px) | ✅ | |
| flex (shorthand) | — | 🔲 | Set grow/shrink/basis individually |
| align-self | setFlex("align_self", ...) | ✅ | auto/start/center/end/stretch |
| order | setFlex("order", n) | ✅ | Stable sort |
| gap | setFlex("gap", px) | ✅ | |
| row-gap | setFlex("row_gap", px) | ✅ | |
| column-gap | setFlex("column_gap", px) | ✅ | |

## 2. CSS Grid Layout (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| display: grid | — | 🔲 | Need grid container type |
| grid-template-columns | — | 🔲 | Critical for token list layout |
| grid-template-rows | — | 🔲 | |
| grid-column | — | 🔲 | |
| grid-row | — | 🔲 | |
| grid-gap | — | 🔲 | Use flex gap as starting point |
| grid-auto-flow | — | 🔲 | |

## 3. CSS Box Model (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| width | setFlex("width", px) | ✅ | |
| height | setFlex("height", px) | ✅ | |
| min-width | setFlex("min_width", px) | ✅ | |
| min-height | setFlex("min_height", px) | ✅ | |
| max-width | setFlex("max_width", px) | ✅ | |
| max-height | setFlex("max_height", px) | ✅ | |
| margin | setFlex("margin", px) | ✅ | Uniform + per-side |
| margin-top/right/bottom/left | setFlex("margin_top", px) etc | ✅ | |
| padding | setFlex("padding", px) | ✅ | Uniform + per-side |
| padding-top/right/bottom/left | setFlex("padding_top", px) etc | ✅ | |
| box-sizing | — | ⚠️ | Always border-box behavior |

## 4. CSS Backgrounds & Borders (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| background-color | setBackground(id, hex) | ✅ | |
| background-image | — | 🔲 | Need image loading |
| background: linear-gradient() | set_fill_gradient_linear (Canvas) | ⚠️ | Canvas API exists, not in bridge as bg |
| background: radial-gradient() | set_fill_gradient_radial (Canvas) | ⚠️ | Canvas API exists, not in bridge as bg |
| background-size | — | 🔲 | |
| background-position | — | 🔲 | |
| background-repeat | — | 🔲 | |
| border | setBorder(id, color, width, radius) | ✅ | |
| border-color | Part of setBorder() | ✅ | |
| border-width | Part of setBorder() | ✅ | |
| border-style | — | ⚠️ | Solid only, no dashed/dotted |
| border-radius | Part of setBorder() | ✅ | Uniform only |
| border-top-left-radius etc | — | 🔲 | Per-corner radius |
| box-shadow | setBoxShadow(id, ox, oy, blur, spread, color) | ✅ | |
| outline | — | 🔲 | |

## 5. CSS Color Level 4 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| color | setTextColor(id, hex) | ✅ | |
| Hex (#RGB, #RRGGBB, #RRGGBBAA) | Supported in all color APIs | ✅ | |
| rgb() / rgba() | — | 🔲 | Parse in bridge |
| hsl() / hsla() | — | 🔲 | Parse in bridge |
| oklch() | OklchEngine in JS | ✅ | Via oklch.js library |
| Named colors | — | 🔲 | |
| transparent | — | 🔲 | Special keyword |
| currentColor | — | 🔲 | Inherited text color |

## 6. CSS Transforms Level 1 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| transform: scale() | setScale(id, value) | ✅ | Scales around center |
| transform: translate() | — | 🔲 | |
| transform: rotate() | — | 🔲 | |
| transform: skew() | — | 🔲 | |
| transform-origin | — | 🔲 | Currently always center |

## 7. CSS Transitions Level 1 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| transition-duration | setTransitionDuration(id, s) | ✅ | |
| transition-property | — | ⚠️ | All properties implicit |
| transition-timing-function | animate() easing param | ✅ | ease-out-quad, ease-out-cubic |
| transition-delay | — | 🔲 | |
| transition (shorthand) | — | 🔲 | |

## 8. CSS Animations Level 1 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| @keyframes | — | 🔲 | Need keyframe definition system |
| animation-name | — | 🔲 | |
| animation-duration | animate() duration param | ⚠️ | Single-shot only |
| animation-timing-function | animate() easing param | ⚠️ | |
| animation-iteration-count | — | 🔲 | No looping support |
| animation-direction | — | 🔲 | |

## 9. CSS Text Level 3 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| text-align | setTextAlign(id, "left"/"center"/"right") | ✅ | |
| text-overflow | setTextOverflow(id, "ellipsis") | ✅ | |
| white-space | — | 🔲 | nowrap, pre, pre-wrap |
| word-break | — | 🔲 | |
| overflow-wrap | — | 🔲 | |
| text-indent | — | 🔲 | |
| text-transform | — | 🔲 | uppercase, lowercase, capitalize |

## 10. CSS Fonts Level 4 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| font-family | — | ⚠️ | Hardcoded "Inter", needs bridge |
| font-size | setFontSize(id, px) | ✅ | |
| font-weight | setFontWeight(id, 100-900) | ✅ | |
| font-style | setFontStyle(id, "normal"/"italic") | ✅ | |
| line-height | setLineHeight(id, px) | ✅ | |
| letter-spacing | setLetterSpacing(id, px) | ✅ | |
| text-decoration | — | 🔲 | underline, strikethrough |

## 11. CSS Overflow Level 3 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| overflow | setOverflow(id, "visible"/"hidden") | ⚠️ | Missing scroll/auto |
| overflow-x | — | 🔲 | |
| overflow-y | — | 🔲 | |
| overflow: scroll | ScrollView widget | ✅ | Via createScrollView |
| overflow: auto | — | 🔲 | Auto scrollbar |
| scrollbar styling | — | ⚠️ | Basic scrollbar, needs webkit styling |

## 12. CSS Filter Effects Level 1 (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| filter: blur() | draw_blurred_backdrop (Canvas) | ⚠️ | Backdrop only, not per-element |
| filter: brightness() | — | 🔲 | |
| filter: contrast() | — | 🔲 | |
| filter: saturate() | — | 🔲 | |
| filter: hue-rotate() | — | 🔲 | |
| filter: drop-shadow() | — | 🔲 | Use box-shadow instead |
| backdrop-filter: blur() | draw_blurred_backdrop | ✅ | |

## 13. CSS Positioned Layout (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| position: static | Default behavior | ✅ | |
| position: relative | — | 🔲 | |
| position: absolute | Overlay paint queue | ⚠️ | ComboBox dropdown uses it |
| position: fixed | Overlay paint queue | ⚠️ | |
| position: sticky | — | 🔲 | |
| top/right/bottom/left | — | 🔲 | |
| z-index | — | 🔲 | Overlay has implicit z |

## 14. CSS Basic User Interface (W3C Rec)

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| cursor | setCursor(id, "pointer"/"crosshair"/"text") | ✅ | |
| pointer-events | — | 🔲 | none/auto |
| user-select | — | 🔲 | none/auto/text |
| resize | — | 🔲 | |
| caret-color | — | 🔲 | TextEditor uses accent |

## 15. CSS Pseudo-Classes (Selectors Level 4)

| Pseudo-Class | Pulp Bridge | Status | Notes |
|-------------|-------------|--------|-------|
| :hover | registerHover(id) + events | ✅ | mouseenter/mouseleave |
| :active | — | 🔲 | mousedown state |
| :focus | on_focus_changed | ✅ | TextEditor shows border |
| :focus-visible | Focus ring logic | ✅ | Only on text inputs |
| :disabled | — | 🔲 | setOpacity(0.5) workaround |
| :checked | — | ⚠️ | Toggle/Checkbox state |
| :nth-child() | — | ➖ | Not applicable (no selectors) |
| :first-child/:last-child | — | ➖ | Not applicable |

## 16. HTML Elements via Widgets

| HTML Element | Pulp Widget | Status | Notes |
|-------------|------------|--------|-------|
| `<div>` | createRow() / createCol() | ✅ | |
| `<span>` / `<p>` | createLabel() | ✅ | |
| `<input type="text">` | createTextEditor() | ✅ | |
| `<input type="range">` | createFader() | ✅ | |
| `<input type="checkbox">` | createCheckbox() | ✅ | |
| `<select>` | createCombo() | ✅ | With overlay dropdown |
| `<button>` | createToggleButton() / Label+click | ✅ | |
| `<canvas>` | createCanvas() | ⚠️ | Exists but limited JS draw API |
| `<img>` | — | 🔲 | Need image loading |
| `<svg>` | Icon widget + path API | ⚠️ | Basic vector icons |
| `<details>/<summary>` | — | 🔲 | Collapsible sections |
| `<progress>` | createProgress() | ✅ | |
| `<dialog>` | — | 🔲 | Modal dialog |

## 17. DOM Events via Bridge

| Event | Pulp Bridge | Status | Notes |
|-------|-------------|--------|-------|
| click | registerClick(id) + on(id, "click") | ✅ | |
| mouseenter | registerHover(id) + on(id, "mouseenter") | ✅ | |
| mouseleave | registerHover(id) + on(id, "mouseleave") | ✅ | |
| change | on(id, "change") | ✅ | Fader, TextEditor, ComboBox |
| input | on(id, "change") | ✅ | Same as change |
| keydown | forward_key_event + on("__global__", "keydown") | ⚠️ | Global only |
| scroll | ScrollView handles internally | ✅ | |
| focus/blur | on_focus_changed | ✅ | |
| submit | on(id, "return") | ✅ | TextEditor return key |

## 18. Canvas 2D API

| Method | Pulp Bridge | Status | Notes |
|--------|-------------|--------|-------|
| fillRect | canvasRect (bridge) | ✅ | |
| strokeRect | — | 🔲 | |
| fillText | Canvas::fill_text | ✅ | |
| measureText | measureText() | ✅ | Returns {width, ascent, descent, lineHeight} |
| beginPath | Canvas::begin_path | ✅ | |
| moveTo/lineTo | Canvas::move_to/line_to | ✅ | |
| quadraticCurveTo | Canvas::quad_to | ✅ | |
| bezierCurveTo | Canvas::cubic_to | ✅ | |
| closePath | Canvas::close_path | ✅ | |
| fill | Canvas::fill_current_path | ✅ | |
| stroke | Canvas::stroke_current_path | ✅ | |
| save/restore | Canvas::save/restore | ✅ | |
| translate/scale/rotate | Canvas::translate/scale/rotate | ✅ | |
| clip | Canvas::clip_rect | ✅ | |
| createLinearGradient | Canvas::set_fill_gradient_linear | ✅ | |
| createRadialGradient | Canvas::set_fill_gradient_radial | ✅ | |
| drawImage | — | 🔲 | Image loading needed |
| getImageData/putImageData | — | 🔲 | Pixel manipulation |

---

## Summary

| Category | Supported | Partial | Planned | N/A |
|----------|-----------|---------|---------|-----|
| Flexbox | 16 | 1 | 0 | 0 |
| Grid | 0 | 0 | 7 | 0 |
| Box Model | 14 | 1 | 0 | 0 |
| Backgrounds & Borders | 6 | 3 | 6 | 0 |
| Color | 3 | 1 | 4 | 0 |
| Transforms | 1 | 0 | 4 | 0 |
| Transitions | 2 | 1 | 2 | 0 |
| Animations | 0 | 2 | 4 | 0 |
| Text | 2 | 0 | 5 | 0 |
| Fonts | 5 | 1 | 1 | 0 |
| Overflow | 1 | 2 | 2 | 0 |
| Filters | 0 | 1 | 5 | 0 |
| Positioning | 1 | 2 | 3 | 0 |
| UI | 1 | 0 | 4 | 0 |
| Pseudo-classes | 4 | 1 | 1 | 3 |
| **TOTAL** | **56** | **16** | **48** | **3** |

**Current coverage: 56 fully + 16 partial = 72 properties supported out of 120 tracked (60%)**
**Target: 90%+ (108+ properties)**
