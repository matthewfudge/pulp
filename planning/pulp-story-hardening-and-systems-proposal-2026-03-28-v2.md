# Pulp Story Hardening and Systems Proposal v2

Date: 2026-03-28

## Why v2

The first proposal clarified the top-level story:

- lead with `Pulp Core`, `Pulp View`, and `Pulp Tools`
- treat automation, design, and reusable components as secondary capabilities until they earn stronger promotion

This v2 goes one level deeper:

- what is missing from automation capabilities
- what is missing from design capabilities
- what is missing from reusable components
- whether those should eventually become named systems or remain inside Core/View/Tools
- which "do not claim yet" items should become real goals, which should remain bounded, and which should never be goals

The purpose is not only messaging. It is product discipline.

---

## Current Position

The most supportable story is still:

> Pulp is a native audio plugin framework where you write C++, JavaScript, or both, and ship from one codebase.

Everything below should strengthen that line.

The current top-level structure should remain:

- `Pulp Core`
- `Pulp View`
- `Pulp Tools`

Automation, design, and reusable components should currently be described as capability clusters inside those surfaces, not as fully independent products.

---

## Reality Check by Capability Cluster

### 1. Automation Capabilities

Current reality:

- repo-level MCP server exists: `tools/mcp/pulp_mcp.cpp`
- plugin CLI harness exists as a library pattern: `tools/plugin-cli/plugin_cli.hpp`
- headless host exists: `core/format/include/pulp/format/headless.hpp`
- screenshot tooling exists and is used by CLI/MCP
- `VISION.md` and `STATUS.md` currently say more than the build system cleanly guarantees

Current gaps:

- `pulp_add_plugin()` does not automatically generate per-plugin CLI targets
- `pulp_add_plugin()` does not automatically generate per-plugin MCP targets
- `pulp` CLI does not expose `pulp mcp serve` even though `VISION.md` says it does
- there is no single automation profile concept in CMake
- repo MCP and future per-plugin MCP are conflated in docs/status
- there is no clear packaging story for automation binaries
- there is no obvious security/disable story for shipping products that do not want CLI or MCP surfaces

Implication:

The automation story is real but fragmented.

### 2. Design Capabilities

Current reality:

- design tokens are real
- inspector exists
- design export exists
- design-tool examples exist
- the bridge/runtime work for frontend-style authoring is substantial
- the design-tool/product story is still inconsistent

Current gaps:

- `pulp design` in `tools/cli/pulp_cli.cpp` expects `build/tools/design/pulp-design`
- the actual desktop target currently lives under `examples/design-tool/` as `pulp-design-tool`
- `tools/design/CMakeLists.txt` is effectively empty
- the design tool is still presented partly as example, partly as product
- the "AI Style Designer" loop is not yet fully productized and verified
- plugin-preview / lock-it-in-and-ship-it workflows are still incomplete as a public product story

Implication:

Design is more real than the average roadmap feature, but less productized than the current vision text implies.

### 3. Reusable Components

Current reality:

- there are real reusable UI pieces in `pulp/view`
- some higher-level components exist, including preset browser and waveform editor surfaces
- `pulp add component` exists in the CLI help
- `tools/add-component.py` exists

Current gaps:

- `tools/add-component.py` is still mostly a stub
- `tools/components/registry.yaml` is effectively empty
- there is no real component packaging/versioning/install story yet
- there is no stable compatibility contract for component distribution
- there is no clear distinction between "internal framework component" and "reusable shipped component"

Implication:

Reusable components exist in code, but not yet as a true product layer.

---

## Should These Become Named Systems?

### Automation

Recommendation:

- today: keep under `Pulp Tools`
- later: promote only if per-plugin automation is generated, documented, tested, and configurable

Good future promotion trigger:

`Pulp Automate` becomes a real named system only when:

- per-plugin CLI generation exists
- per-plugin MCP generation exists
- repo-level MCP and plugin-level MCP are clearly distinguished
- enable/disable policy is explicit
- automation binaries are covered by templates, docs, tests, and packaging

Until then:

talk about `automation capabilities in Pulp Tools`

### Design

Recommendation:

- today: split across `Pulp View` and `Pulp Tools`
- later: promote only if the design tool and AI restyling loop are truly productized

Good future promotion trigger:

`Pulp Design` becomes a real named system only when:

- `pulp design` is a real first-class product entry point
- plugin preview is real
- live token edit -> preview -> accept -> persist -> ship is verified
- style packs are versioned/documented
- importer/exporter surfaces are clear

Until then:

talk about `design capabilities in Pulp View and Pulp Tools`

### Reusable Components

Recommendation:

- today: keep under `Pulp View`
- later: promote only if there is a real registry/install/versioning/docs/test story

Good future promotion trigger:

`Pulp Components` becomes a real named system only when:

- `pulp add component` actually installs things
- a non-empty registry exists
- component versioning/compatibility is documented
- components compile and test independently
- there are polished examples and screenshots

Until then:

talk about `reusable components in Pulp View`

---

## Claim Ladder

This is the most important section.

Each item falls into one of four buckets:

- `Non-goal for the native runtime story`
- `Not a goal, but translation workflow is a goal`
- `Good future goal`
- `Optional future goal, not required for the core story`

### 1. "Pulp is a browser-compatible runtime"

Classification:

- `Non-goal for the native runtime story`

Why:

- it conflicts with the core architectural promise
- it would create a support nightmare
- it would blur Pulp into the exact category it is trying not to be

Important nuance:

- this does **not** mean web/WASM targets are a non-goal
- it means the native UI layer should not be marketed as a browser-compatible runtime

Better goal:

- strong frontend-style authoring
- strong supported subset
- strong translation/adaptation tooling

### 2. "Arbitrary HTML/CSS/JS ports over unchanged"

Classification:

- `Never`

Why:

- unchanged arbitrary ports imply browser semantics, third-party ecosystem expectations, and enormous compatibility scope

Better goal:

- `Not a goal, but translation workflow is a goal`

The real goal should be:

> Pulp can audit and convert many app-style HTML/CSS/JS designs into the supported native subset with clear diagnostics.

Make this concrete:

- audit incoming frontend code or exports
- identify what maps directly to native View primitives
- identify what needs adaptation
- identify what is unsupported
- generate a buildable starting point instead of promising pixel-identical direct execution

That is much more defensible.

Necessary to claim that stronger translation story:

- import audit command
- unsupported construct reporting
- translation guide
- parity examples
- reference-design tests for supported patterns

### 3. "React is supported"

Classification:

- `Never` as a runtime claim

Possible bounded goal:

- `Not a goal, but translation workflow is a goal`

Meaning:

- React component structures may be good input for translation
- React itself should not be the runtime support promise

### 4. "Tailwind is supported"

Classification:

- `Never` as a runtime claim

Possible bounded goal:

- `Not a goal, but translation workflow is a goal`

Meaning:

- Tailwind-like design input or class extraction could feed conversion tools
- Pulp should not present itself as a Tailwind runtime target

### 5. "Every plugin automatically ships as a polished CLI + MCP server"

Classification:

- `Good future goal`, but the exact wording should still probably change

Important nuance:

Even if Pulp builds this, "always ships by default" may not be the right end state.

The better product goal is:

> Pulp can generate per-plugin CLI and MCP automation surfaces as part of the build, with explicit opt-in or profile-based control.

Why this is better:

- some developers will want it disabled
- some products may not want extra binaries or automation endpoints
- opt-in or profile-based generation is easier to defend and test

Necessary to claim the stronger version:

- `pulp_add_plugin()` support for automation profiles
- generated per-plugin CLI target
- generated per-plugin MCP target
- docs for enabling/disabling
- template coverage
- validation tests for generated automation surfaces
- packaging/install behavior defined

Recommended shape:

- `AUTOMATION none|cli|mcp|full`
or
- `ENABLE_PLUGIN_CLI`
- `ENABLE_PLUGIN_MCP`

### 6. "Pulp ships a real Claude plugin product"

Classification:

- `Optional future goal, not required for the core story`

This is important.

Pulp does not need a Claude-specific product to have a strong story.

What it does need:

- agent-ready CLI
- agent-ready MCP
- local docs
- screenshots
- stable project structure

If you still want this claim later, necessary conditions are:

- distributable package or clear install flow
- command inventory
- docs
- versioning
- tests
- multi-agent framing so the repo does not look Claude-locked

### 7. "AI Style Designer is a real headline product"

Classification:

- `Good future goal`

Necessary to claim it strongly:

- real design tool entry point
- real plugin preview
- restyle -> preview -> accept -> persist -> rebuild/ship loop
- style pack storage/versioning
- undo/history
- diff inspection
- screenshot regression / parity validation
- docs that show how a plugin author actually uses it

Required gate sequence:

1. restyle loop works
2. persisted style pack works
3. real plugin preview works
4. regression tests prove it survives rebuild/reload
5. only then promote it as a headline product

Without those:

it should remain a design-system direction, not a headline product promise

---

## What Pulp Should Actively Shoot For

These are strong goals and worth building toward.

### A. Per-plugin automation profile

Goal:

- a plugin can optionally generate a CLI surface and/or MCP surface through `pulp_add_plugin()`

Why:

- this directly strengthens testing, automation, demos, and agent workflows
- it is aligned with Pulp's architecture
- it is more useful than a vague "AI-native" claim

### B. Translation/audit workflow for frontend input

Goal:

- Pulp can audit HTML/CSS/JS, design-tool exports, or AI-generated frontend code and tell the developer:
  - what maps directly
  - what maps with adaptation
  - what is unsupported
  - what conversion output it can generate

Why:

- this is the realistic bridge between "web-like authoring" and "native framework"
- it gives users a path without pretending to be a browser

### C. Productized design loop

Goal:

- run design tool
- preview style changes on a real plugin
- accept/reject
- persist tokens/style pack
- rebuild or hot-reload
- ship the locked-in result

Why:

- this is the point where the design story becomes obviously real

### D. Real reusable component layer

Goal:

- installable, documented, versioned components

Why:

- this gives Pulp leverage beyond low-level widgets
- it makes examples and design systems materially more useful

---

## MVP Requirements by Capability Cluster

### Automation MVP

To justify a stronger automation claim:

- CMake automation profile in `pulp_add_plugin()`
- generated plugin CLI target
- generated plugin MCP target
- one end-to-end example plugin using both
- tests for:
  - parameter introspection
  - audio file processing
  - screenshot tool path
  - enable/disable behavior
  - docs/examples consistency

### Design MVP

To justify a stronger design claim:

- stable `pulp design` entry point
- clear target location/build rules
- plugin preview against a real plugin
- token diff preview
- accept/persist/save flow
- tests for:
  - token schema
  - import/export round-trip
  - live preview screenshot regression
  - persistence across restart/rebuild

### Components MVP

To justify a stronger component claim:

- real populated registry
- non-stub installer
- component metadata format
- docs per component
- tests for:
  - install flow
  - compile/link flow
  - screenshot/example parity
  - dependency/version compatibility

---

## Existing Gaps That Matter Most

### Automation

- docs say `pulp mcp serve`; actual CLI does not expose that path
- repo MCP exists, but plugin MCP is not default-generated
- plugin CLI exists as a harness pattern, not build-system default

### Design

- `pulp design` points to a location/target shape that does not currently line up with the example implementation
- design tool is split between example and product identity

### Components

- `pulp add component` currently promises more than it delivers
- registry/install flow is not real enough for public emphasis

---

## Recommended Messaging Changes from This v2

### Say now

- Pulp includes automation capabilities for headless processing, screenshots, local docs, and AI-agent workflows.
- Pulp includes a design-system layer with tokens, inspector, and export, with a larger design tool direction in progress.
- Pulp includes reusable UI and app-building pieces, with a stronger component distribution story still to come.

### Do not say now

- every plugin automatically ships as CLI + MCP
- `pulp mcp serve` unless the CLI really exposes it
- shipped Claude plugin product
- AI Style Designer as current headline product

### Say later, after MVP

- Pulp can generate per-plugin automation surfaces
- Pulp Design is a first-class design environment
- Pulp Components is a real installable reusable layer

---

## Recommendation

Keep the public story centered on:

- `Pulp Core`
- `Pulp View`
- `Pulp Tools`

Treat the following as capability clusters until they cross a clear productization threshold:

- automation
- design
- reusable components

Do not try to force them into named systems early. Let them earn it.

The strongest strategic move is not to claim more.

It is to build the few missing productization layers that make the strongest future claims obvious and testable.

---

## Suggested Agent Prompt

```text
Use planning/pulp-story-hardening-and-systems-proposal-2026-03-28-v2.md as the primary strategy document for tightening Pulp's positioning and identifying what needs to become real before stronger claims are allowed.

Goals:
- separate permanent non-goals from bounded translation goals
- identify what automation, design, and reusable components still need before they can be promoted
- keep Pulp Core, Pulp View, and Pulp Tools as the primary public systems
- avoid support-nightmare claims

Focus:
- README.md
- VISION.md
- docs/concepts/overview.md
- docs/reference/cli.md
- docs/reference/capabilities.md
- docs/status/support-matrix.yaml
- tools/cmake/PulpUtils.cmake
- tools/mcp/pulp_mcp.cpp
- tools/plugin-cli/plugin_cli.hpp
- tools/add-component.py
- tools/components/registry.yaml
- examples/design-tool/

Tasks:
1. Audit the current docs and implementation against the v2 claim ladder.
2. Identify which claims are:
   - never goals
   - translation/adaptation goals
   - good future goals
   - optional future goals
3. For automation, design, and reusable components:
   - identify current reality
   - identify missing productization layers
   - recommend whether they stay nested or graduate to named systems
4. Propose the smallest MVP required for each stronger claim.
5. Do not change product code unless explicitly asked in a later step.
```
