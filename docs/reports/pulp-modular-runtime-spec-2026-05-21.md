# Pulp Modular Runtime Spec

**Date:** 2026-05-21
**Status:** Planning draft after the node ABI spike
**Scope:** Future work consideration; no implementation commitment

## Thesis

Pulp's node boundary makes custom DSP units buildable. The modular runtime
makes those units composable, saveable, inspectable, editable, reusable, and
distributable.

The ABI-shaped node API answers: "How does one DSP unit plug into the host?"
The modular runtime answers: "How do many units become an instrument, effect,
patch, library, and distribution ecosystem?"

The intended order is:

1. ABI-shaped node API
2. Graph execution semantics
3. Patch format
4. Built-in node catalog
5. Visual patch editor
6. Preset/assets/library system
7. Sampler/instrument layer
8. Distribution/marketplace story

This order matters. A stable binary node ABI without graph semantics is only a
loadable callback. A graph without a saved patch format is not reusable. A
patch format without a catalog and state/assets story is not a real ecosystem.
A visual editor without deterministic execution and inspection tools becomes
hard to trust. Distribution should come last because it freezes user
expectations.

## Design Posture

The current custom-node lane stays experimental and source-rebuild based. Pulp
should let developers build real custom nodes today, while documenting that API
breakage is allowed between releases and binary compatibility is not promised.

Future specs should keep three layers separate:

| Layer | Purpose | Stability |
|-------|---------|-----------|
| Internal runtime | Efficient graph execution, scheduling, scratch buffers, serialization internals. | Private; may change freely. |
| Experimental SDK | Source-level custom nodes, patch authoring, editor hooks, inspection APIs. | Public enough for feedback; rebuilds expected; breaking changes allowed. |
| Stable distribution boundary | Precompiled node binaries, patch packs, sample libraries, signed packages. | Deferred until real demand and compatibility tests exist. |

The modular runtime should be shaped so the future stable boundary is obvious
and testable, but not frozen.

## What To Spec Next

### 1. Patch/Graph Format Spec

A saved format for nodes, edges, params, modulation routes, presets, assets,
versioning, migration, and undo/redo. This is the Reaktor/Max/Kyma foundation.

The spec should define stable patch identity, node instances, typed edges,
parameter defaults, automation state, macro mappings, versioned migrations,
missing-node and missing-asset placeholders, undo/redo transaction boundaries,
and deterministic serialization.

Key questions:

- Should patch files be JSON-first for readability or binary-first for load
  speed?
- Is a patch a single document, or a package with manifest plus assets?
- How much UI layout belongs in the patch versus editor-side metadata?

### 2. Node Catalog Spec

Define the built-in node taxonomy. The catalog should be boring, complete, and
stable enough that patch authors can rely on it.

Initial families:

- oscillator;
- filter;
- envelope;
- LFO;
- mixer;
- math;
- delay;
- sampler;
- sequencer;
- MIDI;
- utility;
- analysis;
- modulation;
- routing.

Each catalog entry should declare a stable type ID and version, port types and
rates, parameter descriptors, state schema, latency/tail behavior,
deterministic behavior requirements, RT-safety guarantees, and optional
capabilities.

Key questions:

- Which nodes must be built-in versus shipped as optional packs?
- How much of the catalog should be implemented in C++ versus generated DSP or
  source-built node packs?
- Do built-in nodes use the same experimental node-facing API as third-party
  nodes to keep the model honest?

### 3. Signal + Event Semantics Spec

Define audio-rate versus control-rate signals, sample-accurate events,
MIDI/event queues, modulation timing, parameter smoothing, block boundaries,
latency, feedback loops, and deterministic graph execution.

The spec should define audio-rate buffers, control-rate values, event ordering,
MIDI 1.0, MIDI 2.0/UMP, MPE, sysex, custom events, modulation summing,
clamping, parameter-domain mapping, smoothing ownership, block-boundary rules,
latency/PDC, deterministic ordering, denormal handling, NaN handling, and
silence propagation.

Key questions:

- Should event streams be sparse-only, or should dense audio-rate modulation
  have a separate buffer path?
- Are feedback edges always one block delayed, or can nodes request explicit
  delay lengths?
- What should the host do when a node violates declared RT rules?

### 4. Patch Editor Spec

Define the visual graph UI: node browser, cables, inspector, scopes/meters,
drag/drop, grouping, macro controls, reusable subpatches, copy/paste, search,
and keyboard shortcuts.

The editor should be an instrument for experts, not only a diagram. It should
make signal flow inspectable and mistakes obvious.

The spec should define node browsing, cable rendering, type/rate compatibility
feedback, param and port inspectors, inline meters, scopes, event traces,
latency badges, grouping, macro controls, subpatches, reusable templates,
copy/paste semantics, keyboard navigation, accessibility, and conflict handling
for missing or incompatible nodes.

Key questions:

- Should the patch editor be built on Pulp's native view/canvas stack, a web
  bridge, or both?
- How do subpatches map to saved patch files and reusable libraries?
- What editor state is portable across hosts and screen sizes?

### 5. State/Preset/Asset Spec

Define how patches save, how presets reference samples/assets, how missing
assets are handled, how state migrates, and how user libraries are packaged.

The spec should define patch state versus preset state versus runtime state,
asset identity, content hashes, relative paths, embedded assets, missing asset
placeholders, relinking flow, preset banks, sample/library metadata, licensing,
attribution, migration hooks, package layout, cache invalidation, and
deduplication.

Key questions:

- Should assets be copied into patch packages by default, or referenced by
  content hash from a library?
- How much state should be human-readable?
- How should user edits to vendor libraries be layered?

### 6. Sampler/Instrument Spec

This is the Kontakt-ish lane: sample zones, key/velocity mapping, round robins,
loop points, envelopes, modulation matrix, disk streaming, sample cache, and
library packaging.

The spec should define zones, groups, key ranges, velocity ranges,
articulations, round robin behavior, random behavior, legato, release triggers,
choke groups, loop points, crossfades, root pitch, tuning, time stretch,
envelopes, filters, LFOs, modulation matrix, macros, scripting hooks, disk
streaming, prefetch size, cache budget, voice stealing, sample format support,
and decode-thread behavior.

Key questions:

- Is the sampler a built-in node family, a library layer above the graph, or
  both?
- How much Kontakt/SFZ/SF2-style import compatibility is worth supporting?
- What parts must be hard real-time and what parts can run on worker threads?

### 7. Scripting/Expression Spec

Optional but important: lightweight expressions or scripting for node logic, UI
bindings, modulation transforms, macro behavior, and generated DSP.

The spec should define expression language scope, determinism rules,
compile-time versus runtime evaluation, safe access to params/events/buffers,
RT-safe restrictions, allocation policy, generated DSP hooks, sandboxing,
capability limits, debugging, and source mapping.

Key questions:

- Is scripting for control logic only, or can it produce DSP?
- Should Pulp lean on Cmajor/Faust-style generated DSP, JS, WASM, or a small
  native expression language?
- How does script code version and migrate inside patches?

### 8. Debugging/Inspection Spec

Define graph profiler, CPU per node, allocations, RT violations, buffer
visualizer, event trace, parameter automation trace, denormal detection, and
graph validation.

The spec should define per-node CPU and wall-time measurement, allocation and
lock detection in audio-thread paths, buffer capture, waveform/spectrum
inspection, event tracing, automation tracing, latency/PDC visualization, graph
validation, denormal/NaN/clipping diagnostics, and exportable diagnostic
reports.

Key questions:

- What instrumentation is always-on versus debug-only?
- How can profiling avoid perturbing real-time behavior?
- How much history should the inspector retain?

### 9. Distribution Spec

Define how node packs, patch packs, sample libraries, and third-party modules
ship. Source-built first, precompiled later.

The spec should define source-built node pack layout, patch pack manifests,
sample library manifests, dependency and compatibility declarations, package
signing, provenance, trust, quarantine policy, updates, rollback, migration,
license metadata, attribution, marketplace metadata, search, preview, rating,
compatibility filters, and future precompiled binary module layout if
`pulp_node_v1` becomes real.

Key questions:

- What is installable on iOS/AUv3/sandboxed targets where dynamic libraries may
  be restricted?
- Does the marketplace distribute source, generated code, binaries, patches,
  assets, or all of the above?
- What security model protects hosts from untrusted nodes and scripts?

## Versioning Model

Each spec in this stack should carry:

- `draft.N` version while experimental;
- an explicit changelog;
- compatibility terms for source/API, serialized format, and binary behavior;
- test requirements before a draft can advance;
- a list of intentionally deferred surfaces.

Draft versions are planning tools, not compatibility guarantees. A draft can be
renamed, reshaped, or abandoned. A stable boundary should require conformance
tests, cross-platform fixtures, migration tests, RT-safety tests,
package/install tests, and a public deprecation policy.

## Immediate Next Step

After `pulp_node_v1.draft.0`, write the Patch/Graph Format Spec. It should be
the next load-bearing document because every later feature depends on saved
graph identity, migration, unresolved-node handling, asset references, and
deterministic execution.

Do not implement a marketplace, binary loader, sampler ecosystem, or visual
patch editor before the patch/graph format and execution semantics are clear.
