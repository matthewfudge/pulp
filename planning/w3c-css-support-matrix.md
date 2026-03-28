Now I have comprehensive knowledge of the entire system. Let me produce the document.

Here is the complete document:

---

# Pulp W3C CSS / HTML / JS Parity Matrix

**Version:** 2026-03-27
**Scope:** Every CSS property a frontend developer would expect, mapped to Pulp's native GPU UI system (Skia Graphite + WebGPU/Dawn + QuickJS bridge). This is NOT a browser -- it translates CSS concepts to native GPU rendering.

**W3C Specs Referenced:**
- CSS Display Level 3 (https://www.w3.org/TR/css-display-3/)
- CSS Flexible Box Layout Level 1 (https://www.w3.org/TR/css-flexbox-1/)
- CSS Grid Layout Level 1 (https://www.w3.org/TR/css-grid-1/)
- CSS Box Model Level 3 (https://www.w3.org/TR/css-box-3/)
- CSS Box Alignment Level 3 (https://www.w3.org/TR/css-align-3/)
- CSS Sizing Level 3 (https://www.w3.org/TR/css-sizing-3/)
- CSS Writing Modes Level 4 (https://www.w3.org/TR/css-writing-modes-4/)
- CSS Positioned Layout Level 3 (https://www.w3.org/TR/css-position-3/)
- CSS Containment Level 3 (https://www.w3.org/TR/css-contain-3/)
- CSS Overflow Level 3 (https://www.w3.org/TR/css-overflow-3/)
- CSS Backgrounds and Borders Level 3 (https://www.w3.org/TR/css-backgrounds-3/)
- CSS Color Level 4 (https://www.w3.org/TR/css-color-4/)
- CSS Transforms Level 1 (https://www.w3.org/TR/css-transforms-1/)
- CSS Transitions Level 1 (https://www.w3.org/TR/css-transitions-1/)
- CSS Animations Level 1 (https://www.w3.org/TR/css-animations-1/)
- CSS Filter Effects Level 1 (https://www.w3.org/TR/filter-effects-1/)
- CSS Compositing and Blending Level 1 (https://www.w3.org/TR/compositing-1/)
- CSS Masking Level 1 (https://www.w3.org/TR/css-masking-1/)
- CSS Fonts Level 4 (https://www.w3.org/TR/css-fonts-4/)
- CSS Text Level 3 (https://www.w3.org/TR/css-text-3/)
- CSS Inline Layout Level 3 (https://www.w3.org/TR/css-inline-3/)
- CSS Text Decoration Level 4 (https://www.w3.org/TR/css-text-decor-4/)
- CSS Basic User Interface Level 4 (https://www.w3.org/TR/css-ui-4/)
- CSS Selectors Level 4 (https://www.w3.org/TR/selectors-4/)
- CSS Scroll Snap Module Level 1 (https://www.w3.org/TR/css-scroll-snap-1/)
- CSS Custom Properties Level 1 (https://www.w3.org/TR/css-variables-1/)
- CSSOM View Module (https://www.w3.org/TR/cssom-view-1/)
- HTML Living Standard (form elements, canvas, events)
- Canvas 2D Context (https://html.spec.whatwg.org/multipage/canvas.html)

---

## Status Key

- **✅ Supported** -- fully implemented and exposed via JS bridge
- **⚠️ Partial** -- implemented but limited or needs enhancement
- **🔲 Planned** -- not yet implemented, tracked for implementation
- **➖ N/A** -- intentionally not applicable to native GPU UI

---

## 1. CSS Display Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `display: flex` | `createRow()` / `createCol()` | ✅ | Implicit -- all views are flex containers |
| `display: inline-flex` | -- | ➖ | No inline formatting context; all containers are block-level flex |
| `display: grid` | -- | 🔲 | Need grid container type |
| `display: inline-grid` | -- | ➖ | No inline formatting context |
| `display: block` | Default View | ⚠️ | Views behave as block-level flex items |
| `display: inline` | -- | ➖ | No inline formatting context; use Label for inline text |
| `display: inline-block` | -- | ➖ | Not applicable |
| `display: none` | `setVisible(id, false)` | ✅ | `set_visible(false)` removes from layout |
| `display: contents` | -- | 🔲 | Useful for wrapper elimination |
| `display: flow-root` | -- | ➖ | No block formatting context model |
| `display: table` / table-* | -- | ➖ | No table layout; use grid when available |
| `display: list-item` | -- | ➖ | Not applicable |

---

## 2. CSS Flexible Box Layout Level 1

### Container Properties

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `flex-direction` | `setFlex(id, "direction", "row"/"col")` | ✅ | `row` and `column` supported |
| `flex-wrap` | `setFlex(id, "flex_wrap", 1)` | ✅ | Boolean; `wrap-reverse` not supported |
| `flex-flow` (shorthand) | -- | ⚠️ | Set direction + wrap individually |
| `justify-content` | `setFlex(id, "justify_content", ...)` | ✅ | `start`, `center`, `end`, `space-between`, `space-around`, `space-evenly` |
| `align-items` | `setFlex(id, "align_items", ...)` | ✅ | `start`, `center`, `end`, `stretch` |
| `align-content` | -- | 🔲 | Multi-line cross-axis alignment (requires flex-wrap) |
| `gap` | `setFlex(id, "gap", px)` | ✅ | Shorthand for row-gap + column-gap |
| `row-gap` | `setFlex(id, "row_gap", px)` | ✅ | |
| `column-gap` | `setFlex(id, "column_gap", px)` | ✅ | |

### Item Properties

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `flex-grow` | `setFlex(id, "flex_grow", n)` | ✅ | |
| `flex-shrink` | `setFlex(id, "flex_shrink", n)` | ✅ | Default: 1 |
| `flex-basis` | `setFlex(id, "flex_basis", px)` | ✅ | -1 = use preferred size |
| `flex` (shorthand) | -- | 🔲 | Set grow/shrink/basis individually |
| `align-self` | `setFlex(id, "align_self", ...)` | ✅ | `auto`, `start`, `center`, `end`, `stretch` |
| `order` | `setFlex(id, "order", n)` | ✅ | Stable sort |

---

## 3. CSS Grid Layout Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `display: grid` | -- | 🔲 | Need grid container type |
| `grid-template-columns` | -- | 🔲 | Critical for dashboard layouts |
| `grid-template-rows` | -- | 🔲 | |
| `grid-template-areas` | -- | 🔲 | Named area placement |
| `grid-template` (shorthand) | -- | 🔲 | |
| `grid-auto-columns` | -- | 🔲 | |
| `grid-auto-rows` | -- | 🔲 | |
| `grid-auto-flow` | -- | 🔲 | `row`, `column`, `dense` |
| `grid` (shorthand) | -- | 🔲 | |
| `grid-column-start` | -- | 🔲 | |
| `grid-column-end` | -- | 🔲 | |
| `grid-row-start` | -- | 🔲 | |
| `grid-row-end` | -- | 🔲 | |
| `grid-column` (shorthand) | -- | 🔲 | |
| `grid-row` (shorthand) | -- | 🔲 | |
| `grid-area` | -- | 🔲 | |
| `gap` (grid context) | -- | 🔲 | Reuse flex gap infrastructure |
| `row-gap` (grid context) | -- | 🔲 | |
| `column-gap` (grid context) | -- | 🔲 | |
| `justify-items` | -- | 🔲 | |
| `justify-self` | -- | 🔲 | |
| `align-items` (grid) | -- | 🔲 | |
| `align-self` (grid) | -- | 🔲 | |
| `place-items` (shorthand) | -- | 🔲 | |
| `place-self` (shorthand) | -- | 🔲 | |
| `place-content` (shorthand) | -- | 🔲 | |
| `fr` unit | -- | 🔲 | Fractional unit for track sizing |
| `repeat()` | -- | 🔲 | `repeat(3, 1fr)` etc |
| `minmax()` | -- | 🔲 | `minmax(100px, 1fr)` etc |
| `auto-fill` / `auto-fit` | -- | 🔲 | Responsive grid tracks |

---

## 4. CSS Box Model Level 3

### Sizing

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `width` | `setFlex(id, "width", px)` | ✅ | Maps to `preferred_width` |
| `height` | `setFlex(id, "height", px)` | ✅ | Maps to `preferred_height` |
| `min-width` | `setFlex(id, "min_width", px)` | ✅ | |
| `min-height` | `setFlex(id, "min_height", px)` | ✅ | |
| `max-width` | `setFlex(id, "max_width", px)` | ✅ | 0 = no maximum |
| `max-height` | `setFlex(id, "max_height", px)` | ✅ | 0 = no maximum |
| `box-sizing` | -- | ⚠️ | Always `border-box` behavior; `content-box` not supported |

### Margin

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `margin` | `setFlex(id, "margin", px)` | ✅ | Uniform all sides |
| `margin-top` | `setFlex(id, "margin_top", px)` | ✅ | -1 = use uniform |
| `margin-right` | `setFlex(id, "margin_right", px)` | ✅ | |
| `margin-bottom` | `setFlex(id, "margin_bottom", px)` | ✅ | |
| `margin-left` | `setFlex(id, "margin_left", px)` | ✅ | |
| `margin: auto` | -- | 🔲 | Auto margins for centering (flex spec requires this) |
| Margin collapse | -- | ➖ | No block formatting context; flex items don't collapse margins per spec |

### Padding

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `padding` | `setFlex(id, "padding", px)` | ✅ | Uniform all sides |
| `padding-top` | `setFlex(id, "padding_top", px)` | ✅ | -1 = use uniform |
| `padding-right` | `setFlex(id, "padding_right", px)` | ✅ | |
| `padding-bottom` | `setFlex(id, "padding_bottom", px)` | ✅ | |
| `padding-left` | `setFlex(id, "padding_left", px)` | ✅ | |

---

## 5. CSS Box Alignment Level 3

| Property | Context | Pulp Bridge | Status | Notes |
|----------|---------|-------------|--------|-------|
| `justify-content` | Flex | `setFlex(id, "justify_content", ...)` | ✅ | 6 values supported |
| `justify-content` | Grid | -- | 🔲 | Needs grid |
| `justify-items` | Flex | -- | ➖ | Not defined for flex |
| `justify-items` | Grid | -- | 🔲 | |
| `justify-self` | Flex | -- | ➖ | Not defined for flex |
| `justify-self` | Grid | -- | 🔲 | |
| `align-content` | Flex | -- | 🔲 | Multi-line flex |
| `align-content` | Grid | -- | 🔲 | |
| `align-items` | Flex | `setFlex(id, "align_items", ...)` | ✅ | `start`, `center`, `end`, `stretch` |
| `align-items` | Grid | -- | 🔲 | |
| `align-self` | Flex | `setFlex(id, "align_self", ...)` | ✅ | `auto`, `start`, `center`, `end`, `stretch` |
| `align-self` | Grid | -- | 🔲 | |
| `place-content` | Shorthand | -- | 🔲 | |
| `place-items` | Shorthand | -- | 🔲 | |
| `place-self` | Shorthand | -- | 🔲 | |
| `baseline` alignment | -- | -- | 🔲 | `align-items: baseline` not implemented |

---

## 6. CSS Sizing Level 3

| Property / Value | Pulp Bridge | Status | Notes |
|------------------|-------------|--------|-------|
| `width` / `height` | `setFlex(id, "width"/"height", px)` | ✅ | Pixel values only |
| `min-width` / `min-height` | `setFlex(id, "min_width"/"min_height", px)` | ✅ | |
| `max-width` / `max-height` | `setFlex(id, "max_width"/"max_height", px)` | ✅ | |
| `width: auto` | Default behavior | ✅ | Flex-determined |
| `width: min-content` | -- | 🔲 | Requires intrinsic sizing pass |
| `width: max-content` | -- | 🔲 | |
| `width: fit-content` | -- | 🔲 | `fit-content(length)` function |
| `width: stretch` / `fill-available` | `flex_grow: 1` | ⚠️ | Achieved via flex-grow, not native keyword |
| `aspect-ratio` | -- | 🔲 | Auto aspect ratio from width/height |
| Percentage values (`width: 50%`) | -- | 🔲 | No percentage unit support; use flex-grow instead |
| `calc()` | -- | 🔲 | No CSS function evaluation |
| Viewport units (`vw`, `vh`, `vmin`, `vmax`) | -- | 🔲 | Could map to root view dimensions |

---

## 7. CSS Writing Modes Level 4

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `direction` | -- | 🔲 | `ltr`/`rtl`; needed for i18n |
| `writing-mode` | -- | ➖ | `horizontal-tb` only; vertical text not planned |
| `text-orientation` | -- | ➖ | Only relevant with vertical writing-mode |
| `unicode-bidi` | -- | ➖ | Complex bidi not in scope |
| Logical properties (`inline-start`, `block-end`) | -- | 🔲 | Useful for RTL support |

---

## 8. CSS Positioned Layout Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `position: static` | Default behavior | ✅ | Flex layout flow |
| `position: relative` | -- | 🔲 | Offset without removing from flow |
| `position: absolute` | Overlay paint queue | ⚠️ | ComboBox dropdown uses overlay; no general-purpose bridge |
| `position: fixed` | Overlay paint queue | ⚠️ | Same as absolute relative to root |
| `position: sticky` | -- | 🔲 | Needs ScrollView integration |
| `top` | -- | 🔲 | Only available with positioned layout |
| `right` | -- | 🔲 | |
| `bottom` | -- | 🔲 | |
| `left` | -- | 🔲 | |
| `inset` (shorthand) | -- | 🔲 | |
| `z-index` | -- | 🔲 | Overlay has implicit z-order; no explicit stacking |
| Stacking context | -- | ⚠️ | Overlays create implicit stacking; no general model |

---

## 9. CSS Containment Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `contain` | -- | 🔲 | Layout/paint containment could optimize rendering |
| `contain: layout` | -- | 🔲 | Skip subtree relayout |
| `contain: paint` | `setOverflow(id, "hidden")` | ⚠️ | Overflow hidden provides paint containment |
| `contain: size` | -- | 🔲 | |
| `contain: style` | -- | ➖ | No cascade model |
| `contain: strict` / `content` | -- | 🔲 | |
| `content-visibility` | -- | 🔲 | Auto-hide offscreen subtrees |
| `content-visibility: auto` | -- | 🔲 | Would benefit large scrolling lists |
| `contain-intrinsic-size` | -- | 🔲 | Placeholder size for content-visibility |

---

## 10. CSS Overflow Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `overflow` (shorthand) | `setOverflow(id, "visible"/"hidden")` | ⚠️ | Missing `scroll`, `auto`, `clip` |
| `overflow-x` | -- | 🔲 | Directional overflow |
| `overflow-y` | -- | 🔲 | |
| `overflow: visible` | `setOverflow(id, "visible")` | ✅ | Default for non-ScrollView |
| `overflow: hidden` | `setOverflow(id, "hidden")` | ✅ | Clips via `clip_rect` in `paint_all` |
| `overflow: clip` | -- | 🔲 | Like hidden but no scrollable overflow |
| `overflow: scroll` | `createScrollView()` | ✅ | Dedicated ScrollView widget |
| `overflow: auto` | -- | 🔲 | Auto scrollbar when content overflows |
| `text-overflow` | `setTextOverflow(id, "ellipsis")` | ✅ | `ellipsis` supported |
| `text-overflow: clip` | -- | ⚠️ | Default behavior (no ellipsis) |
| Scrollbar styling | -- | ⚠️ | Basic scrollbar in ScrollView; no CSS scrollbar-* properties |
| `scrollbar-width` | -- | 🔲 | `thin`, `none`, `auto` |
| `scrollbar-color` | -- | 🔲 | thumb + track colors |
| `scrollbar-gutter` | -- | 🔲 | Reserve space for scrollbar |

---

## 11. CSS Backgrounds and Borders Level 3

### Background

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `background-color` | `setBackground(id, hex)` | ✅ | Hex color string |
| `background-image` | -- | 🔲 | Need image loading system |
| `background-image: linear-gradient()` | Canvas: `set_fill_gradient_linear` | ⚠️ | Canvas API exists; not exposed as view background |
| `background-image: radial-gradient()` | Canvas: `set_fill_gradient_radial` | ⚠️ | Canvas API exists; not exposed as view background |
| `background-image: conic-gradient()` | -- | 🔲 | |
| `background-image: repeating-linear-gradient()` | -- | 🔲 | |
| `background-image: repeating-radial-gradient()` | -- | 🔲 | |
| `background-size` | -- | 🔲 | `cover`, `contain`, length values |
| `background-position` | -- | 🔲 | |
| `background-repeat` | -- | 🔲 | `repeat`, `no-repeat`, `repeat-x`, `repeat-y` |
| `background-attachment` | -- | ➖ | Scroll/fixed; not relevant in non-document context |
| `background-origin` | -- | 🔲 | `border-box`, `padding-box`, `content-box` |
| `background-clip` | -- | 🔲 | `border-box`, `padding-box`, `content-box`, `text` |
| `background` (shorthand) | -- | 🔲 | |
| Multiple backgrounds | -- | 🔲 | Layered background images |

### Border

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `border` (shorthand) | `setBorder(id, color, width, radius)` | ✅ | Single call for color + width + radius |
| `border-color` | Part of `setBorder()` | ✅ | |
| `border-width` | Part of `setBorder()` | ✅ | Uniform width |
| `border-style` | -- | ⚠️ | `solid` only; no `dashed`, `dotted`, `double`, `groove`, `ridge`, `inset`, `outset` |
| `border-radius` | Part of `setBorder()` | ✅ | Uniform radius |
| `border-top-left-radius` | -- | 🔲 | Per-corner radius |
| `border-top-right-radius` | -- | 🔲 | |
| `border-bottom-right-radius` | -- | 🔲 | |
| `border-bottom-left-radius` | -- | 🔲 | |
| `border-top` / `border-right` / `border-bottom` / `border-left` | -- | 🔲 | Per-side border |
| `border-top-color` / `border-right-color` / etc | -- | 🔲 | |
| `border-top-width` / `border-right-width` / etc | -- | 🔲 | |
| `border-top-style` / `border-right-style` / etc | -- | 🔲 | |
| `border-image` | -- | 🔲 | |
| `border-image-source` / `slice` / `width` / `outset` / `repeat` | -- | 🔲 | |

### Box Shadow

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `box-shadow` | `setBoxShadow(id, ox, oy, blur, spread, color)` | ✅ | Single shadow |
| `box-shadow` (multiple) | -- | 🔲 | Multiple comma-separated shadows |
| `box-shadow: inset` | -- | 🔲 | Inner shadow |

### Outline

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `outline` | -- | 🔲 | Focus rings are custom-painted |
| `outline-color` | -- | 🔲 | |
| `outline-width` | -- | 🔲 | |
| `outline-style` | -- | 🔲 | |
| `outline-offset` | -- | 🔲 | |

---

## 12. CSS Color Level 4

| Property / Value | Pulp Bridge | Status | Notes |
|------------------|-------------|--------|-------|
| `color` | `setTextColor(id, hex)` | ✅ | Hex string with parseHexColor |
| Hex `#RGB` | Supported in all color APIs | ✅ | 3-digit shorthand |
| Hex `#RRGGBB` | Supported | ✅ | |
| Hex `#RRGGBBAA` | Supported | ✅ | 8-digit with alpha |
| Hex `#RGBA` | -- | 🔲 | 4-digit shorthand with alpha |
| `rgb()` / `rgba()` | -- | 🔲 | Parse functional notation in bridge |
| `hsl()` / `hsla()` | -- | 🔲 | Parse in bridge |
| `hwb()` | -- | 🔲 | |
| `oklch()` | `OklchEngine` in JS (oklch.js) | ✅ | Via JS library, not native bridge |
| `oklab()` | -- | 🔲 | |
| `lab()` | -- | 🔲 | |
| `lch()` | -- | 🔲 | |
| `color()` function | -- | 🔲 | `color(display-p3 1 0 0)` etc |
| `color-mix()` | -- | 🔲 | Interpolation between colors |
| Named colors | -- | 🔲 | 148 CSS named colors |
| `transparent` | -- | 🔲 | `rgba(0,0,0,0)` equivalent |
| `currentColor` | -- | 🔲 | Inherited text color keyword |
| System colors | -- | ➖ | `Canvas`, `CanvasText`, etc; use theme tokens instead |
| `forced-color-adjust` | -- | ➖ | High contrast mode; not in scope |
| `color-scheme` | -- | 🔲 | `light`/`dark`; maps to `setTheme()` |
| `opacity` | `setOpacity(id, value)` | ✅ | 0.0-1.0, applied as layer alpha |

---

## 13. CSS Transforms Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `transform: scale()` | `setScale(id, value)` | ✅ | Uniform scale around center |
| `transform: scaleX()` / `scaleY()` | -- | 🔲 | Non-uniform scale |
| `transform: translate()` | -- | 🔲 | Canvas has `translate()` |
| `transform: translateX()` / `translateY()` | -- | 🔲 | |
| `transform: rotate()` | -- | 🔲 | Canvas has `rotate()` |
| `transform: skew()` / `skewX()` / `skewY()` | -- | 🔲 | |
| `transform: matrix()` | -- | 🔲 | 2D affine matrix |
| `transform: matrix3d()` | -- | ➖ | 3D not in scope |
| `transform: perspective()` | -- | ➖ | 3D not in scope |
| `transform: rotate3d()` | -- | ➖ | 3D not in scope |
| `transform-origin` | -- | 🔲 | Currently always center |
| `transform-box` | -- | 🔲 | |
| `transform-style` | -- | ➖ | `flat` only; no 3D preserve |
| `perspective` (property) | -- | ➖ | 3D perspective not in scope |
| `perspective-origin` | -- | ➖ | |
| `backface-visibility` | -- | ➖ | No 3D transforms |
| Individual transform: `translate` | -- | 🔲 | CSS individual transform properties |
| Individual transform: `rotate` | -- | 🔲 | |
| Individual transform: `scale` | `setScale(id, value)` | ✅ | Already supported |

---

## 14. CSS Transitions Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `transition-property` | -- | ⚠️ | All animated properties implicit; no per-property control |
| `transition-duration` | `setTransitionDuration(id, seconds)` | ✅ | |
| `transition-timing-function` | `animate()` easing param | ✅ | 11 easing functions: linear, ease-in/out quad/cubic, expo, elastic, bounce |
| `transition-delay` | -- | 🔲 | |
| `transition` (shorthand) | -- | 🔲 | |
| `cubic-bezier()` custom timing | -- | 🔲 | Only named easings; no arbitrary cubic-bezier |
| `steps()` timing function | -- | 🔲 | Stepped animation |
| `transition-behavior` | -- | ➖ | |

### Available Easing Functions

| CSS Equivalent | Pulp Name | Status |
|----------------|-----------|--------|
| `linear` | `linear` | ✅ |
| `ease-in` (approx) | `ease_in_quad` | ✅ |
| `ease-out` (approx) | `ease_out_quad` | ✅ |
| `ease-in-out` (approx) | `ease_in_out_quad` | ✅ |
| -- | `ease_in_cubic` | ✅ |
| -- | `ease_out_cubic` | ✅ |
| -- | `ease_in_out_cubic` | ✅ |
| -- | `ease_in_expo` | ✅ |
| -- | `ease_out_expo` | ✅ |
| -- | `ease_out_elastic` | ✅ |
| -- | `ease_out_bounce` | ✅ |
| `cubic-bezier(a,b,c,d)` | -- | 🔲 |
| `steps(n, jump-start)` | -- | 🔲 |

---

## 15. CSS Animations Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `@keyframes` | -- | 🔲 | Need keyframe definition system |
| `animation-name` | -- | 🔲 | |
| `animation-duration` | `animate()` duration param | ⚠️ | Single-shot via `AnimationManager` |
| `animation-timing-function` | `animate()` easing param | ⚠️ | Same easings as transitions |
| `animation-delay` | -- | 🔲 | |
| `animation-iteration-count` | -- | 🔲 | No looping; `infinite` not supported |
| `animation-direction` | -- | 🔲 | `normal`, `reverse`, `alternate` |
| `animation-fill-mode` | -- | 🔲 | `forwards`, `backwards`, `both` |
| `animation-play-state` | -- | 🔲 | `running`, `paused` |
| `animation` (shorthand) | -- | 🔲 | |

### Current Animation API

```
animate(id, property, from, to, duration, easing)
setMotionToken(id, token)
getMotionToken(id) -> token
```

`ValueAnimation` supports target-chasing with `animate_to(target, duration, easing)`. The `AnimationManager` supports multiple concurrent tweens with callbacks. Keyframe sequences and looping are not yet implemented.

---

## 16. CSS Filter Effects Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `filter: blur()` | -- | ⚠️ | Only via `draw_blurred_backdrop` (backdrop, not per-element) |
| `filter: brightness()` | -- | 🔲 | Skia `SkColorFilter` can do this |
| `filter: contrast()` | -- | 🔲 | |
| `filter: grayscale()` | -- | 🔲 | |
| `filter: hue-rotate()` | -- | 🔲 | |
| `filter: invert()` | -- | 🔲 | |
| `filter: opacity()` | `setOpacity()` | ⚠️ | Opacity is separate; not a filter chain member |
| `filter: saturate()` | -- | 🔲 | |
| `filter: sepia()` | -- | 🔲 | |
| `filter: drop-shadow()` | -- | 🔲 | Use `setBoxShadow` as alternative |
| `filter` (chained) | -- | 🔲 | Multiple filters in sequence |
| `backdrop-filter: blur()` | `draw_blurred_backdrop()` | ✅ | With tint color, corner radius |
| `backdrop-filter: brightness()` | -- | 🔲 | |
| `backdrop-filter` (other functions) | -- | 🔲 | |

---

## 17. CSS Compositing and Blending Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `mix-blend-mode` | Canvas: `set_blend_mode()` | ⚠️ | 16 modes defined in Canvas API; not exposed via view bridge |
| `isolation` | -- | 🔲 | Create isolated stacking context |
| `background-blend-mode` | -- | 🔲 | Blend between background layers |

### Canvas Blend Modes Available

`normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `color_dodge`, `color_burn`, `hard_light`, `soft_light`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`

---

## 18. CSS Masking Level 1

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `clip-path` | Canvas: `clip_rect()` | ⚠️ | Rectangle clip only; no polygon, circle, ellipse, path, SVG |
| `clip-path: inset()` | -- | 🔲 | |
| `clip-path: circle()` | -- | 🔲 | |
| `clip-path: ellipse()` | -- | 🔲 | |
| `clip-path: polygon()` | -- | 🔲 | |
| `clip-path: path()` | -- | 🔲 | SVG path syntax |
| `mask-image` | -- | 🔲 | |
| `mask-mode` | -- | 🔲 | |
| `mask-repeat` | -- | 🔲 | |
| `mask-position` | -- | 🔲 | |
| `mask-size` | -- | 🔲 | |
| `mask-composite` | -- | 🔲 | |
| `mask` (shorthand) | -- | 🔲 | |

---

## 19. CSS Fonts Level 4

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `font-family` | -- | ⚠️ | Hardcoded "Inter"; Theme strings could hold family; no bridge setter |
| `font-size` | `setFontSize(id, px)` | ✅ | |
| `font-weight` | `setFontWeight(id, 100-900)` | ✅ | Numeric weight |
| `font-style` | `setFontStyle(id, "normal"/"italic")` | ✅ | 0=normal, 1=italic |
| `font-stretch` | -- | 🔲 | `condensed`, `expanded`, etc |
| `font-variant` | -- | 🔲 | Small-caps, ligatures, numerals |
| `font-variant-caps` | -- | 🔲 | `small-caps`, `all-small-caps` |
| `font-variant-ligatures` | -- | 🔲 | |
| `font-variant-numeric` | -- | 🔲 | Tabular figures, oldstyle, etc |
| `font-variant-east-asian` | -- | ➖ | |
| `font-feature-settings` | -- | 🔲 | OpenType feature tags |
| `font-variation-settings` | -- | 🔲 | Variable font axes |
| `font-optical-sizing` | -- | 🔲 | |
| `font-size-adjust` | -- | 🔲 | Normalize x-height across fonts |
| `font-synthesis` | -- | 🔲 | Auto-generate bold/italic |
| `font-kerning` | -- | 🔲 | `auto`, `normal`, `none` |
| `font-language-override` | -- | ➖ | |
| `font` (shorthand) | -- | 🔲 | |
| `@font-face` | -- | 🔲 | Custom font loading |
| `font-display` | -- | 🔲 | `auto`, `block`, `swap`, `fallback`, `optional` |
| `unicode-range` | -- | 🔲 | Font subsetting |
| `line-height` | `setLineHeight(id, px)` | ✅ | Pixel value; 0=auto (1.4x) |
| `letter-spacing` | `setLetterSpacing(id, px)` | ✅ | Pixel value |

---

## 20. CSS Text Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `text-align` | `setTextAlign(id, "left"/"center"/"right")` | ✅ | |
| `text-align: justify` | -- | 🔲 | |
| `text-align-last` | -- | 🔲 | |
| `text-indent` | -- | 🔲 | |
| `text-transform` | -- | 🔲 | `uppercase`, `lowercase`, `capitalize`, `full-width` |
| `white-space` | -- | 🔲 | `normal`, `nowrap`, `pre`, `pre-wrap`, `pre-line`, `break-spaces` |
| `word-break` | -- | 🔲 | `normal`, `break-all`, `keep-all` |
| `overflow-wrap` / `word-wrap` | -- | 🔲 | `normal`, `break-word`, `anywhere` |
| `line-break` | -- | 🔲 | |
| `hyphens` | -- | 🔲 | `none`, `manual`, `auto` |
| `word-spacing` | -- | 🔲 | |
| `tab-size` | -- | 🔲 | |
| `text-wrap` | -- | 🔲 | `wrap`, `nowrap`, `balance`, `pretty` |
| `text-wrap-mode` | -- | 🔲 | |
| `text-wrap-style` | -- | 🔲 | |

---

## 21. CSS Inline Layout Level 3

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `vertical-align` | -- | 🔲 | Not applicable in flex; relevant for inline text |
| `line-height` | `setLineHeight(id, px)` | ✅ | |
| `initial-letter` | -- | ➖ | Drop caps; not relevant |
| `dominant-baseline` | -- | 🔲 | SVG/text baseline |
| `alignment-baseline` | -- | 🔲 | |
| `baseline-source` | -- | 🔲 | |

---

## 22. CSS Text Decoration Level 4

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `text-decoration` (shorthand) | -- | 🔲 | |
| `text-decoration-line` | -- | 🔲 | `underline`, `overline`, `line-through`, `none` |
| `text-decoration-color` | -- | 🔲 | |
| `text-decoration-style` | -- | 🔲 | `solid`, `double`, `dotted`, `dashed`, `wavy` |
| `text-decoration-thickness` | -- | 🔲 | |
| `text-underline-offset` | -- | 🔲 | |
| `text-underline-position` | -- | 🔲 | |
| `text-decoration-skip-ink` | -- | 🔲 | |
| `text-emphasis` | -- | 🔲 | |
| `text-emphasis-style` | -- | 🔲 | |
| `text-emphasis-color` | -- | 🔲 | |
| `text-emphasis-position` | -- | 🔲 | |
| `text-shadow` | -- | 🔲 | |
| `text-overflow` | `setTextOverflow(id, "ellipsis")` | ✅ | `ellipsis` and `clip` (default) |

---

## 23. CSS Basic User Interface Level 4

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `cursor` | `setCursor(id, ...)` | ✅ | `default`, `pointer`, `crosshair`, `text`, `grab`, `grabbing`, `not-allowed` |
| `cursor: none` | -- | 🔲 | |
| `cursor: wait` / `progress` | -- | 🔲 | |
| `cursor: move` | -- | 🔲 | |
| `cursor: col-resize` / `row-resize` | -- | 🔲 | |
| `cursor: n-resize` / `e-resize` / etc | -- | 🔲 | |
| `cursor: url()` (custom) | -- | 🔲 | |
| `pointer-events` | -- | 🔲 | `auto`, `none` (hit-test passthrough) |
| `user-select` | -- | 🔲 | `auto`, `none`, `text`, `all` |
| `resize` | -- | 🔲 | `none`, `both`, `horizontal`, `vertical` |
| `caret-color` | -- | 🔲 | TextEditor uses accent color from theme |
| `caret-shape` | -- | 🔲 | `auto`, `bar`, `block`, `underscore` |
| `appearance` | -- | ➖ | Native widget appearance; not relevant |
| `touch-action` | -- | 🔲 | `auto`, `none`, `pan-x`, `pan-y`, `manipulation` |
| `accent-color` | -- | 🔲 | Theme token equivalent |
| `color-scheme` | `setTheme("dark"/"light")` | ✅ | Via theme system |
| `nav-up` / `nav-down` / `nav-left` / `nav-right` | -- | ⚠️ | `focus_next`/`focus_prev` provide basic tab navigation |

---

## 24. CSS Pseudo-Classes (Selectors Level 4)

Pulp has no selector/cascade system. Pseudo-class behavior is achieved through event callbacks and widget state.

| Pseudo-Class | Pulp Equivalent | Status | Notes |
|-------------|-----------------|--------|-------|
| `:hover` | `registerHover(id)` + `on(id, "mouseenter"/"mouseleave")` | ✅ | `on_mouse_enter`/`on_mouse_leave` callbacks |
| `:active` | -- | 🔲 | No mousedown-held state tracking in bridge |
| `:focus` | `on_focus_changed` | ✅ | TextEditor, focusable widgets |
| `:focus-visible` | Focus ring logic | ✅ | Keyboard-triggered focus ring |
| `:focus-within` | -- | 🔲 | Parent has focused descendant |
| `:disabled` | -- | 🔲 | No `setEnabled()`; workaround: `setOpacity(0.5)` + remove handlers |
| `:enabled` | -- | 🔲 | |
| `:checked` | -- | ⚠️ | Toggle/Checkbox `is_on()`/`is_checked()` but no bridge query |
| `:indeterminate` | -- | 🔲 | Checkbox/progress third state |
| `:placeholder-shown` | -- | ⚠️ | TextEditor has `placeholder` but no state query from JS |
| `:empty` | -- | ➖ | No selector model |
| `:first-child` / `:last-child` | -- | ➖ | No selector model |
| `:nth-child()` | -- | ➖ | No selector model |
| `:not()` | -- | ➖ | No selector model |
| `:is()` / `:where()` | -- | ➖ | No selector model |
| `:has()` | -- | ➖ | No selector model |
| `::before` / `::after` | -- | ➖ | No pseudo-elements; use child views |
| `::placeholder` | -- | ➖ | Style via TextEditor properties |
| `::selection` | -- | 🔲 | TextEditor selection uses theme accent |
| `::marker` | -- | ➖ | No list items |
| `::scrollbar` / `::scrollbar-thumb` etc | -- | 🔲 | ScrollView has hardcoded scrollbar style |

---

## 25. Scrolling (CSSOM View + Scroll Snap + Overscroll)

### CSSOM View

| API / Property | Pulp Bridge | Status | Notes |
|----------------|-------------|--------|-------|
| `scrollTop` | `ScrollView::scroll_y()` | ⚠️ | C++ API exists; no JS bridge getter |
| `scrollLeft` | `ScrollView::scroll_x()` | ⚠️ | |
| `scrollTo(x, y)` | `ScrollView::set_scroll(x, y)` | ⚠️ | C++ API; no JS bridge |
| `scrollBy(dx, dy)` | `ScrollView::scroll_by(dx, dy)` | ⚠️ | C++ API with smooth animation |
| `scrollIntoView()` | -- | 🔲 | |
| `scrollWidth` / `scrollHeight` | `setScrollContentSize(id, w, h)` | ⚠️ | Set only; no getter from JS |
| `clientWidth` / `clientHeight` | -- | 🔲 | |
| `getBoundingClientRect()` | -- | 🔲 | |
| `offsetWidth` / `offsetHeight` | -- | 🔲 | |
| `offsetTop` / `offsetLeft` | -- | 🔲 | |

### Scroll Snap

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `scroll-snap-type` | -- | 🔲 | `x`/`y`/`both` + `mandatory`/`proximity` |
| `scroll-snap-align` | -- | 🔲 | `start`, `center`, `end` |
| `scroll-snap-stop` | -- | 🔲 | `normal`, `always` |
| `scroll-padding` | -- | 🔲 | |
| `scroll-margin` | -- | 🔲 | |

### Scroll Behavior

| Property | Pulp Bridge | Status | Notes |
|----------|-------------|--------|-------|
| `scroll-behavior` | -- | ⚠️ | `smooth` is default in ScrollView (ValueAnimation); no `auto` (instant) option via bridge |
| `overscroll-behavior` | -- | 🔲 | `auto`, `contain`, `none` |
| `overscroll-behavior-x` / `y` | -- | 🔲 | |

---

## 26. CSS Custom Properties and Variables

| Feature | Pulp Equivalent | Status | Notes |
|---------|-----------------|--------|-------|
| `--custom-property` declaration | Theme token system | ⚠️ | `Theme::colors`, `Theme::dimensions`, `Theme::strings` |
| `var(--name)` | `resolve_color(name)` / `resolve_dimension(name)` | ⚠️ | C++ API walks parent chain; no `var()` syntax in JS |
| `var(--name, fallback)` | `resolve_color(name, fallback)` | ⚠️ | Fallback supported in C++ |
| Inheritance down the tree | Parent chain walk in `resolve_color`/`resolve_dimension` | ✅ | Tokens inherit through view hierarchy |
| `@property` | -- | 🔲 | Typed custom properties |
| `setTheme(name)` | `setTheme("dark"/"light"/"pro_audio")` | ✅ | Preset themes |
| `applyTokenDiff(json)` | `applyTokenDiff(json)` | ✅ | Merge token overrides |
| `getThemeJson()` | `getThemeJson()` | ✅ | Serialize current theme |

---

## 27. Selectors and Cascade

Pulp intentionally has NO cascade, NO specificity, NO selector matching. This is a fundamental architectural decision, not a gap.

| CSS Concept | Pulp Replacement | Notes |
|-------------|------------------|-------|
| Selectors (`div > p.class`) | Direct widget ID references | `widget("myLabel")` |
| Specificity | N/A | No competing rules |
| Cascade order | N/A | Last `set*` call wins |
| `!important` | N/A | Direct property setting |
| Class-based styling (`.btn-primary`) | Theme tokens + JS functions | Reusable style functions in JS |
| Media queries | -- | 🔲 Could respond to root view size changes |
| `@container` queries | -- | 🔲 |
| `@layer` | -- | ➖ |
| `@import` | JS `import` / `require` | ✅ Via QuickJS module system |
| `@supports` | -- | ➖ Feature detection not needed; single rendering engine |

---

## 28. HTML Elements via Widgets

### Block / Container Elements

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<div>` | View | `createRow()` / `createCol()` | ✅ | Flex containers |
| `<section>` / `<article>` / `<aside>` | View | `createRow()` / `createCol()` | ✅ | Semantic equivalents are just containers |
| `<header>` / `<footer>` / `<nav>` / `<main>` | View | `createRow()` / `createCol()` | ✅ | |
| `<span>` / `<p>` / `<h1>`-`<h6>` | Label | `createLabel(id, text, parent)` | ✅ | Font size/weight via `setFontSize`/`setFontWeight` |

### Form Elements

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<input type="text">` | TextEditor | `createTextEditor(id, parent)` | ✅ | Full editing: selection, clipboard, undo/redo |
| `<input type="number">` | TextEditor | `createTextEditor` + `numeric_only=true` | ✅ | |
| `<input type="password">` | TextEditor | `createTextEditor` + `password_mode=true` | ✅ | |
| `<input type="range">` | Fader | `createFader(id, parent)` | ✅ | Vertical/horizontal |
| `<input type="checkbox">` | Checkbox | `createCheckbox(id, parent)` | ✅ | |
| `<input type="radio">` | -- | -- | 🔲 | Use mutually-exclusive Toggles |
| `<input type="color">` | -- | -- | 🔲 | Color picker widget |
| `<input type="date">` / `<input type="time">` | -- | -- | 🔲 | |
| `<input type="file">` | -- | -- | 🔲 | File picker |
| `<input type="search">` | TextEditor | `createTextEditor` | ⚠️ | No search icon/clear button |
| `<textarea>` | TextEditor | `createTextEditor` + `multi_line=true` | ✅ | |
| `<select>` | ComboBox | `createCombo(id, parent)` | ✅ | Overlay dropdown |
| `<select multiple>` | ListBox | -- | ⚠️ | C++ ListBox exists; no bridge `createListBox` |
| `<button>` | ToggleButton / Label+click | `createToggleButton(id, parent)` | ✅ | |
| `<form>` | -- | -- | ➖ | No form submission model |
| `<label>` | Label | `createLabel()` | ✅ | No `for` attribute binding |
| `<fieldset>` / `<legend>` | Panel | `createPanel()` | ⚠️ | Styled container without semantic grouping |
| `<output>` | Label | `createLabel()` | ✅ | |
| `<datalist>` | -- | -- | 🔲 | Autocomplete suggestions |

### Interactive Elements

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<details>` / `<summary>` | -- | -- | 🔲 | Collapsible section; TabPanel is partial equivalent |
| `<dialog>` | CallOutBox | -- | ⚠️ | C++ CallOutBox exists; no bridge API |
| `<menu>` | -- | -- | 🔲 | Context menu |

### Media Elements

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<img>` | -- | -- | 🔲 | Need image loading + rendering |
| `<svg>` | Icon + Canvas path API | `drawPath()` | ⚠️ | Basic vector icons; drawPath TODO incomplete |
| `<canvas>` | CanvasWidget | `createCanvas(id, parent)` | ⚠️ | Records draw commands; limited JS draw API |
| `<video>` | -- | -- | ➖ | Not in scope for audio plugin UI |
| `<audio>` | -- | -- | ➖ | Audio handled by plugin processor |
| `<picture>` | -- | -- | 🔲 | Responsive images |

### Progress / Meter

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<progress>` | ProgressBar | `createProgress(id, parent)` | ✅ | 0-1 range; negative = indeterminate |
| `<meter>` | Meter | `createMeter(id, orient, parent)` | ✅ | Audio level meter with ballistics |

### Other

| HTML Element | Pulp Widget | Bridge API | Status | Notes |
|-------------|------------|------------|--------|-------|
| `<hr>` | -- | -- | 🔲 | Horizontal rule; easy to build with View+bg |
| `<br>` | -- | -- | ➖ | No inline text flow |
| `<table>` / `<tr>` / `<td>` | -- | -- | 🔲 | Use grid when available |
| `<ul>` / `<ol>` / `<li>` | -- | -- | ⚠️ | Use Col with Labels; no list markers |
| `<pre>` / `<code>` | Label | `createLabel()` | ⚠️ | No monospace font selection via bridge |
| `<iframe>` | -- | -- | ➖ | No embedded documents |
| `<slot>` | -- | -- | ➖ | No shadow DOM |

### Pulp-Specific Widgets (No HTML Equivalent)

| Widget | Bridge API | Purpose |
|--------|-----------|---------|
| Knob | `createKnob(id, parent)` | Rotary control for audio parameters |
| Toggle | `createToggle(id, parent)` | On/off switch |
| XYPad | `createXYPad(id, parent)` | 2D parameter surface |
| WaveformView | `createWaveform(id, parent)` | Audio waveform display |
| SpectrumView | `createSpectrum(id, parent)` | FFT spectrum display |
| Panel | `createPanel(id, parent)` | Themed container with tokens |
| ScrollView | `createScrollView(id, parent)` | Scrollable container |
| TabPanel | -- (C++ only) | Tabbed interface |
| Tooltip | -- (C++ only) | Hover tooltip |
| CallOutBox | -- (C++ only) | Alert/confirm dialog |
| ListBox | -- (C++ only) | Selectable list |

---

## 29. DOM Events via Bridge

### Mouse / Pointer Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `click` | `registerClick(id)` + `on(id, "click")` | ✅ | Fires on mousedown |
| `dblclick` | `MouseEvent::click_count == 2` | ⚠️ | C++ level; not dispatched to JS |
| `mousedown` | `on_mouse_event` (is_down=true) | ⚠️ | C++ level; not dispatched as named JS event |
| `mouseup` | `on_mouse_event` (is_down=false) | ⚠️ | C++ level |
| `mousemove` | -- | 🔲 | No continuous mouse tracking in bridge |
| `mouseenter` | `registerHover(id)` + `on(id, "mouseenter")` | ✅ | |
| `mouseleave` | `registerHover(id)` + `on(id, "mouseleave")` | ✅ | |
| `mouseover` / `mouseout` | -- | 🔲 | Bubbling variants |
| `contextmenu` | -- | 🔲 | Right-click |
| `wheel` | `ScrollView::on_mouse_event` (is_wheel) | ✅ | Handled internally by ScrollView |
| `pointerdown` / `pointerup` / `pointermove` | -- | 🔲 | Unified pointer events |
| `pointerenter` / `pointerleave` | -- | 🔲 | |
| `pointercancel` | -- | 🔲 | Touch cancellation |
| `gotpointercapture` / `lostpointercapture` | -- | 🔲 | Pointer capture API |
| `touchstart` / `touchmove` / `touchend` / `touchcancel` | `MouseEvent::pointer_id` / `isTouch()` | ⚠️ | Touch mapped to mouse events with pointer_id > 0 |

### Keyboard Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `keydown` | `forward_key_event` + `on("__global__", "keydown")` | ⚠️ | Global only; not per-widget from JS |
| `keyup` | `forward_key_event` (is_down=false) | ⚠️ | C++ level; not dispatched to JS |
| `keypress` | -- | ➖ | Deprecated in DOM |
| `input` (from text) | `on(id, "change")` | ✅ | TextEditor fires on each change |
| `compositionstart` / `compositionend` | -- | 🔲 | IME composition events |

### Focus Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `focus` | `on_focus_changed(true)` | ✅ | TextEditor, focusable widgets |
| `blur` | `on_focus_changed(false)` | ✅ | |
| `focusin` / `focusout` | -- | 🔲 | Bubbling variants |

### Form Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `change` | `on(id, "change")` | ✅ | Fader, ComboBox, TextEditor, Toggle |
| `input` | `on(id, "change")` | ✅ | Same as change in Pulp |
| `submit` | `on(id, "return")` | ✅ | TextEditor return key |
| `reset` | -- | ➖ | No form model |
| `toggle` | `on(id, "toggle")` | ✅ | Toggle widget state change |

### Scroll Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `scroll` | ScrollView internal | ✅ | Handled internally; no JS event dispatch |
| `scrollend` | -- | 🔲 | |

### Drag & Drop Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `drag` / `dragstart` / `dragend` | -- | 🔲 | `drag_drop.hpp` exists but limited |
| `dragenter` / `dragleave` / `dragover` | -- | 🔲 | |
| `drop` | -- | 🔲 | |

### Custom / Lifecycle Events

| DOM Event | Pulp Bridge | Status | Notes |
|-----------|-------------|--------|-------|
| `DOMContentLoaded` | Script loaded = ready | ✅ | Script executes synchronously on load |
| `resize` | `on_resized()` | ⚠️ | C++ callback; not dispatched to JS |
| `load` / `unload` | -- | 🔲 | |
| `beforeunload` | -- | ➖ | |
| Custom events (`dispatchEvent`) | `__dispatch__` | ⚠️ | Internal dispatch; no user-defined events |
| `AnimationEvent` | -- | 🔲 | `animationstart`, `animationend`, `animationiteration` |
| `TransitionEvent` | -- | 🔲 | `transitionstart`, `transitionend`, `transitioncancel` |

---

## 30. Canvas 2D API (CanvasRenderingContext2D)

### State

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `save()` | `Canvas::save()` | ✅ | |
| `restore()` | `Canvas::restore()` | ✅ | |
| `reset()` | -- | 🔲 | |
| `isContextLost()` | -- | ➖ | GPU context managed by Dawn |

### Transform

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `translate(x, y)` | `Canvas::translate()` | ✅ | |
| `scale(sx, sy)` | `Canvas::scale()` | ✅ | |
| `rotate(angle)` | `Canvas::rotate()` | ✅ | Radians |
| `transform(a, b, c, d, e, f)` | -- | 🔲 | Concatenate arbitrary 2D matrix |
| `setTransform(a, b, c, d, e, f)` | -- | 🔲 | Replace current transform |
| `getTransform()` | -- | 🔲 | Returns DOMMatrix |
| `resetTransform()` | -- | 🔲 | |

### Compositing

| Property | Pulp Bridge / Canvas | Status | Notes |
|----------|---------------------|--------|-------|
| `globalAlpha` | `Canvas::set_opacity()` | ✅ | |
| `globalCompositeOperation` | `Canvas::set_blend_mode()` | ⚠️ | 16 modes defined; not all mapped to CSS names |

### Fill / Stroke Style

| Property / Method | Pulp Bridge / Canvas | Status | Notes |
|-------------------|---------------------|--------|-------|
| `fillStyle` (color) | `Canvas::set_fill_color()` | ✅ | |
| `fillStyle` (gradient) | `set_fill_gradient_linear` / `set_fill_gradient_radial` | ✅ | |
| `fillStyle` (pattern) | -- | 🔲 | Image pattern fill |
| `strokeStyle` (color) | `Canvas::set_stroke_color()` | ✅ | |
| `strokeStyle` (gradient) | -- | 🔲 | Stroke gradient |
| `lineWidth` | `Canvas::set_line_width()` | ✅ | |
| `lineCap` | `Canvas::set_line_cap()` | ✅ | `butt`, `round`, `square` |
| `lineJoin` | `Canvas::set_line_join()` | ✅ | `miter`, `round`, `bevel` |
| `miterLimit` | -- | 🔲 | |
| `lineDashOffset` | -- | 🔲 | |
| `setLineDash()` / `getLineDash()` | -- | 🔲 | Dashed line patterns |
| `createLinearGradient()` | `Canvas::set_fill_gradient_linear()` | ✅ | |
| `createRadialGradient()` | `Canvas::set_fill_gradient_radial()` | ✅ | |
| `createConicGradient()` | -- | 🔲 | |
| `createPattern()` | -- | 🔲 | |

### Shadow

| Property | Pulp Bridge / Canvas | Status | Notes |
|----------|---------------------|--------|-------|
| `shadowBlur` | -- | 🔲 | View-level `setBoxShadow` is separate |
| `shadowColor` | -- | 🔲 | |
| `shadowOffsetX` / `shadowOffsetY` | -- | 🔲 | |

### Path

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `beginPath()` | `Canvas::begin_path()` + JS `beginPath()` | ✅ | |
| `moveTo(x, y)` | `Canvas::move_to()` | ✅ | |
| `lineTo(x, y)` | `Canvas::line_to()` | ✅ | |
| `quadraticCurveTo(cpx, cpy, x, y)` | `Canvas::quad_to()` | ✅ | |
| `bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y)` | `Canvas::cubic_to()` | ✅ | |
| `closePath()` | `Canvas::close_path()` | ✅ | |
| `arc(x, y, r, startAngle, endAngle, ccw)` | `Canvas::stroke_arc()` | ⚠️ | Stroke-only arc; no fill arc |
| `arcTo(x1, y1, x2, y2, r)` | -- | 🔲 | |
| `ellipse(x, y, rx, ry, rotation, start, end, ccw)` | -- | 🔲 | |
| `rect(x, y, w, h)` | -- | 🔲 | Add rect subpath (not fill/stroke) |
| `roundRect(x, y, w, h, radii)` | -- | 🔲 | |
| `fill()` | `Canvas::fill_current_path()` | ✅ | |
| `stroke()` | `Canvas::stroke_current_path()` | ✅ | |
| `clip()` | `Canvas::clip_rect()` | ⚠️ | Rect clip only; no path clip |
| `isPointInPath()` | -- | 🔲 | |
| `isPointInStroke()` | -- | 🔲 | |
| `Path2D` object | -- | 🔲 | Reusable path objects |

### Rectangles

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `fillRect(x, y, w, h)` | `canvasRect(id, x, y, w, h, hex)` + `Canvas::fill_rect()` | ✅ | |
| `strokeRect(x, y, w, h)` | `Canvas::stroke_rect()` | ✅ | C++ API; not in CanvasWidget bridge |
| `clearRect(x, y, w, h)` | `canvasClear(id)` | ⚠️ | Clears entire canvas, not a rect region |

### Text

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `fillText(text, x, y)` | `Canvas::fill_text()` | ✅ | |
| `strokeText(text, x, y)` | -- | 🔲 | |
| `measureText(text)` | `measureText(text, fontSize)` | ✅ | Returns `{width, ascent, descent, lineHeight}` |
| `font` property | `Canvas::set_font(family, size)` | ✅ | |
| `textAlign` | `Canvas::set_text_align()` | ✅ | `left`, `center`, `right` |
| `textBaseline` | -- | ⚠️ | Enum exists (`top`, `middle`, `bottom`); not fully wired |
| `direction` | -- | 🔲 | `ltr`, `rtl`, `inherit` |
| `letterSpacing` | -- | ⚠️ | Available on Label widget, not in Canvas text API |
| `wordSpacing` | -- | 🔲 | |
| `fontKerning` | -- | 🔲 | |
| `textRendering` | -- | ➖ | GPU rendering always optimized |

### Image Drawing

| Method | Pulp Bridge / Canvas | Status | Notes |
|--------|---------------------|--------|-------|
| `drawImage(image, dx, dy)` | -- | 🔲 | Need image loading |
| `drawImage(image, dx, dy, dw, dh)` | -- | 🔲 | Scaled drawing |
| `drawImage(image, sx, sy, sw, sh, dx, dy, dw, dh)` | -- | 🔲 | Source rect |
| `createImageData()` | -- | 🔲 | |
| `getImageData()` | -- | 🔲 | Pixel readback |
| `putImageData()` | -- | 🔲 | Pixel upload |
| `imageSmoothingEnabled` | -- | 🔲 | |
| `imageSmoothingQuality` | -- | 🔲 | |

### Filters (Canvas 2D)

| Property | Pulp Bridge / Canvas | Status | Notes |
|----------|---------------------|--------|-------|
| `filter` | -- | 🔲 | CSS filter string on canvas context |

### Additional Canvas Features

| Feature | Pulp Bridge / Canvas | Status | Notes |
|---------|---------------------|--------|-------|
| `toDataURL()` / `toBlob()` | -- | 🔲 | Export canvas as image |
| `OffscreenCanvas` | -- | 🔲 | |
| `ImageBitmap` | -- | 🔲 | |
| Hit regions | -- | ➖ | Deprecated in DOM |

### Pulp-Specific Canvas Extensions

| Feature | Canvas API | Status | Notes |
|---------|-----------|--------|-------|
| SDF shapes | `draw_sdf_shape()` | ✅ | GPU-accelerated signed distance field rendering |
| Bloom/glow | `set_bloom()` | ✅ | Post-processing effect |
| Waveform rendering | `draw_waveform()` | ✅ | GPU-accelerated audio waveform |
| Blurred backdrop | `draw_blurred_backdrop()` | ✅ | iOS-style blur effect |
| SVG path string | `drawPath(id, svgPath, fill, stroke, lineWidth)` | 🔲 | Bridge exists but TODO in implementation |
| Shader compilation | `compileShader(sksl)` | ⚠️ | Validates SkSL; does not apply |

---

## 31. CSS Variables and Theming

### Pulp Token System (CSS Custom Properties Equivalent)

| Feature | Pulp API | Status | Notes |
|---------|----------|--------|-------|
| Define color token | `Theme::colors["token.name"] = color` | ✅ | |
| Define dimension token | `Theme::dimensions["token.name"] = value` | ✅ | Spacing, radius, font size |
| Define string token | `Theme::strings["token.name"] = value` | ✅ | Font family, icon names |
| Resolve color (with inheritance) | `resolve_color("token.name", fallback)` | ✅ | Walks parent chain |
| Resolve dimension (with inheritance) | `resolve_dimension("token.name", fallback)` | ✅ | Walks parent chain |
| Switch theme preset | `setTheme("dark"/"light"/"pro_audio")` | ✅ | |
| Override individual tokens | `applyTokenDiff(json)` | ✅ | Merge JSON token overrides |
| Read current theme | `getThemeJson()` | ✅ | Full theme serialization |
| Load theme from JSON | `Theme::from_json(json)` | ✅ | |
| Per-subtree theming | `view->set_theme(theme)` | ✅ | Theme scoped to subtree |

### Comparison: CSS Variables vs Pulp Tokens

| CSS Variables | Pulp Tokens | Notes |
|---------------|-------------|-------|
| `--color-primary: #3b82f6` | `theme.colors["color.primary"] = {59,130,246}` | Same concept, different syntax |
| `var(--color-primary)` | `resolve_color("color.primary")` | Pulp resolves at use site |
| Inheritance through DOM | Inheritance through view tree | Same parent-chain walk |
| `:root { --x: 10px }` | Root theme | Apply to root view |
| Scope via selector | Scope via `set_theme()` on subtree | More explicit |
| `@media (prefers-color-scheme: dark)` | `setTheme("dark")` | Explicit theme switching |

---

## 32. Architectural Notes

### Layout Engine

Pulp uses a custom flex layout engine (`View::layout_children()`) that implements a subset of the CSS Flexbox specification. Key characteristics:

- **All views are flex containers.** There is no block, inline, or table layout. Every view arranges its children along a main axis (row or column).
- **Single-pass layout.** The current engine does a single pass for simple cases. Multi-pass layout (for wrapping, intrinsic sizing, nested flex) will need enhancement.
- **No percentage units.** All dimensions are in device-independent pixels. Percentage-based sizing is achieved through `flex-grow`.
- **`border-box` only.** There is no `content-box` model; padding is always inside the specified width/height.
- **No margin collapse.** Flex containers do not collapse margins per the W3C flex spec.

### Render Pipeline

```
JS script execution (QuickJS)
  -> Widget tree mutations via bridge
    -> Layout pass (FlexStyle resolution)
      -> Paint pass (View::paint_all)
        -> Canvas commands (Skia Graphite via Dawn/WebGPU)
          -> GPU submission
```

- **Frame clock** drives animations at display refresh rate via `FrameClock`
- **Recording canvas** available for testing (captures draw commands without GPU)
- **Screenshot mode** for headless CI verification

### Animation System

Three levels of animation:
1. **`ValueAnimation`** -- lightweight, zero-allocation member variable animator. Used in widget hover/transition effects.
2. **`AnimationManager`** -- heap-managed tween collection with callbacks. Used by JS `animate()` bridge.
3. **`Tween`** -- standalone from/to/duration/easing object.

Missing from a full CSS animation model:
- Keyframe sequences (multi-stop animations)
- Looping / iteration count
- Animation direction (alternate/reverse)
- Fill modes (forwards/backwards/both)
- Play state control (pause/resume)
- Animation events (start/end/iteration)
- Staggered animations / animation delay

### Text Rendering

- Text is rendered via Skia's text shaping (HarfBuzz) when using the Skia backend
- Font metrics available via `Canvas::measure_text_full()`
- Label widget supports multi-line text, alignment, letter-spacing, line-height
- TextEditor provides full editing with selection, clipboard, undo/redo
- No rich text / inline styling within a single Label

---

## 33. Explicit Non-Goals

These CSS/DOM features are intentionally excluded from Pulp's design:

| Feature | Reason |
|---------|--------|
| **Cascade / specificity** | Pulp uses direct property setting via JS. No competing style rules. Tokens provide reusable values without cascade complexity. |
| **Inline formatting context** | No mixed inline/block flow. Use flex layout with Labels for text alongside controls. |
| **Table layout** | Use grid (when implemented) or nested flex containers. |
| **Float / clear** | Legacy layout; flex/grid supersede. |
| **Multi-column layout** | Not applicable to audio plugin UIs. |
| **CSS Paged Media** | No print layout. |
| **`@page` / `@media print`** | No print context. |
| **`::before` / `::after`** | No pseudo-elements; create explicit child views. |
| **`content` property** | No generated content. |
| **`counter-increment` / `counter-reset`** | No CSS counters. |
| **`quotes`** | Not applicable. |
| **Shadow DOM** | No encapsulation model; widget IDs are globally scoped. |
| **`<iframe>` / embedded documents** | Single-document model. |
| **3D transforms** | 2D transforms only; 3D would require depth buffer management. |
| **`will-change`** | GPU-first rendering; all elements are already composited. |
| **`@font-face` with remote URLs** | Fonts are bundled locally; no network font loading. |
| **`navigator` / Web APIs** | Not a browser; no navigator, fetch, WebSocket in the bridge. |

---

## 34. Gaps and Risks

### Critical Gaps (Block Adoption)

| Gap | Impact | Difficulty | Notes |
|-----|--------|------------|-------|
| **CSS Grid** | Cannot build dashboard/token-list layouts | High | Full grid spec is complex; start with simple `grid-template-columns` + `grid-template-rows` |
| **Image loading (`<img>`, `background-image`, `drawImage`)** | No image display anywhere | Medium | Need async loading, decoding, GPU upload |
| **`position: absolute/relative` with TRBL** | Cannot build overlays, tooltips, popovers generically | Medium | Overlay queue exists but is not general-purpose |
| **Per-element `filter: blur()`** | Cannot build frosted-glass panels without backdrop-filter | Medium | Skia `SkImageFilter` supports this |
| **`font-family` bridge setter** | Cannot change fonts from JS | Low | Theme string tokens exist; need bridge API |
| **`@keyframes` animation** | Cannot build complex multi-step animations | Medium | Need keyframe interpolation engine |

### High-Value Gaps (Developer Expectations)

| Gap | Impact | Difficulty |
|-----|--------|------------|
| `margin: auto` | Centering idiom broken | Low |
| Per-corner `border-radius` | Cannot build pill shapes, chat bubbles | Low |
| `rgb()`/`hsl()` color parsing | Every CSS tutorial uses these | Low |
| `pointer-events: none` | Cannot build click-through overlays | Low |
| `user-select: none` | Cannot prevent text selection on labels | Low |
| `text-decoration: underline` | Cannot underline links/text | Low |
| `text-transform: uppercase` | Common styling pattern | Low |
| `white-space: nowrap` | Cannot prevent text wrapping | Low |
| `overflow-x` / `overflow-y` | Cannot independently control overflow axes | Low |
| Named CSS colors | `"red"`, `"blue"`, `"rebeccapurple"` | Low |
| `transition-delay` | Cannot stagger transition timing | Low |
| `z-index` | Cannot control stacking order | Medium |
| `getBoundingClientRect()` | Cannot query element geometry from JS | Low |
| `:disabled` state | Cannot disable widgets from JS | Low |

### Known Divergences from W3C

| Area | Divergence | Rationale |
|------|-----------|-----------|
| **All containers are flex** | CSS has block/inline/flex/grid; Pulp has flex only | Simplification; flex handles 95% of plugin UI layouts |
| **No cascade** | CSS specificity and cascade are core to CSS; Pulp uses direct-set | Plugin UIs are authored, not styled by users; cascade adds complexity without benefit |
| **Pixel units only** | CSS has px, em, rem, %, vw, vh, etc | Single-scale rendering; DPI scaling handled at canvas level |
| **`box-sizing: border-box` always** | CSS defaults to `content-box` | Modern CSS universally overrides to border-box; Pulp just defaults there |
| **Solid borders only** | CSS has 8 border styles | Dashed/dotted add render complexity; solid covers 99% of audio plugin use cases |
| **Scale around center only** | CSS `transform-origin` can be any point | Center-origin handles most audio widget scale animations |
| **Hover is explicit registration** | CSS `:hover` is automatic on all elements | Performance optimization; only elements needing hover get tracked |

### Architecture Risks

| Risk | Description | Mitigation |
|------|-------------|------------|
| **Grid complexity** | Full CSS Grid is a very large spec (tracks, areas, auto-flow, subgrid) | Implement a "Grid Lite" covering `grid-template-columns/rows` + `grid-column/row` placement, skip subgrid |
| **Text layout complexity** | Full CSS text (bidi, hyphenation, justification) is enormous | Leverage Skia/HarfBuzz for shaping; implement `white-space`, `word-break`, `text-overflow` only |
| **Filter performance** | Per-element blur/filters on GPU can be expensive | Use Skia `SkImageFilter` with saveLayer; profile carefully; offer `contain: paint` optimization |
| **Keyframe animation engine** | CSS `@keyframes` with multi-property interpolation is nontrivial | Start with single-property keyframes; add multi-property later |
| **Percentage units** | Adding % requires layout-relative calculations | Defer; `flex-grow` covers most use cases |

---

## Summary

| Category | ✅ Supported | ⚠️ Partial | 🔲 Planned | ➖ N/A | Total |
|----------|-------------|-----------|-----------|-------|-------|
| Display | 3 | 1 | 1 | 7 | 12 |
| Flexbox (container) | 9 | 1 | 1 | 0 | 11 |
| Flexbox (item) | 4 | 0 | 1 | 0 | 5 |
| Grid | 0 | 0 | 29 | 0 | 29 |
| Box Model (sizing) | 6 | 1 | 0 | 0 | 7 |
| Box Model (margin) | 5 | 0 | 1 | 1 | 7 |
| Box Model (padding) | 5 | 0 | 0 | 0 | 5 |
| Box Alignment | 4 | 0 | 9 | 2 | 15 |
| Sizing L3 | 3 | 1 | 8 | 0 | 12 |
| Writing Modes | 0 | 0 | 1 | 3 | 4 |
| Positioned Layout | 1 | 3 | 7 | 0 | 11 |
| Containment | 0 | 1 | 6 | 1 | 8 |
| Overflow | 2 | 2 | 7 | 0 | 11 |
| Backgrounds | 1 | 2 | 12 | 1 | 16 |
| Borders | 2 | 1 | 14 | 0 | 17 |
| Box Shadow | 1 | 0 | 2 | 0 | 3 |
| Outline | 0 | 0 | 4 | 0 | 4 |
| Color | 4 | 0 | 11 | 2 | 17 |
| Transforms | 2 | 0 | 7 | 6 | 15 |
| Transitions | 2 | 1 | 4 | 1 | 8 |
| Animations | 0 | 2 | 8 | 0 | 10 |
| Filters | 0 | 2 | 9 | 0 | 11 |
| Compositing/Blending | 0 | 1 | 2 | 0 | 3 |
| Masking | 0 | 1 | 11 | 0 | 12 |
| Fonts | 5 | 1 | 15 | 2 | 23 |
| Text L3 | 1 | 0 | 14 | 0 | 15 |
| Inline Layout | 1 | 0 | 3 | 1 | 5 |
| Text Decoration | 1 | 0 | 12 | 0 | 13 |
| UI | 2 | 1 | 11 | 1 | 15 |
| Pseudo-classes | 4 | 2 | 5 | 10 | 21 |
| Scrolling (CSSOM View) | 0 | 5 | 5 | 0 | 10 |
| Scroll Snap | 0 | 0 | 5 | 0 | 5 |
| Scroll Behavior | 0 | 1 | 3 | 0 | 4 |
| Custom Properties | 5 | 3 | 1 | 0 | 9 |
| DOM Events (mouse/pointer) | 3 | 3 | 10 | 0 | 16 |
| DOM Events (keyboard) | 1 | 2 | 1 | 1 | 5 |
| DOM Events (focus) | 2 | 0 | 2 | 0 | 4 |
| DOM Events (form) | 3 | 0 | 0 | 1 | 4 |
| DOM Events (scroll) | 1 | 0 | 1 | 0 | 2 |
| DOM Events (drag) | 0 | 0 | 6 | 0 | 6 |
| DOM Events (lifecycle) | 1 | 2 | 2 | 1 | 6 |
| Canvas 2D (state) | 2 | 0 | 1 | 1 | 4 |
| Canvas 2D (transform) | 3 | 0 | 4 | 0 | 7 |
| Canvas 2D (compositing) | 1 | 1 | 0 | 0 | 2 |
| Canvas 2D (fill/stroke) | 7 | 0 | 6 | 0 | 13 |
| Canvas 2D (shadow) | 0 | 0 | 3 | 0 | 3 |
| Canvas 2D (path) | 8 | 2 | 5 | 0 | 15 |
| Canvas 2D (rect) | 1 | 1 | 0 | 0 | 2 |
| Canvas 2D (text) | 3 | 2 | 4 | 1 | 10 |
| Canvas 2D (image) | 0 | 0 | 7 | 0 | 7 |
| Canvas 2D (filter) | 0 | 0 | 1 | 0 | 1 |
| Canvas 2D (export) | 0 | 0 | 3 | 0 | 3 |
| HTML Widgets (containers) | 6 | 0 | 0 | 0 | 6 |
| HTML Widgets (form) | 8 | 3 | 6 | 1 | 18 |
| HTML Widgets (interactive) | 0 | 1 | 2 | 0 | 3 |
| HTML Widgets (media) | 0 | 2 | 2 | 2 | 6 |
| HTML Widgets (progress/meter) | 2 | 0 | 0 | 0 | 2 |
| HTML Widgets (other) | 0 | 2 | 2 | 4 | 8 |
| **TOTALS** | **119** | **52** | **312** | **50** | **533** |

**Current coverage: 119 fully supported + 52 partial = 171 properties with some support out of 483 applicable (excluding N/A) = 35%**

**Fully supported: 119/483 = 25%**
**At least partial: 171/483 = 35%**
**Planned (no support yet): 312/483 = 65%**

### Priority Tiers for Implementation

**Tier 1 -- Ship Blockers (needed for any real plugin UI):**
- Image loading (`<img>`, `drawImage`, `background-image`)
- `font-family` bridge setter
- `pointer-events: none`
- `margin: auto`
- Per-corner `border-radius`
- `:disabled` / `setEnabled()`
- `z-index` / stacking control
- `getBoundingClientRect()` equivalent

**Tier 2 -- Developer Expectations (needed for comfortable adoption):**
- CSS Grid (basic subset)
- `rgb()` / `hsl()` / named color parsing
- `@keyframes` animation system
- `text-decoration`, `text-transform`
- `white-space: nowrap`
- Per-element `filter: blur()`
- `position: relative` with `top`/`left` offsets
- `transition-delay`
- `overflow-x`/`overflow-y`

**Tier 3 -- Full Parity (professional-grade):**
- Full Grid spec
- Variable fonts / `font-variation-settings`
- Scroll snap
- `clip-path` beyond rectangles
- Masking
- Container queries
- RTL / `direction`
- Custom `cubic-bezier()` easing
