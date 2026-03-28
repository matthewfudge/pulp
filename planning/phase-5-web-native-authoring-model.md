# Phase 5 -- Web-Native Authoring Model

**Version:** 2026-03-27
**Status:** Planned
**Depends on:** Phases 14-18 (CSS property parity work)
**Goal:** A compatibility layer so frontend developers can write familiar HTML/CSS/JS against Pulp's native GPU UI system.

---

## Motivation

Pulp's current JS bridge exposes a widget-oriented API (`createRow`, `setFlex`, `setBackground`, etc.) that maps directly to native C++ views. This is efficient and correct, but alien to frontend developers who think in `document.createElement("div")`, `element.style.backgroundColor`, and `document.querySelector(".panel")`.

This phase adds a **web-native authoring layer** on top of the existing bridge. The public API feels browser-shaped. The internal runtime remains native GPU (QuickJS -> View tree -> Canvas -> Skia Graphite -> Dawn/WebGPU). No DOM, no CSSOM, no browser engine.

---

## 1. HTML-Like Element Creation

### API Surface

```js
// createElement returns a handle object with chainable setters
const header = document.createElement("div");
header.id = "main-header";
header.className = "toolbar dark";
header.style.backgroundColor = "#1a1a2e";
header.style.padding = "12px";
header.style.display = "flex";
header.style.gap = "8px";
document.body.appendChild(header);

const title = document.createElement("span");
title.textContent = "My Plugin";
title.style.fontSize = "18px";
title.style.fontWeight = "700";
title.style.color = "#e0e0e0";
header.appendChild(title);

const slider = document.createElement("input");
slider.type = "range";
slider.min = 0;
slider.max = 100;
slider.value = 50;
header.appendChild(slider);
```

### Element Tag Mapping

| HTML Tag | Internal Widget | Notes |
|----------|----------------|-------|
| `div` | View (Row or Col based on flex-direction) | Default flex-direction: column |
| `span` | Label | Inline-like text element |
| `p`, `h1`-`h6` | Label | Font size/weight set from tag |
| `button` | ToggleButton / clickable Label | |
| `input[type=text]` | TextEditor | |
| `input[type=number]` | TextEditor (numeric_only) | |
| `input[type=password]` | TextEditor (password_mode) | |
| `input[type=range]` | Fader | |
| `input[type=checkbox]` | Checkbox | |
| `textarea` | TextEditor (multi_line) | |
| `select` | ComboBox | |
| `img` | ImageWidget | Requires image loading (Phase 18) |
| `canvas` | CanvasWidget | |
| `progress` | ProgressBar | |
| `details` | Collapsible container | |
| `dialog` | CallOutBox / overlay | |
| `label` | Label | `for` attribute binds click to target |
| `hr` | View with 1px height + bg | |
| `section`, `article`, `aside`, `header`, `footer`, `nav`, `main` | View | Semantic aliases for div |

### The `document` Object

```js
document.body           // root view
document.createElement(tag)
document.getElementById(id)
document.querySelector(selector)
document.querySelectorAll(selector)
```

`document` is a lightweight shim object registered in QuickJS global scope. `document.body` maps to the root View. `createElement` returns an `Element` proxy object that wraps a native widget ID.

### Element Proxy Object

Each `createElement` call returns an `Element` object with:

```js
element.id                    // get/set widget ID
element.className             // get/set class list string
element.classList.add(c)      // add class
element.classList.remove(c)   // remove class
element.classList.toggle(c)   // toggle class
element.classList.contains(c) // check class
element.style                 // CSSStyleDeclaration proxy (see below)
element.textContent           // get/set Label text
element.value                 // get/set for form elements
element.disabled              // get/set enabled state
element.hidden                // get/set visibility
element.children              // child element list
element.parentElement         // parent reference
element.appendChild(child)
element.removeChild(child)
element.insertBefore(child, ref)
element.replaceChild(new, old)
element.remove()
element.setAttribute(name, value)
element.getAttribute(name)
element.dataset               // data-* attributes as object
element.getBoundingClientRect()  // layout geometry query
element.scrollTo(x, y)
element.scrollTop / element.scrollLeft
element.offsetWidth / element.offsetHeight
element.clientWidth / element.clientHeight
```

### Implementation Strategy

The `Element` proxy is a JS-side class (defined in a prelude script loaded before user code) that:

1. Calls existing bridge functions internally (`createRow`, `createLabel`, `setFlex`, etc.)
2. Maintains a JS-side map from element IDs to Element objects
3. Translates `style.*` property writes into appropriate `setFlex`/`setBackground`/etc. calls
4. Translates event listener registrations into `registerClick`/`registerHover`/`on` calls

No C++ changes are needed for the basic layer -- it is a JS-only shim. Advanced features (querySelector by class, getBoundingClientRect) require targeted C++ bridge additions.

---

## 2. CSS-Like Styling

### The `style` Property

Every Element has a `style` property that is a `CSSStyleDeclaration` proxy. Property writes translate to bridge calls.

```js
element.style.display = "flex";           // -> setVisible / setFlex direction
element.style.flexDirection = "row";      // -> setFlex(id, "direction", "row")
element.style.justifyContent = "center";  // -> setFlex(id, "justify_content", "center")
element.style.alignItems = "center";      // -> setFlex(id, "align_items", "center")
element.style.gap = "8px";               // -> setFlex(id, "gap", 8)
element.style.padding = "12px 16px";     // -> setFlex(id, "padding_top/right/bottom/left", ...)
element.style.margin = "0 auto";         // -> setFlex(id, "margin_*", ...)
element.style.width = "200px";           // -> setFlex(id, "width", 200)
element.style.minWidth = "100px";        // -> setFlex(id, "min_width", 100)
element.style.maxWidth = "400px";        // -> setFlex(id, "max_width", 400)
element.style.flexGrow = "1";            // -> setFlex(id, "flex_grow", 1)
element.style.flexShrink = "0";          // -> setFlex(id, "flex_shrink", 0)
element.style.backgroundColor = "#1a1a2e";  // -> setBackground(id, "#1a1a2e")
element.style.color = "#e0e0e0";         // -> setTextColor(id, "#e0e0e0")
element.style.fontSize = "14px";         // -> setFontSize(id, 14)
element.style.fontWeight = "700";        // -> setFontWeight(id, 700)
element.style.borderRadius = "8px";      // -> setBorder(id, ..., 8)
element.style.opacity = "0.8";           // -> setOpacity(id, 0.8)
element.style.overflow = "hidden";       // -> setOverflow(id, "hidden")
element.style.cursor = "pointer";        // -> setCursor(id, "pointer")
element.style.transform = "scale(1.1)";  // -> setScale(id, 1.1)
element.style.transition = "all 0.3s ease-out";  // -> setTransitionDuration(id, 0.3)
element.style.zIndex = "10";             // -> setZIndex(id, 10)
element.style.position = "absolute";     // -> setPosition(id, "absolute")
element.style.top = "10px";              // -> setPositionOffset(id, "top", 10)
```

### CSS Value Parsing

The style proxy must parse CSS value strings into native calls:

| Value Format | Parser | Example |
|-------------|--------|---------|
| `"12px"` | Strip `px`, parse float | `padding = "12px"` -> 12 |
| `"1.5em"` | Multiply by parent font-size | `fontSize = "1.5em"` -> 21 (if parent is 14) |
| `"50%"` | Compute against parent dimension | `width = "50%"` -> 200 (if parent is 400) |
| `"12px 16px"` | Two-value shorthand (vertical horizontal) | `padding = "12px 16px"` |
| `"12px 16px 8px 4px"` | Four-value shorthand (TRBL) | `margin = "12px 16px 8px 4px"` |
| `"auto"` | Context-dependent | `margin = "0 auto"` -> auto centering |
| `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"` | Hex color | Direct passthrough |
| `"rgb(r, g, b)"` | Functional color | Parse and convert to hex |
| `"rgba(r, g, b, a)"` | Functional color with alpha | Parse and convert |
| `"hsl(h, s%, l%)"` | HSL color | Convert to hex |
| `"red"`, `"blue"`, etc. | Named CSS colors | Lookup table (148 colors) |
| `"var(--name)"` | Custom property | Resolve via theme tokens |
| `"var(--name, fallback)"` | Custom property with fallback | Resolve with fallback |
| `"calc(100% - 20px)"` | Calc expression | Evaluate at layout time |
| `"min(200px, 50%)"` | Min function | Evaluate at layout time |
| `"max(100px, 30%)"` | Max function | Evaluate at layout time |
| `"clamp(100px, 50%, 300px)"` | Clamp function | Evaluate at layout time |
| `"scale(1.1)"` | Transform function | Parse and apply |
| `"translate(10px, 20px)"` | Transform function | Parse and apply |
| `"rotate(45deg)"` | Transform function | Convert deg to rad |
| `"all 0.3s ease-out"` | Transition shorthand | Parse property, duration, easing |

### Class-Based Styling

```js
// Define reusable style rules
const styles = new StyleSheet({
  ".toolbar": {
    display: "flex",
    flexDirection: "row",
    alignItems: "center",
    gap: "8px",
    padding: "8px 16px",
    backgroundColor: "var(--surface-1)",
  },
  ".toolbar.dark": {
    backgroundColor: "var(--surface-0)",
  },
  ".btn": {
    padding: "6px 12px",
    borderRadius: "4px",
    cursor: "pointer",
    transition: "background-color 0.15s ease-out",
  },
  ".btn:hover": {
    backgroundColor: "var(--surface-2)",
  },
  ".btn-primary": {
    backgroundColor: "var(--accent)",
    color: "#ffffff",
  },
});

// Apply stylesheet -- rules are matched against className
styles.attach();

// Elements with matching classes get styled automatically
const btn = document.createElement("button");
btn.className = "btn btn-primary";
btn.textContent = "Save";
toolbar.appendChild(btn);
```

### StyleSheet Implementation

`StyleSheet` is a JS-side class that:

1. Stores rules as a map from selector string to property object
2. On `attach()`, registers with the element system
3. When an element's `className` changes, re-evaluates matching rules
4. Applies matching rules in declaration order (no specificity -- last match wins)
5. Pseudo-class rules (`:hover`, `:focus`, `:active`, `:disabled`) register event handlers that toggle styles
6. `var()` references resolve against the Pulp theme token system

Supported selectors (subset):

| Selector | Meaning | Support |
|----------|---------|---------|
| `.class` | Class match | Phase 5.1 |
| `.class1.class2` | Multiple class match | Phase 5.1 |
| `tag` | Tag name match | Phase 5.1 |
| `tag.class` | Tag + class | Phase 5.1 |
| `#id` | ID match | Phase 5.1 |
| `.class:hover` | Hover pseudo-class | Phase 5.1 |
| `.class:focus` | Focus pseudo-class | Phase 5.1 |
| `.class:active` | Active pseudo-class | Phase 5.2 |
| `.class:disabled` | Disabled pseudo-class | Phase 5.2 |
| `.parent > .child` | Direct child | Phase 5.3 |
| `.parent .descendant` | Descendant | Phase 5.3 |
| `.class:first-child` | Structural pseudo | Phase 5.3 |
| `.class:last-child` | Structural pseudo | Phase 5.3 |
| `.class:nth-child(n)` | Structural pseudo | Phase 5.3 |

No cascade, no specificity scoring. Rules apply in source order. This is intentional -- it avoids the complexity of the CSS cascade while still giving developers class-based reuse.

### Custom Properties (CSS Variables)

```js
// Set via Pulp theme token system
document.documentElement.style.setProperty("--accent", "#3b82f6");
document.documentElement.style.setProperty("--surface-0", "#0f0f1a");
document.documentElement.style.setProperty("--radius-md", "8px");

// Use in styles
element.style.backgroundColor = "var(--surface-0)";
element.style.borderRadius = "var(--radius-md)";

// Use in stylesheets
new StyleSheet({
  ".panel": {
    backgroundColor: "var(--surface-0)",
    borderRadius: "var(--radius-md)",
  },
});
```

`var()` is resolved by calling `resolve_color` or `resolve_dimension` on the Pulp theme system. `setProperty` maps to `applyTokenDiff`.

### Flexbox and Grid

Flexbox properties map directly to existing Pulp flex layout:

```js
container.style.display = "flex";           // already native
container.style.flexDirection = "row";      // setFlex direction
container.style.flexWrap = "wrap";          // setFlex flex_wrap
container.style.justifyContent = "center";  // setFlex justify_content
container.style.alignItems = "center";      // setFlex align_items
container.style.gap = "8px";               // setFlex gap
```

Grid properties require the grid layout engine (Phase 14):

```js
container.style.display = "grid";
container.style.gridTemplateColumns = "1fr 2fr 1fr";
container.style.gridTemplateRows = "auto 1fr auto";
container.style.gap = "16px";

child.style.gridColumn = "1 / 3";
child.style.gridRow = "2";
```

### Box Model

```js
// Full box model shorthand support
element.style.margin = "10px";                // all sides
element.style.margin = "10px 20px";           // vertical horizontal
element.style.margin = "10px 20px 30px";      // top horizontal bottom
element.style.margin = "10px 20px 30px 40px"; // top right bottom left
element.style.marginTop = "10px";             // individual side

// Same for padding
element.style.padding = "12px 16px";
element.style.paddingLeft = "24px";

// Border shorthand
element.style.border = "1px solid #333";
element.style.borderRadius = "8px";
element.style.borderTopLeftRadius = "12px";
```

### Transforms

```js
element.style.transform = "scale(1.1) rotate(45deg) translate(10px, 20px)";
element.style.transformOrigin = "center";
```

Transform parsing extracts individual functions and maps them to native calls. Multiple transforms are composed in order.

### Positioning and Z-Index

```js
element.style.position = "absolute";
element.style.top = "10px";
element.style.right = "10px";
element.style.zIndex = "100";
```

Maps to positioned layout system (Phase 17).

### calc / min / max / clamp

```js
element.style.width = "calc(100% - 40px)";
element.style.fontSize = "clamp(12px, 2vw, 18px)";
element.style.padding = "max(8px, 1%)";
```

Requires a lightweight expression evaluator in the style proxy that resolves at layout time. Percentage and viewport units require querying parent/root dimensions.

---

## 3. DOM-Like JS API

### Query Selectors

```js
const el = document.getElementById("volume-knob");
const panels = document.querySelectorAll(".panel");
const firstBtn = document.querySelector(".toolbar > button");
const labels = document.querySelectorAll("label");
```

Implementation:

- `getElementById` -- direct map lookup (already supported via widget ID)
- `querySelector` / `querySelectorAll` -- JS-side tree walk with selector matching
- Supported selector syntax: tag, `.class`, `#id`, `tag.class`, `.a .b` (descendant), `.a > .b` (child), `:first-child`, `:last-child`, `:nth-child(n)`

### Event Model

```js
element.addEventListener("click", (e) => {
  console.log("Clicked at", e.clientX, e.clientY);
});

element.addEventListener("mouseenter", (e) => {
  element.classList.add("hovered");
});

element.addEventListener("mouseleave", (e) => {
  element.classList.remove("hovered");
});

element.addEventListener("input", (e) => {
  console.log("Value changed:", e.target.value);
});

element.addEventListener("keydown", (e) => {
  if (e.key === "Escape") modal.close();
});

// Event delegation
container.addEventListener("click", (e) => {
  if (e.target.classList.contains("item")) {
    selectItem(e.target);
  }
}, { capture: false }); // bubbling is default
```

Event object shape:

```js
{
  type: "click",
  target: Element,       // element that fired
  currentTarget: Element, // element with the listener
  clientX: number,
  clientY: number,
  offsetX: number,       // relative to target
  offsetY: number,
  button: number,        // 0=left, 1=middle, 2=right
  ctrlKey: boolean,
  shiftKey: boolean,
  altKey: boolean,
  metaKey: boolean,
  key: string,           // for keyboard events
  code: string,          // for keyboard events
  preventDefault(): void,
  stopPropagation(): void,
}
```

Event propagation:

1. Capture phase (top-down) -- if `{ capture: true }`
2. Target phase
3. Bubble phase (bottom-up) -- default

`stopPropagation()` halts propagation. `preventDefault()` prevents default behavior (e.g., preventing a form submit).

### Geometry Queries

```js
const rect = element.getBoundingClientRect();
// { x, y, width, height, top, right, bottom, left }

const scrollY = container.scrollTop;
container.scrollTo({ top: 100, behavior: "smooth" });

element.scrollIntoView({ behavior: "smooth", block: "center" });
```

Requires C++ bridge additions:

- `getLayoutRect(id)` -- returns `{x, y, width, height}` in root-relative coordinates
- `getScrollPosition(id)` -- returns `{scrollTop, scrollLeft}`
- `scrollTo(id, x, y, smooth)` -- scroll with optional animation
- `scrollIntoView(id, block, behavior)` -- scroll parent to reveal child

### Computed Style

```js
const computed = getComputedStyle(element);
const bg = computed.backgroundColor;   // resolved color value
const w = computed.width;              // "200px" (layout-resolved)
const fs = computed.fontSize;          // "14px"
```

`getComputedStyle` returns a read-only object that queries the native view's current resolved values. This requires C++ bridge functions to read back layout results and style state.

---

## 4. Media and Assets

### Image Loading

```js
const img = document.createElement("img");
img.src = "assets/logo.png";
img.style.width = "64px";
img.style.height = "64px";
img.onload = () => console.log("loaded");
img.onerror = () => console.log("failed");
container.appendChild(img);

// Background images
panel.style.backgroundImage = "url('assets/texture.png')";
panel.style.backgroundSize = "cover";
panel.style.backgroundPosition = "center";
```

Maps to the image loading system (Phase 18). Images are loaded asynchronously, decoded, uploaded to GPU as SkImage, and rendered in the paint pass.

### Font Loading

```js
// @font-face equivalent
const font = new FontFace("CustomFont", "url(assets/CustomFont.woff2)");
await font.load();
document.fonts.add(font);

element.style.fontFamily = "CustomFont, Inter, sans-serif";
```

Font loading is local-only (no network fetch). `FontFace` triggers Skia font loading. The font stack falls back through listed families.

---

## 5. Architecture

### Layer Diagram

```
Developer writes:
  HTML-like JS (document.createElement, element.style, addEventListener)
    |
    v
Web Authoring Layer (pure JS, loaded as prelude)
  - Element proxy objects
  - CSSStyleDeclaration proxy with value parsing
  - StyleSheet class-based rules
  - querySelector tree traversal
  - Event delegation / bubbling
    |
    v (calls existing bridge functions)
Widget Bridge (C++ <-> QuickJS)
  - createRow, createLabel, setFlex, setBackground, etc.
  - registerClick, registerHover, on(), etc.
  - New: getLayoutRect, getScrollPosition, etc.
    |
    v
Native View Tree (C++)
  - View hierarchy, FlexStyle layout engine
  - Widget state management
  - Theme token resolution
    |
    v
Canvas / Render Pipeline
  - Skia Graphite via Dawn/WebGPU
  - Recording canvas for testing
```

### What Lives Where

| Component | Language | Location | Notes |
|-----------|----------|----------|-------|
| `document` global, `Element` class, `StyleSheet` class | JS | `core/view/js/web-compat.js` (prelude) | Loaded before user script |
| CSS value parser (px, %, calc, colors) | JS | `core/view/js/css-parser.js` (prelude) | Pure JS string parsing |
| Named CSS color table | JS | `core/view/js/css-colors.js` (prelude) | 148-entry lookup |
| `getLayoutRect`, `getComputedValue` | C++ | `core/view/src/widget_bridge.cpp` | New bridge functions |
| Event bubbling dispatch | JS | `core/view/js/web-compat.js` | Tree walk in JS |
| Selector matching engine | JS | `core/view/js/selector-engine.js` | Simple CSS selector parser |

### Performance Considerations

1. **Element proxies are lightweight.** Each is a small JS object holding an ID string. No heavy state.
2. **Style writes are immediate.** Setting `element.style.width = "200px"` calls `setFlex(id, "width", 200)` synchronously. No batching needed because layout is deferred to next frame anyway.
3. **Class-based style matching is O(rules * elements).** For typical plugin UIs (< 200 elements, < 50 rules), this is sub-millisecond.
4. **querySelector walks the JS-side element tree.** For deep trees, this could be slow. Mitigation: maintain an ID map and a class-to-elements multimap.
5. **Event bubbling walks parent chain.** Typical depth is < 10. Negligible cost.

---

## 6. Parity Targets

### Phase 5.1 -- Core Authoring (Minimum Viable)

| Feature | Status |
|---------|--------|
| `document.createElement` for div, span, button, input, select, textarea, img, canvas, progress | Required |
| `element.style.*` for all currently-supported CSS properties | Required |
| `element.id`, `className`, `classList`, `textContent`, `value` | Required |
| `appendChild`, `removeChild`, `insertBefore`, `remove` | Required |
| `addEventListener` / `removeEventListener` with bubbling | Required |
| `document.getElementById` | Required |
| CSS value parsing: px, hex colors, named colors, shorthand expansion | Required |
| `StyleSheet` with `.class` and `#id` selectors | Required |
| `var()` resolution via theme tokens | Required |

### Phase 5.2 -- Enhanced Styling

| Feature | Status |
|---------|--------|
| `querySelector` / `querySelectorAll` | Required |
| `:hover`, `:focus`, `:active`, `:disabled` in StyleSheet | Required |
| `rgb()`, `rgba()`, `hsl()`, `hsla()` color parsing | Required |
| `calc()`, `min()`, `max()`, `clamp()` evaluation | Required |
| Percentage unit support (`width: 50%`) | Required |
| `em` / `rem` unit support | Required |
| `getBoundingClientRect()` | Required |
| `getComputedStyle()` | Required |
| Transition shorthand parsing | Required |

### Phase 5.3 -- Full Compatibility

| Feature | Status |
|---------|--------|
| Descendant / child combinators in selectors | Required |
| `:first-child`, `:last-child`, `:nth-child(n)` | Required |
| `@keyframes`-like animation via JS API | Required |
| Event delegation with `e.target` propagation | Required |
| `scrollIntoView` | Required |
| `FontFace` loading API | Required |
| `element.dataset` | Required |
| `setAttribute` / `getAttribute` | Required |

---

## 7. Known Gaps and Intentional Divergences

### Will NOT Support

| Feature | Reason |
|---------|--------|
| Full CSS cascade / specificity | Complexity outweighs value; source-order application is sufficient |
| `<iframe>`, embedded documents | Single-document model |
| Shadow DOM / web components | Use JS modules and classes for component encapsulation |
| `innerHTML` / `outerHTML` | No HTML parser; use `createElement` API |
| `<template>` / `<slot>` | Use JS functions for templating |
| `window.fetch`, `XMLHttpRequest` | Not a browser; no network APIs |
| `navigator`, `history`, `location` | Not a browser |
| Server-side rendering | Not applicable |
| `@media` queries (true breakpoints) | Use root view resize callback instead |
| `::before` / `::after` pseudo-elements | Create explicit child elements |

### Intentional Divergences

| Area | Browser Behavior | Pulp Behavior | Rationale |
|------|-----------------|---------------|-----------|
| Default display | `block` | `flex` (column) | All containers are flex; matches modern UI development |
| Default box-sizing | `content-box` | `border-box` | Modern best practice; eliminates box-sizing reset boilerplate |
| Margin collapse | Margins collapse in block flow | No collapse | Flex containers do not collapse margins per W3C flex spec |
| Style application | Cascade with specificity | Source order, last match wins | Simpler mental model; no specificity surprises |
| Font rendering | OS text renderer | Skia/HarfBuzz | Consistent cross-platform rendering |
| Hover registration | Automatic on all elements | Automatic via style proxy | `:hover` in StyleSheet triggers auto-registration |
| `display: none` | Removed from layout | `setVisible(false)` | Equivalent behavior |
| Inline elements | Flow inline with text | Flex items in row container | No inline formatting context |

---

## 8. Naming Alignment Plan

### CSS Property to Bridge Function Mapping

The style proxy contains a mapping table. Key translations:

| CSS Property (camelCase) | Bridge Call |
|--------------------------|-------------|
| `backgroundColor` | `setBackground(id, color)` |
| `color` | `setTextColor(id, color)` |
| `fontSize` | `setFontSize(id, px)` |
| `fontWeight` | `setFontWeight(id, weight)` |
| `fontStyle` | `setFontStyle(id, style)` |
| `fontFamily` | `setFontFamily(id, family)` |
| `opacity` | `setOpacity(id, value)` |
| `cursor` | `setCursor(id, cursor)` |
| `overflow` | `setOverflow(id, value)` |
| `textAlign` | `setTextAlign(id, align)` |
| `textOverflow` | `setTextOverflow(id, value)` |
| `letterSpacing` | `setLetterSpacing(id, px)` |
| `lineHeight` | `setLineHeight(id, px)` |
| `display` | `setVisible(id, value !== "none")` + layout mode |
| `flexDirection` | `setFlex(id, "direction", value)` |
| `justifyContent` | `setFlex(id, "justify_content", value)` |
| `alignItems` | `setFlex(id, "align_items", value)` |
| `alignSelf` | `setFlex(id, "align_self", value)` |
| `flexGrow` | `setFlex(id, "flex_grow", value)` |
| `flexShrink` | `setFlex(id, "flex_shrink", value)` |
| `flexBasis` | `setFlex(id, "flex_basis", value)` |
| `flexWrap` | `setFlex(id, "flex_wrap", value)` |
| `gap` | `setFlex(id, "gap", value)` |
| `order` | `setFlex(id, "order", value)` |
| `width` | `setFlex(id, "width", value)` |
| `height` | `setFlex(id, "height", value)` |
| `minWidth` | `setFlex(id, "min_width", value)` |
| `maxWidth` | `setFlex(id, "max_width", value)` |
| `minHeight` | `setFlex(id, "min_height", value)` |
| `maxHeight` | `setFlex(id, "max_height", value)` |
| `margin` | `setFlex(id, "margin", value)` (+ per-side) |
| `padding` | `setFlex(id, "padding", value)` (+ per-side) |
| `border` | `setBorder(id, color, width, radius)` |
| `borderRadius` | `setBorder(id, ..., radius)` |
| `boxShadow` | `setBoxShadow(id, ...)` |
| `transform` | `setScale/setTranslate/setRotate` |
| `transition` | `setTransitionDuration(id, duration)` |
| `position` | `setPosition(id, value)` |
| `top/right/bottom/left` | `setPositionOffset(id, side, value)` |
| `zIndex` | `setZIndex(id, value)` |

---

## 9. Example: Complete Plugin UI

```js
// Styles
const styles = new StyleSheet({
  ".plugin-root": {
    display: "flex",
    flexDirection: "column",
    backgroundColor: "var(--surface-0)",
    color: "var(--text-primary)",
    fontFamily: "Inter, sans-serif",
    fontSize: "13px",
    width: "100%",
    height: "100%",
  },
  ".header": {
    display: "flex",
    flexDirection: "row",
    alignItems: "center",
    padding: "8px 16px",
    gap: "12px",
    borderBottom: "1px solid var(--border)",
  },
  ".title": {
    fontSize: "16px",
    fontWeight: "600",
  },
  ".controls": {
    display: "flex",
    flexDirection: "row",
    flexWrap: "wrap",
    gap: "16px",
    padding: "24px",
    justifyContent: "center",
  },
  ".knob-group": {
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    gap: "4px",
  },
  ".knob-label": {
    fontSize: "11px",
    fontWeight: "500",
    textTransform: "uppercase",
    letterSpacing: "0.5px",
    opacity: "0.7",
  },
});
styles.attach();

// Structure
const root = document.body;
root.className = "plugin-root";

const header = document.createElement("div");
header.className = "header";
root.appendChild(header);

const title = document.createElement("span");
title.className = "title";
title.textContent = "PulpSynth";
header.appendChild(title);

const controls = document.createElement("div");
controls.className = "controls";
root.appendChild(controls);

function createKnobGroup(name, paramId) {
  const group = document.createElement("div");
  group.className = "knob-group";

  const knob = document.createElement("knob"); // Pulp-specific element
  knob.id = paramId;
  knob.style.width = "48px";
  knob.style.height = "48px";
  group.appendChild(knob);

  const label = document.createElement("span");
  label.className = "knob-label";
  label.textContent = name;
  group.appendChild(label);

  return group;
}

controls.appendChild(createKnobGroup("Cutoff", "filter-cutoff"));
controls.appendChild(createKnobGroup("Resonance", "filter-reso"));
controls.appendChild(createKnobGroup("Attack", "env-attack"));
controls.appendChild(createKnobGroup("Release", "env-release"));
```

---

## 10. Acceptance Criteria

1. A frontend developer can write `document.createElement`, `element.style.*`, `addEventListener`, `querySelector` and have it produce a working native GPU UI
2. The web-compat prelude is < 15KB minified
3. No measurable frame-time regression (< 0.5ms overhead per frame for a 200-element UI)
4. All existing Pulp bridge tests continue to pass
5. 50+ unit tests covering Element creation, style parsing, class matching, event bubbling, and selector queries
6. A reference plugin UI written entirely in web-compat style renders identically to the same UI written in raw bridge calls
7. Screenshot regression tests for the reference plugin UI
