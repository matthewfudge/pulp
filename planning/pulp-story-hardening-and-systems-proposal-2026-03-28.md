# Pulp Story Hardening and Systems Proposal

Date: 2026-03-28

## Purpose

Harden Pulp's outward story until it is specific, bounded, and defensible.

The goal is not to make Pulp sound smaller. The goal is to make it sound sharper:

- strong where it is already real
- explicit about where it is still forming
- clear about which parts are product surfaces versus architectural direction

The key positioning constraint:

> Pulp is a curated native compatibility layer for frontend-style plugin authoring, not a browser and not an AI demo shell.

---

## Current Reality

Pulp already has several real, differentiated surfaces:

- a real native plugin framework with format adapters, state, audio, MIDI, and headless processing
- a real native UI layer with both C++ and browser-shaped JS authoring
- a real CLI with project creation, build, test, status, validation, docs lookup, environment checks, packaging, upgrade, and cache management
- a real local-docs system
- a real repo-level MCP server
- a real screenshot tool and headless/UI test surface

But the repo story currently compresses these into one sweeping narrative. That creates avoidable support risk.

### Evidence

- `README.md` and `VISION.md` position Pulp as a broad all-in-one modern framework.
- `docs/reference/cli.md` describes a narrower CLI than `VISION.md` advertises.
- `tools/cli/pulp_cli.cpp` exposes `inspect`, `design`, `audit`, and `add`, but the public CLI reference currently centers the smaller set of documented commands.
- `tools/mcp/pulp_mcp.cpp` provides a repo-level MCP server with a useful but bounded tool set.
- `tools/plugin-cli/plugin_cli.hpp` provides a plugin CLI harness, but this is not the same thing as "every plugin automatically ships as a polished CLI + MCP tool."

---

## Narrative Problem

The current story bundles together four different things:

1. the framework itself
2. the native UI system
3. developer tooling
4. agent/AI workflows

Each is real to a different degree. When those are described as one monolithic "Pulp does all of this now" platform, the strongest parts lose credibility because the weaker parts pull them down.

The main overreach points are:

- "Every plugin compiles to a CLI and exposes an MCP server"
- "The CLI is the source of truth for all tooling"
- "Pulp ships with a Claude Code plugin that covers the full lifecycle"
- "AI Style Designer" language presented too close to present-tense product capability
- product names in `VISION.md` that do not yet map cleanly to repo surfaces users can find and run

The main under-communicated strengths are:

- the first-class native C++ UI path
- the hybrid C++ + JS authoring path
- the already-real local docs workflow
- the repo's actual headless processing and screenshot/testing story

Another important guardrail from Claude's review:

- agent/AI/MCP workflows should not be the lead product pillar
- they are useful developer-experience multipliers, not the primary reason someone adopts a plugin framework
- the engineering story should land first; the agent story should reinforce it

---

## Strongest Truthful Story

If Pulp wants to sound strong without sounding slippery, the top-level story should be:

> Pulp is a native audio plugin framework with three authoring modes for UI: native C++, browser-shaped JavaScript, or a hybrid of both. It ships with local-first tooling for scaffolding, testing, validation, and documentation, and it is structured to work well with AI agents through CLI and MCP surfaces.

That is already materially supported by the codebase.

What this does better than the current story:

- keeps the native framework primary
- makes JS important but optional
- frames agent support as structured tooling, not magic
- avoids implying that every aspirational AI/design surface is already fully productized

---

## What Pulp Should Claim Now

These are strong and supportable claims today.

### Framework

- Pulp is a native audio plugin framework with a modular C++ core.
- Pulp targets multiple plugin formats and supports headless processing and testing.
- macOS is the primary platform; Windows, Linux, iOS, and web remain honest secondary surfaces with uneven maturity.

### UI

- Pulp supports three native UI authoring modes: C++, JS/web-compat, and hybrid.
- The JS layer is browser-shaped and frontend-friendly, but it is not a browser engine.
- The UI renders natively through Pulp's own view/canvas/render stack rather than through WebView on the primary path.

### Tooling

- Pulp includes a local-first CLI for creation, build, test, validation, status, docs, and packaging workflows.
- Pulp includes local machine-readable docs and manifests that agents can consume without web calls.

### Automation

- Pulp has a repo-level MCP server and headless tooling that already support some AI-agent workflows.
- Pulp includes a plugin CLI harness pattern for turning processors into command-line tools.

---

## What Pulp Should Not Claim Yet

- Pulp should not claim to be a browser-compatible runtime.
- Pulp should not claim that arbitrary HTML/CSS/JS ports over unchanged.
- Pulp should not claim that React, Tailwind, or design-tool exports are first-class supported runtimes.
- Pulp should not claim that every plugin automatically ships as a polished CLI + MCP server unless the build system actually makes that true by default.
- Pulp should not claim a shipped, end-to-end Claude plugin product unless that integration is documented, distributable, and testable.
- Pulp should not present the AI Style Designer as a current headline product unless the real restyle -> preview -> accept -> persist -> ship loop is implemented and verified.

---

## Recommended Product Shape

Pulp should stop reading like one giant framework blob and start reading like a few clear product surfaces.

Important correction after Claude review:

- `Pulp Core`, `Pulp View`, and `Pulp Tools` earn top-level names now
- automation, design tooling, and reusable components should remain capabilities inside those systems until they are more mature
- six branded nouns is too much for a pre-1.0 framework

### 1. Pulp Core

What it is:

- the native plugin framework
- processor abstraction
- format adapters
- audio, MIDI, state, runtime, platform services

What to say:

> Pulp Core is the native framework layer for building audio plugins and apps.

This should be the primary identity.

### 2. Pulp View

What it is:

- retained native view tree
- widgets
- canvas
- themes and tokens
- browser-shaped JS authoring layer
- C++ UI authoring layer
- hybrid composition model

What to say:

> Pulp View is the native UI system behind Pulp. You can author in C++, JavaScript, or a mix.

This should be the second pillar.

### 3. Pulp Tools

What it is:

- `pulp` CLI
- local docs
- scaffolding
- validation
- packaging
- environment diagnosis

What to say:

> Pulp Tools is the local-first developer toolchain: scaffold, build, test, validate, inspect docs, and package.

This is real enough to brand now.

### 4. Automation Capabilities

What it is:

- headless host workflows
- plugin CLI harness
- screenshot tool
- repo MCP server
- future per-plugin automation surfaces

What to say:

> Pulp includes automation capabilities for headless processing, screenshots, CLI workflows, and AI-agent access.

Important guardrail:

This should be described as `usable but still consolidating`, not as a fully unified branded product yet.

### 5. Design Capabilities

What it is:

- design tokens
- inspector
- design export
- AI Style Designer direction
- future design tool

What to say:

> Pulp includes a design-system layer built around tokens, inspection, and export.

Important guardrail:

This should be explicitly framed as `partly real, partly roadmap` until the design tool and locked-in shipping workflow are complete.

### 6. Reusable Components

What it is:

- reusable higher-level app/plugin features
- preset browser
- waveform editor
- musical typing
- diagnostics
- other branded reusable items

What to say:

> Pulp also aims to ship reusable higher-level plugin/app building blocks once they are documented and stable enough to depend on.

Important guardrail:

Only brand this prominently once the components are actually shipped, documented, and stable enough to reuse.

---

## Recommended Messaging Hierarchy

### Homepage / README

Lead with:

1. Pulp Core
2. Pulp View
3. Pulp Tools

Then mention:

4. automation capabilities
5. design capabilities

Then optionally mention:

6. reusable components

This prevents the story from leading with its least-settled surfaces.

### One-Line Positioning

Recommended:

> Pulp is a native audio plugin framework with modular C++ foundations, a browser-shaped but native UI layer, and local-first tooling for automation and agent workflows.

Stronger but still safe:

> Pulp gives frontend-minded and audio-native developers a shared native plugin platform: C++ at the core, JavaScript where it helps, and no browser required on the primary UI path.

Claude's tighter version:

> A native audio plugin framework where you write C++, JavaScript, or both, and ship from one codebase.

### Three Audience Framing

For frontend-minded developers:

> Build many native plugin UIs with familiar DOM-like and CSS-like patterns, hot reload, and design tokens, without depending on WebView.

For audio/plugin developers:

> Keep DSP and platform control in native code, use JS only if it helps, and keep the audio side separate from the UI stack.

For framework contributors:

> Work in a modular, testable codebase with explicit boundaries between runtime, format, render, view, tooling, and automation.

---

## Guardrails for the CLI / MCP / Claude Narrative

### CLI

Position:

> The `pulp` CLI is the local-first entry point for everyday development workflows.

Do not imply:

- that every workflow described in `VISION.md` is already exposed as a mature CLI command
- that `pulp design`, `pulp inspect`, `pulp add`, and `pulp audit` are equally mature until docs/status say so

Needed tightening:

- make `docs/reference/cli.md` match `tools/cli/pulp_cli.cpp`
- assign honest status labels to each command
- separate "core commands" from "experimental tools"

### MCP

Position:

> Pulp already has a repo-level MCP server and AI-addressable tooling, with room to consolidate this into a stronger per-project/per-plugin automation story.

Do not imply:

- that the current MCP surface already equals the full plugin runtime surface
- that per-plugin MCP is automatic everywhere

Needed tightening:

- document the current `pulp-mcp` tool explicitly
- distinguish repo MCP from future per-plugin MCP

### Claude

Position:

> Pulp is being shaped to work well with Claude, Codex, and similar agents through CLI, MCP, local docs, screenshots, and structured repo surfaces.

Do not imply:

- that there is already a polished, shipped, cross-user "Claude plugin product" unless one actually exists

Needed tightening:

- change "Claude Code plugin" language to "Claude-ready workflows" or "Claude integration" unless the plugin is real, documented, and distributable
- keep the story multi-agent, not Claude-only

---

## Specific Tightening Recommendations

### 1. Split "works now" from "direction"

`VISION.md` is currently mixing current product language with roadmap language.

Recommendation:

- add a short "What Pulp is today" section
- add a short "Where Pulp is going" section
- move speculative or not-yet-unified items into the latter

### 2. Demote the one-binary automation claim

Current direction:

> One binary serves all three roles — plugin, CLI, and MCP server.

Better:

> Pulp supports plugin, headless CLI, and MCP-based automation workflows, with consolidation toward a cleaner per-plugin tool story.

This is much more believable given the current code.

### 3. Make native C++ UI explicit

This is one of Pulp's most under-marketed strengths.

Recommendation:

- always describe the UI system as `C++`, `JS/web-compat`, or `hybrid`
- never let the JS narrative imply that C++ UI is a fallback or second-class path

### 4. Reclassify AI Design as a design-system roadmap surface

Recommendation:

- present token system, inspector, and design export as current
- present AI Style Designer and the full design tool as emerging or experimental unless fully validated

### 5. Stop letting spec counts carry the story

The web-compat work is valuable, but "26 W3C specs" sounds closer to browser-compatibility than the project should promise.

Recommendation:

- use phrases like `curated web-style compatibility surface`
- use examples of what works well rather than spec count as the headline

---

## Proposed System Labels

If Pulp wants branded system names, this set is coherent and maps cleanly to the repo:

- `Pulp Core`
- `Pulp View`
- `Pulp Tools`

Recommendation:

Use the first set, and stop there for now.

Why:

- it matches repo architecture better
- it avoids overcommitting to roadmap surfaces
- it avoids teaching too many nouns before users have built anything
- automation, design, and reusable components can live under these three systems until they are mature enough to promote

---

## What Each System Can Honestly Promise

### Pulp Core

- robust native plugin foundation
- modular architecture
- multiple format targets
- headless testing and processing

### Pulp View

- native GPU-aware UI direction
- native C++ UI
- browser-shaped JS UI
- hybrid authoring
- token-based theming

### Pulp Tools

- scaffold, build, test, validate, docs, diagnose, package
- local-first developer workflows

### Automation Within Pulp Tools

- repo MCP server
- screenshot and view-inspection tooling
- headless processor harness
- path toward stronger per-plugin automation

### Design Within Pulp View and Pulp Tools

- tokens
- inspector
- export
- future AI restyling and design workflows

### Reusable Components Within Pulp View

- reusable branded higher-level building blocks when mature enough

---

## Docs That Should Be Aligned Next

- `README.md`
- `VISION.md`
- `docs/concepts/overview.md`
- `docs/reference/cli.md`
- `docs/reference/capabilities.md`
- `docs/reference/modules.md`
- `docs/status/support-matrix.yaml`

Second wave:

- docs for `pulp-mcp`
- docs for plugin CLI harness usage
- docs for `inspect`, `design`, `audit`, and `add`

---

## Recommended Near-Term Edits

These are the highest-value story-hardening edits.

1. Rewrite the top of `README.md` around three truths:
   - native plugin framework
   - native UI in C++ or JS or both
   - local-first CLI/docs/automation

2. Refactor `VISION.md` into:
   - current reality
   - roadmap systems
   - explicit maturity language

3. Change "Claude Code plugin" to "Claude integration" unless there is a real shipped plugin package.

4. Add one page that explains the system map:
   - Pulp Core
   - Pulp View
   - Pulp Tools
   - plus where automation and design capabilities currently sit inside those systems

5. Tighten CLI docs so current commands, statuses, and examples match the actual binary.

---

## Short Version

Pulp should sound less like one giant promise and more like a platform with a few named systems:

- Core
- View
- Tools

The narrative should lead with what is already coherent and defensible:

- native plugin framework
- native UI with C++ and JS authoring
- local-first tooling

Then it should layer in the more experimental but differentiating parts:

- agent workflows
- design tooling
- reusable component systems

That makes the story stronger, not weaker.

---

## Suggested Agent Prompt

```text
Harden Pulp's public story so it is strong, specific, and supportable.

Primary reference:
- planning/pulp-story-hardening-and-systems-proposal-2026-03-28.md

Goals:
- tighten Pulp's story around what is already real
- separate current systems from roadmap systems
- reduce support risk from overclaiming
- make the CLI/MCP/Claude/plugin narrative more coherent
- adopt only the system names that genuinely earn top-level status

Focus files:
- README.md
- VISION.md
- docs/concepts/overview.md
- docs/reference/cli.md
- docs/reference/capabilities.md
- docs/reference/modules.md
- docs/status/support-matrix.yaml

What to do:
1. Compare the current docs against planning/pulp-story-hardening-and-systems-proposal-2026-03-28.md.
2. Identify overclaims, underclaims, and conflicting claims.
3. Rewrite the docs so Pulp consistently reads as:
   - a native audio plugin framework
   - with a native UI system that supports C++, JS/web-compat, and hybrid authoring
   - with local-first tools for scaffold/build/test/validate/docs
   - with real but bounded automation/agent surfaces
4. Introduce the system framing where it helps:
   - Pulp Core
   - Pulp View
   - Pulp Tools
5. Treat automation, design tooling, and reusable components as secondary capabilities unless there is a strong reason to name them directly.
6. Be careful not to imply:
   - browser compatibility
   - full unified per-plugin CLI/MCP automation if it is not yet automatic
   - a shipped Claude plugin product unless it is truly present and documented
7. Tighten `docs/reference/cli.md` so it reflects the actual CLI command surface and uses honest status labels.
8. Keep the strongest truthful differentiators intact:
   - native plugin output
   - browser-shaped JS authoring without WebView on the primary path
   - first-class native C++ UI
   - strong local docs / agent workflows

Deliverables:
- updated docs listed above
- one short summary of:
  - claims removed or weakened
  - claims strengthened
  - system labels adopted or rejected
  - remaining gaps that still block stronger messaging

Constraints:
- do not change product code
- optimize for trust, not hype
- if a capability is real but not consolidated, describe it honestly instead of hiding it
```
