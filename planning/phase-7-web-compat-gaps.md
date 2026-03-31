# Phase 7 — Web-Compat Gap Closure

**Version:** 2026-03-28
**Status:** Proposed
**Goal:** Close the highest-impact W3C compliance gaps so React/Vue developers can author plugin UIs without hitting unexpected dead ends.

---

## Motivation

Pulp's web-compat layer covers ~60% of what a modern frontend developer expects. The remaining gaps cluster into a few categories that repeatedly block real-world UI patterns:

1. **Responsive sizing is broken** — `em`, `rem`, `vw`, `vh` parse but resolve as `px`. `calc()` is absent. This means any relative layout code silently renders at the wrong size.
2. **DOM traversal is incomplete** — No `closest()`, `matches()`, `innerHTML`. Event delegation and template patterns break.
3. **Selectors miss the basics** — No `:first-child`, `:nth-child()`, `:not()`. Can't style alternating rows or exclude elements.
4. **No responsive breakpoints** — No `matchMedia()` or `@media`. Plugin UIs can't adapt to different host window sizes.
5. **Animation lifecycle is invisible** — No `transitionend` / `animationend` events. JS can't sequence animations.
6. **Missing visual properties** — `aspect-ratio`, `outline`, `visibility`, `white-space` are everyday CSS that doesn't work.

These aren't exotic features — they're the bread-and-butter of any CSS-in-JS workflow. Closing them makes the difference between "web-shaped API" and "actually works like the web."

---

## Priority Tiers

### P0 — Broken by Design (fix before anything else)

These are bugs disguised as missing features. Code that looks correct silently produces wrong results.

| Item | Problem | Fix |
|------|---------|-----|
| **Unit resolution** | `1.5em`, `50%`, `10vw` all treated as pixel values | css-parser.js: resolve em/rem against font-size chain, % against parent dimension, vw/vh against root |
| **calc() expressions** | `calc(100% - 20px)` throws or returns 0 | css-parser.js: lightweight expression evaluator with `+`, `-`, `*`, `/`, nested `calc/min/max/clamp` |
| **window.innerWidth/Height** | Hardcoded to 800×600 | Bridge: expose actual root view dimensions, update on resize |

### P1 — Blocks Common Patterns

Missing APIs that force developers to work around Pulp rather than with it.

| Item | What developers expect | Scope |
|------|------------------------|-------|
| **closest(selector)** | Event delegation: `e.target.closest('.panel')` | JS: walk parentElement, test _matchesSelector |
| **matches(selector)** | Conditional logic: `if (el.matches('.active'))` | JS: call _matchesSelector on self |
| **innerHTML** | Template insertion: `el.innerHTML = '<div>…</div>'` | JS: parse simple HTML string, create elements |
| **:first-child / :last-child** | Style first/last items | Selector engine: check index in parent.children |
| **:nth-child(n)** | Alternating rows: `tr:nth-child(odd)` | Selector engine: parse An+B syntax |
| **:not(selector)** | Exclusion: `.item:not(.disabled)` | Selector engine: negate inner match |
| **matchMedia()** | Responsive breakpoints | JS: query root dimensions, fire change events on resize |
| **transitionend / animationend** | Animation sequencing | Bridge: fire events when FrameClock animation completes |
| **aspect-ratio** | Maintain proportions: `aspect-ratio: 16/9` | FlexStyle: add aspect_ratio field, enforce in layout_children |
| **visibility: hidden** | Hide but preserve layout space | View: add `visibility_` separate from `visible_` |
| **outline** | Focus indicators, debug borders | View: add outline properties (width, color, offset), paint outside border |

### P2 — Quality of Life

Nice-to-have for polished UIs. Not blocking, but developers will ask for them.

| Item | Use case | Scope |
|------|----------|-------|
| **white-space: nowrap/pre** | Prevent text wrapping | Label: add whitespace mode |
| **word-break / overflow-wrap** | Long text handling | Label: break behavior |
| **text-shadow** | Text effects | View: add text_shadow paint in Label |
| **user-select: none** | Prevent text selection on UI chrome | View: add selectable flag |
| **pointer-events: none** | Click-through overlays | View: skip in hit_test when disabled |
| **classList.replace()** | Token swap | JS: remove old, add new |
| **before() / after() / prepend() / append()** | Modern DOM insertion | JS: delegate to insertBefore/appendChild |
| **focus() / blur()** | Programmatic focus management | JS: dispatch to native focus system |
| **wheel event** | Custom scroll handling | Bridge: wire scrollWheel as 'wheel' event |
| **scroll event** | Detect scroll position changes | Bridge: wire scroll position callbacks |
| **dblclick event** | Double-click detection | Bridge: fire on click_count == 2 |
| **resize event** | Window resize detection | Bridge: fire on root view bounds change |
| **background-size / background-position** | Image background control | View: add background image properties |
| **font-family** | Custom fonts | Label/TextEditor: font family selection |

### P3 — Advanced / Niche

Sophisticated features most plugin UIs won't need but power users may ask for.

| Item | Notes |
|------|-------|
| **clip-path** | Polygon/circle/ellipse clipping — needs canvas support |
| **backdrop-filter** | Blur behind element — requires compositor-level rendering |
| **mix-blend-mode** | Blending — requires Skia blend modes |
| **mask / mask-image** | Alpha masking — requires image loading + compositing |
| **attribute selectors [data-*]** | Rarely needed when className works |
| **:is() / :has()** | Modern selectors — nice but not critical |
| **::before / ::after** | Pseudo-elements — would need virtual child views |
| **form validation** | checkValidity, required, pattern — not critical for plugin UIs |
| **grid-template-areas** | Named areas — convenience over start/end indices |
| **grid-auto-flow** | Auto-placement direction — minor grid feature |

---

## Implementation Plan

### Phase 7.1 — Fix What's Broken (P0)

**Estimated scope:** css-parser.js + widget_bridge.cpp + web-compat.js
**Files:**
- `core/view/js/css-parser.js` — Add `resolveLength(parsed, context)`, `evaluateCalc(expr, context)`
- `core/view/src/widget_bridge.cpp` — Add `getRootSize()` bridge for viewport units
- `core/view/js/web-compat.js` — Wire window.innerWidth/Height to getRootSize()

**Acceptance criteria:**
- `element.style.width = "50%"` correctly sizes to half of parent
- `element.style.fontSize = "1.5em"` resolves to 1.5× parent font-size
- `element.style.width = "calc(100% - 40px)"` evaluates correctly
- `window.innerWidth` returns actual root view width
- 30+ unit resolution tests

### Phase 7.2 — DOM & Selector Gaps (P1)

**Files:**
- `core/view/js/web-compat.js` — Add closest(), matches(), innerHTML, focus()/blur()
- `core/view/js/web-compat.js` (selector engine) — Add :first-child, :last-child, :nth-child(An+B), :not()

**Acceptance criteria:**
- `el.closest('.panel')` finds nearest ancestor matching selector
- `el.matches('.active')` returns boolean
- `el.innerHTML = '<div class="x"><span>hi</span></div>'` creates correct tree
- `div:nth-child(odd)` selects odd children
- `.item:not(.disabled)` excludes disabled items
- 40+ tests

### Phase 7.3 — Responsive & Events (P1)

**Files:**
- `core/view/js/web-compat.js` — Add matchMedia(), ResizeObserver-like functionality
- `core/view/src/widget_bridge.cpp` — Add resize event dispatch, transitionend/animationend events
- `core/view/include/pulp/view/view.hpp` — Add visibility_, outline_, aspect_ratio to View
- `core/view/include/pulp/view/geometry.hpp` — Add aspect_ratio to FlexStyle
- `core/view/src/view.cpp` — Enforce aspect-ratio in layout_children

**Acceptance criteria:**
- `matchMedia('(min-width: 600px)')` returns correct result and fires change events
- `element.addEventListener('transitionend', fn)` fires when CSS transition completes
- `element.style.aspectRatio = "16/9"` maintains proportion
- `element.style.visibility = "hidden"` hides but preserves layout space
- `element.style.outline = "2px solid blue"` renders outside border
- 30+ tests

### Phase 7.4 — Quality of Life (P2)

Implement remaining P2 items based on developer feedback and adoption patterns.

---

## What We Deliberately Skip

Pulp is not a browser. These are intentionally out of scope:

- **Full HTML parsing** — innerHTML supports a safe subset, not arbitrary HTML
- **CSS cascade / specificity** — StyleSheet rules apply in source order, no specificity scoring
- **Shadow DOM** — Not needed for plugin UIs
- **Web Components** — Custom elements are just createElement with tag mapping
- **Service Workers / IndexedDB** — No offline or storage APIs
- **SVG rendering** — Canvas 2D API covers drawing needs
- **CSS Houdini** — No paint worklets or layout API extensions
- **CSS Container Queries** — matchMedia covers breakpoint needs

---

## Success Metric

After Phase 7, a developer should be able to take a typical React component's CSS/JSX and port it to Pulp's web-compat layer with minimal changes beyond removing React-specific hooks. The gap list should shrink from ~40 items to <10 niche features.
