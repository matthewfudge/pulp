# Pulp HTML/CSS/JS Positioning and Guardrails

Date: 2026-03-28

## Purpose

Define a realistic, supportable product story for Pulp's HTML/CSS/JS surface.

This is not a browser-platform plan.
It is a positioning and scope-control document for:

- product messaging
- docs/status language
- implementation priorities
- support expectations

## Executive Summary

Pulp already has a real native JS UI runtime with a substantial browser-shaped compatibility layer.

That means the right story is not:

- "Pulp runs the web platform inside plugins"
- "Bring any web app and it will look the same"
- "React/Tailwind/HTML just compile straight to native"

The right story is:

Pulp provides a native GPU-rendered plugin UI system with a browser-shaped JS authoring layer. Frontend developers can build many control-heavy plugin UIs using familiar DOM/CSS-style patterns, but the target is a curated native compatibility layer, not full browser compatibility.

The main implementation risk is not lack of ambition. It is support drift:

- the code already exposes a broad web-compat surface
- the docs often describe it in browser-shaped language
- the repo does not yet set sufficiently sharp limits on what is and is not promised

If that is left vague, users will reasonably assume broader compatibility than Pulp can sustainably support.

## Current Reality

## What clearly exists today

These are grounded in the current code/docs/test surface:

- A real embedded JS runtime exists via QuickJS in `core/view/include/pulp/view/script_engine.hpp`.
- A real native widget bridge exists in `core/view/src/widget_bridge.cpp`.
- A browser-shaped compatibility prelude exists in:
  - `core/view/js/web-compat.js`
  - `core/view/js/web-compat-style-decl.js`
  - `core/view/js/web-compat-element.js`
  - `core/view/js/web-compat-document.js`
- The native `View` model already exposes substantial layout/style/input state in `core/view/include/pulp/view/view.hpp`.
- The repo already tracks web standards coverage in `docs/reference/w3c-coverage.md`.
- The repo already claims a web-compat authoring surface in:
  - `docs/guides/web-compat.md`
  - `docs/guides/from-react-css.md`
  - `docs/reference/js-bridge.md`
  - `docs/reference/capabilities.md`
  - `docs/status/support-matrix.yaml`
- There is a real hot-reload path in `test/test_hot_reload.cpp`.
- There is a real browser host for WASM plugin formats in `docs/guides/web-plugins.md`.

So this is not hypothetical. Pulp already has a meaningful HTML/CSS/JS story.

## What the code suggests the story actually is

The implementation is not "HTML renderer -> native plugin."

It is:

1. JS runs inside Pulp
2. browser-like objects are provided by a JS shim
3. CSS-like properties are parsed and translated
4. the shim maps into a native `View` tree and native widgets
5. painting happens through Canvas / Skia / Dawn / platform backends

That makes Pulp a native UI framework with web-inspired authoring, not a browser engine.

## Trust Drift and Inconsistency

The repo already has some truth drift that should be corrected before pushing the story harder.

### 1. "Browser-shaped" and "no DOM" are both used, sometimes loosely

`docs/guides/web-compat.md` correctly says "No WebView, no DOM, no browser engine — just a JS prelude."

But `core/view/js/web-compat.js` really does expose DOM-like concepts:

- `document.createElement`
- `appendChild`
- `querySelector`
- `innerHTML`
- `classList`
- `addEventListener`

So the truthful phrasing is:

- no real browser DOM/CSSOM engine
- yes, a DOM-like compatibility shim over native widgets

### 2. Some docs are more absolute than the implementation justifies

Examples:

- `docs/reference/capabilities.md` presents Grid, Flexbox, JS scripting, and web-compat features as broadly "usable"
- `docs/reference/w3c-coverage.md` still marks important parts as partial:
  - Grid partial
  - transitions partial
  - animations partial
  - filter effects partial
  - background image support incomplete
  - intrinsic sizing keywords incomplete
  - full cascade/specificity deliberately absent

That does not mean the work is weak.
It means the current docs flatten important nuance.

### 3. The React/CSS migration guide contains drift

`docs/guides/from-react-css.md` says some concepts do not transfer, including selectors and class-based styling.

But the current web-compat layer clearly does support:

- `className`
- `StyleSheet`
- selector matching
- `querySelector`
- pseudo-class matching

That guide needs to distinguish:

- supported browser-shaped features
- unsupported browser engine features
- conceptual differences from React

not collapse them together.

### 4. Spec-count language risks over-promising

`docs/reference/w3c-coverage.md` says Pulp was audited against 26 specs.

That is useful engineering evidence.
It is not a safe product promise by itself.

Users hear "26 specs" and often infer:

- broad browser compatibility
- design-tool export compatibility
- Tailwind/React portability
- HTML/CSS page fidelity

The count should support the story, not be the story.

## Strongest Truthful Claim We Can Make Now

This is the strongest claim that appears supportable from the current repo:

Pulp already provides a native GPU-rendered plugin UI system with a browser-shaped JavaScript authoring layer. For many app-style plugin UIs, developers can use familiar DOM-like JS, CSS-like layout/styling, and hot reload to build native interfaces without relying on WebView.

Important implied limits:

- "for many app-style plugin UIs"
- "browser-shaped"
- "CSS-like"
- "without relying on WebView"

This avoids implying:

- full browser compatibility
- arbitrary HTML app import
- React runtime support
- complete CSS support

## Right Near-Term Product Promise

The best near-term promise is:

Pulp is a native plugin framework that lets frontend-minded developers build real native plugin UIs using familiar HTML/CSS/JS patterns, with a curated compatibility layer and native audio/widget primitives where the web platform has no good equivalent.

More concretely:

- Use DOM-like JS and CSS-like layout/styling for structure and visual organization
- Use native bridge widgets for audio controls, visualization, and plugin-specific interaction
- Use design tokens for theming and cross-plugin style systems
- Use screenshots/tests to validate parity against reference designs
- Expect adaptation, not direct browser equivalence

This is much safer than:

- "build plugins with just web code"
- "import frontend apps directly"
- "full W3C support"

## What We Should Explicitly Not Claim

These should be explicit non-claims in docs and positioning:

- Pulp is not a browser engine
- Pulp does not provide full DOM/CSSOM behavior
- Pulp does not guarantee arbitrary HTML/CSS page fidelity
- Pulp does not run React directly
- Pulp does not run Tailwind directly as a first-class supported runtime
- Pulp does not promise npm/bundler/browser ecosystem compatibility
- Pulp does not promise that AI-generated frontend code will run unchanged
- Pulp does not currently provide a general compile-any-HTML/CSS-to-native pipeline
- Pulp does not promise browser-identical text layout, font loading, intrinsic sizing, or media behavior

These non-claims are not weaknesses.
They are what keeps the story supportable.

## Support Classification

This section defines how the HTML/CSS/JS story should be described publicly.

## Native C++ Fallback and Mixed Authoring

Pulp should be explicit that the HTML/CSS/JS layer is optional.

If a developer does not want the browser-shaped JS authoring layer, the fallback is not WebView and not "no UI."
The fallback is Pulp's native retained UI system in C++:

- `View` tree composition in `pulp/view/view.hpp`
- built-in widgets in `pulp/view/widgets.hpp`
- additional components in `pulp/view/ui_components.hpp`
- direct theming via `Theme`
- custom drawing via `Canvas`
- custom widgets by subclassing `View` and overriding `paint()` / input handlers

This path already exists and should be described as first-class, not legacy.

That means the support story is:

- lowest level: native C++ `View` / `Canvas`
- middle level: native C++ widgets and components
- high level: JS bridge / web-compat authoring layer

This is a strength. It lets Pulp serve:

- frontend-minded developers who want fast UI iteration
- audio/plugin developers who want native C++ control
- teams who want to mix both

## Can C++ and JS be mixed?

Yes, but the docs should describe the safe mental model carefully.

Architecturally, the JS bridge creates and mutates the same native `View` tree.
So the system is mixable by design.

The truthful claim is:

- C++ and JS can coexist in one UI
- they are best treated as two authoring layers over the same native widget/view model

The practical guidance should be:

- use C++ for custom widgets, platform integration, advanced paint/input behavior, and performance-sensitive or framework-level UI pieces
- use JS/web-compat for layout, styling, view composition, design-tool workflows, and rapid iteration

The safest supportable mixing patterns are:

1. C++ owns the root and injects a JS-authored subtree
2. JS owns most of the structure, while C++ supplies custom widgets/views exposed through the bridge
3. C++ and JS share theme/token data, but ownership boundaries stay explicit

What should not be implied:

- arbitrary bidirectional control with no ownership rules
- seamless browser-like introspection of every pre-existing C++ node from JS
- a fully unified React-style component system across C++ and JS

So the docs should say:

Pulp supports native C++, JS/web-compat authoring, or a hybrid approach. The hybrid model works best when each layer owns clear parts of the same native view tree.

## Works Well

### JavaScript-authored native UI

Works well.

Why:

- QuickJS runtime exists
- native bridge exists
- hot-reload exists
- parameter/state access exists
- screenshot/testing infrastructure exists

Good fit:

- scripted plugin UIs
- design-tool-like interfaces
- native UI driven by JS files
- mixing layout/styling helpers with native audio widgets

### CSS-like layout and styling subset for app-style UIs

Works well, with bounded scope.

Why:

- Flexbox subset is strong
- many box model / color / typography / transform features exist
- StyleSheet + selectors + className exist
- browser-style tests and reftests exist

Good fit:

- panels, rows, columns, inspectors, token browsers, control strips, settings screens
- plugin UIs that look frontend-inspired but are still native

### DOM-like authoring for simple to medium-complexity trees

Works well.

Why:

- `document.createElement`
- `appendChild`
- `querySelector`
- `classList`
- `style`
- `innerHTML` for common nested patterns

Good fit:

- programmatic UI authoring
- AI-authored browser-shaped JS that targets the supported subset

## Works With Adaptation

### HTML

Works with adaptation.

Why:

- tag mapping exists
- `innerHTML` exists for common nested patterns
- the system maps tags to native widgets

But:

- this is not full HTML parsing/rendering
- unsupported semantics should be expected
- layout fidelity depends on the supported subset

Practical expectation:

- use HTML as a convenient authoring shape or import source
- do not promise arbitrary HTML document fidelity

### CSS

Works with adaptation.

Why:

- substantial subset is implemented
- many familiar properties already map well

But:

- no real CSS engine
- no true browser cascade/specificity model
- some properties are parsed but stubbed or partial
- intrinsic sizing and some advanced layout rules are incomplete

Practical expectation:

- control-heavy app UIs can map well
- rich marketing pages and edge-case CSS should not be promised

### AI-generated frontend code

Works with adaptation.

Why:

- AI output that is simple DOM-like JS plus inline styles is a plausible input
- AI output can also target the native bridge directly

But:

- AI-generated React/Tailwind/browser code should be treated as source material, not guaranteed runtime input
- prompts need to target the supported subset if you want reliability

Practical expectation:

- "generate UI for the Pulp web-compat subset" is realistic
- "paste arbitrary AI-generated web app code and it just works" is not

### Design-tool exports

Works with adaptation.

Why:

- token system is real
- token export is real
- design-tool parity work is in progress

But:

- direct import/export pipelines are not yet a mature supported product surface
- external design-tool exports will need translation

Practical expectation:

- design tokens, structure hints, and reference layouts are good inputs
- general Figma/HTML export parity should not be claimed yet

### Browser preview as a reference workflow

Works with adaptation.

Why:

- browser host exists for WASM plugin formats
- optional WebView exists for browser content panels

But:

- browser preview is not the primary native UI runtime
- browser rendering should not be treated as the canonical source of native fidelity

Practical expectation:

- good for demos, WASM deployment, HTML reference comparison
- not the same thing as native UI parity

## Experimental

### Compile-to-native HTML/CSS workflows

Experimental.

Why:

- the roadmap clearly points in this direction
- current implementation behaves more like a runtime translation layer than a formal compiler pipeline

But:

- there is no mature constrained compiler contract yet
- there is no clearly frozen supported HTML/CSS input profile

Practical expectation:

- treat this as roadmap / advanced workflow
- do not market it as a stable supported ingestion path yet

### Tailwind

Experimental.

Why:

- Tailwind assumes a CSS pipeline and utility-class expansion model
- Pulp has class names and style rules, but not a native Tailwind integration story

Possible path:

- precompile Tailwind classes to a supported CSS subset or inline style object
- translate utility output into Pulp style declarations

But today:

- Tailwind should be presented as an adaptation target, not direct support

### React

Experimental as a migration target, not as a runtime.

Why:

- Pulp has a migration guide and many concepts map well
- React does not run directly in the current architecture

Not supported today:

- JSX runtime
- reconciliation
- hooks/lifecycle model
- bundler-driven app execution inside Pulp

Practical expectation:

- "React-like mental model" can be supported
- "run React in Pulp" should not be claimed

## Out of Scope

These should remain out of scope unless the architecture changes materially:

- full browser compatibility
- full CSS cascade/specificity
- Shadow DOM / custom elements as a core promise
- general npm package compatibility
- arbitrary third-party web component compatibility
- browser-exact layout/text behavior for any random site
- turning Pulp into a WebView-first framework

## Why W3C Specs Still Matter

The right reason to implement W3C-aligned behavior is not marketing.

It is contract quality.

If Pulp wants to feel familiar to frontend developers, then:

- Flexbox semantics
- Grid subset semantics
- box model
- typography
- selectors
- events
- pointer behavior
- transforms
- transitions/animation concepts

need to behave predictably enough that web instincts transfer.

So the W3C work should be framed as:

- standards-informed compatibility work
- support for the curated subset that matters to plugin/app UIs

not:

- pursuit of browser completeness

## Recommended Product Messaging

## One-line positioning

Pulp is a native audio plugin framework with a browser-shaped JavaScript UI layer that lets frontend-minded developers build native plugin interfaces using familiar web patterns.

## Stronger short paragraph

Pulp is a native C++/Swift audio plugin framework with a GPU-rendered JavaScript UI layer. Its web-compat layer lets developers use familiar DOM-like JS and CSS-like layout/styling for many app-style plugin UIs, while native widgets and the C++ core handle audio, MIDI, plugin formats, and platform integration. The goal is web-like authoring, not browser compatibility.

## Recommended guardrail sentence

Pulp supports a curated HTML/CSS/JS compatibility layer for native plugin UIs; it does not aim to run arbitrary browser apps unchanged.

## Messaging to remove or weaken

Avoid phrases like:

- "full W3C support"
- "bring over any HTML/CSS/JS design"
- "no C++ required" as a universal product statement
- "like a browser, but native"
- "React or Tailwind support" without the word "adaptation"

Prefer:

- "frontend-style authoring"
- "browser-shaped compatibility layer"
- "curated subset"
- "native translation layer"
- "reference design parity"

## Recommended Phased Implementation

## Phase A — Promise Discipline

Goal:

- make the docs honest before adding more surface area

Work:

- unify README, VISION, overview, capabilities, web-compat guide, migration guide, and support matrix language
- define the supported subset explicitly
- add a top-level "what we do / do not mean by HTML/CSS/JS support" page

## Phase B — Stabilize the Supported Subset

Goal:

- make the current subset durable and test-backed

Work:

- freeze canonical supported HTML tags
- freeze canonical supported CSS properties/behaviors
- freeze canonical supported JS/browser-like APIs
- add representative screenshot/reference fixtures
- make "parsed but not enforced" cases visible in docs

## Phase C — AI/Design Import Path

Goal:

- make AI-generated and design-tool-authored input reliable

Work:

- define a promptable supported subset for AI tools
- define a translation path from HTML/CSS-like input to native Pulp trees
- add import diagnostics explaining unsupported constructs
- prefer transform/translation over silent partial behavior

## Phase D — Constrained Compiler Layer

Goal:

- support an explicit compile-to-native path for a bounded profile

Work:

- define supported HTML/CSS subset as a spec
- compile/transpile that subset into native Pulp widgets/styles/tokens
- add parity corpus: HTML reference -> native output
- keep this separate from browser completeness

## Phase E — Tailwind / React Adapters Only If Justified

Goal:

- avoid ecosystem surface area without a support strategy

Work:

- add a Tailwind utility translation layer only if real users need it
- add React-oriented authoring/export only if it compiles to the supported subset
- do not run either runtime directly unless architecture changes justify it

## Recommended Documentation Changes

These should happen after agreement on positioning:

1. Add a docs page:
   - `docs/guides/html-css-js-positioning.md`
   - clearly defines supported subset, non-goals, and migration expectations

2. Reframe `docs/reference/w3c-coverage.md`
   - keep the spec inventory
   - add a blunt note that spec count does not equal browser compatibility

3. Tighten `docs/guides/from-react-css.md`
   - distinguish supported browser-shaped concepts from unsupported React runtime concepts
   - remove outdated statements that selectors/class-based styling do not exist

4. Tighten `docs/reference/capabilities.md` and `docs/status/support-matrix.yaml`
   - split "usable" into more honest support profiles where needed
   - especially for Grid, filters, animation semantics, and web-API coverage

5. Add one short section to the overview/getting-started story:
   - "Three ways to build UIs in Pulp"
   - native C++
   - JS/web-compat
   - hybrid C+++JS

## Recommended Support Policy

When users ask whether something is supported, answer in one of four ways only:

- supported directly
- supported with adaptation
- experimental
- out of scope

Do not answer with:

- "kind of"
- "mostly web-compatible"
- "26 specs"

Those phrases create support debt.

## Bottom Line

Pulp already has enough real implementation to claim a meaningful HTML/CSS/JS story.

But the right promise is narrower than "the web, but native."

The most supportable framing is:

Pulp offers a native GPU-rendered plugin UI system with a browser-shaped authoring layer. It is strong for frontend-inspired app UIs, strong for JS-driven native plugins, and promising for AI/design translation workflows. It is not a general-purpose browser engine, and should not be marketed as one.

## Suggested Agent Prompt

Use this prompt for the docs/positioning cleanup pass:

```text
Audit and refine Pulp's HTML/CSS/JS positioning across the docs so the story is ambitious but supportable.

Primary reference:
- planning/html-css-js-positioning-and-guardrails-2026-03-28.md

Goals:
- make the HTML/CSS/JS story honest, clear, and consistent
- preserve the value of Pulp's browser-shaped JS authoring layer
- avoid implying full browser compatibility
- explain the native C++ fallback and the hybrid C+++JS path clearly

Focus docs:
- README.md
- VISION.md
- docs/concepts/overview.md
- docs/guides/web-compat.md
- docs/guides/from-react-css.md
- docs/reference/w3c-coverage.md
- docs/reference/capabilities.md
- docs/reference/modules.md
- docs/status/support-matrix.yaml

What to do:
1. Compare the current docs against planning/html-css-js-positioning-and-guardrails-2026-03-28.md.
2. Identify overclaims, ambiguous claims, and conflicting claims.
3. Rewrite the docs so they consistently present Pulp as:
   - a native GPU-rendered plugin framework
   - with a browser-shaped JS authoring layer
   - with a first-class native C++ UI path
   - and an optional hybrid C+++JS path
4. Use clear support language:
   - works well
   - works with adaptation
   - experimental
   - out of scope
5. Make sure the docs explicitly do not imply:
   - full browser compatibility
   - React runtime support
   - Tailwind as a first-class supported runtime
   - arbitrary HTML/CSS/JS app fidelity
6. Keep the strongest truthful claim intact:
   - frontend-minded developers can build many app-style native plugin UIs using familiar DOM-like JS and CSS-like patterns
7. Add or improve one concise section explaining the three authoring modes:
   - native C++
   - JS/web-compat
   - hybrid
8. Do not change product code.

Deliverables:
- updated docs listed above
- one short summary of:
  - what claims were weakened
  - what claims were strengthened
  - what support boundaries are now explicit

Success criteria:
- an experienced engineer can read the docs and understand exactly what Pulp's HTML/CSS/JS story is
- a frontend developer is attracted, but not misled
- a native plugin developer understands they can ignore JS entirely or mix it in selectively
- the docs no longer read like Pulp is secretly a browser engine
```
