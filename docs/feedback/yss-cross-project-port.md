# Pulp Port — Daniel Feedback

**Status:** For Daniel Raffel — 2026-07-05. The external-facing extract from the cross-project
research. Two things: (1) the **5 open questions** that gate the port, (2) the
**contribute-back backlog** (places JUCE covers something Pulp doesn't, that we could build and
push upstream).

Companions (for context, not required reading for Daniel): `Cross-Project Pulp Port - Full JUCE
Exit.md`, `Pulp Port - Execution Manifests.md`, `Framework-Neutral State and Parameter Model.md`,
`StepGrid Pulp Port and the Gesture-Canvas Requirement.md` — all in this dir.

**Context for Daniel:** We're porting three codebases — the **yss** shared library plus
**DDIF** (Dream Date) and **Triaz** (Wave Alchemy) products — onto Pulp, in two phases: (1) Pulp
UI on a JUCE backend (`pulp-embed-juce`, the release path — DDFX is already done this way), then
(2) full `pulp::format::Processor` (zero JUCE). The architecture is a **framework-neutral core**
in yss with Pulp as the first swappable binder (and a backdoor for future frameworks). **None of
our products have shipped users**, so we have no backward-compat constraints — clean break
everywhere.

---

## Part 1 — The 5 questions (what gates us)

### Q1. Gesture canvas — is `Kind::custom` the blessed pattern? (the Triaz go/no-go)
We need a `DesignFrameView` element that (a) paints procedurally from app data each frame AND
(b) receives raw pointer events (down/drag/up with x/y **and modifiers**) that the app turns into
arbitrary multi-value writes — coexisting with retained `param_key`-bound elements. The pieces
appear present: `DesignFrameElement::Kind::custom` + `register_design_control_factory()` +
virtual `View::paint(Canvas&)` + `on_mouse_event()` (full pointer event: x/y + modifiers +
pointer ID) + `set_pointer_capture()` + the Canvas 2D command API. **DDFX already does
immediate-mode procedural draw + multi-value gestures** (master-scope threshold drag, LFO
dual-handle depth sliders), which is strong evidence this works. **Confirm: is
`Kind::custom` + `paint` + `on_mouse_event` a *supported* pattern (vs an escape hatch), and how
does a `custom` element officially compose with retained `param_key` elements?** If yes, the
Triaz StepGrid (16-step velocity grid + a "grow" vine slider) is implementation work, not
blocked. If no, what does Pulp plan to provide? *(Our StepGrid PRD §3–§5 has the full spec.)*

### Q2. Base-vs-modulated — uniform or per-target?
`modulation_lane.hpp` exposes modulation lanes. We have products that need a uniform
`base_value + Σ_modulation = effective_value` read across **all** params (so mod rings / meters
bind uniformly). **Is `modulation_lane` a uniform read across all params, or only on params
explicitly declared as modulation targets?** (Our audit reads `ModulationTarget::modulatable` as
per-target — confirm?) Triaz specifically lacks base-vs-modulated today (motion writes
destructively) and must gain the pair before Pulp mod rings can bind.

### Q3. Observable change channel — `clone_synced()` for a non-Pulp producer?
We need message-thread fan-out from a producer that does **not** itself live in Pulp (the yss
engine writes; Pulp views observe). **Is `StateTree::clone_synced()` the recommended pattern for
this, or should the binder own a copy + reconcile?** Our backdoor requirement means we'd like to
define a **neutral** change channel (per-key + per-subtree subscription) and have the Pulp binder
back it with Pulp's listeners — confirm that's viable and there's no forced coupling to
`StateTree` semantics.

### Q4. StateRecord ↔ StateStore bridge — `state_migration` the right seam?
We have a neutral `StateRecord` (named-children tree, typed leaves: bool/int/double/string/blob/
array) + binary/JSON codecs. We'll map it onto Pulp's `StateStore`/`StateTree` for the full-port
path. **Is `state_migration.hpp` the right seam for reading our one-shot-converted legacy files,
or should the binder bypass it?**

### Q5. Coupling principle — confirm no Pulp-type leak into the neutral core
Our neutral core must compile with **no `<pulp/...>` includes** (so a future non-Pulp binder is
cheap). **Can you flag any of your APIs that would force a leak?** Specifically: if
`StateTree`'s semantics are the only practical change channel, the binder may need to own a copy
and expose neutral types upward. (Q1–Q5 all reduce to: we want Pulp to be a *binder*, not an
*owner*, of the model.)

---

## Part 2 — Contribute-back backlog (JUCE covers, Pulp doesn't — yet)

Ranked by impact on a Triaz/DDIF port. We're happy to build any of these and PR them upstream.

### 1. MTS-ESP / microtuning hook — `signal/tuning/` (MEDIUM impact, **top candidate**)
Pulp has no microtuning story today (no `.scl`/`.kbm` parser, no MTS-ESP OSC client, no per-note
frequency table; `core/osc/` is generic OSC, not MTS-ESP). Our products care about microtuning.
**Proposal:** a `signal/tuning/microtuner.hpp` exposing a per-note frequency table sourced from
MTS-ESP (OSC client) or a `.scl`/`.kbm` parser, with `get_frequency(note, channel)`. Until then
we link MTS-ESP directly.

### 2. MIDI-learn helper — `state/midi_learn.hpp` (MEDIUM)
`view/midi_binding.hpp` is one-way (UI→MIDI). A bidirectional **learn** primitive ("hover a
control, jiggle a CC, persist the map") is something every product rebuilds by hand.
**Proposal:** a `MidiLearn` helper that maps an inbound `MidiBuffer` CC to a hovered `ParamID`
and persists the map.

### 3. Richer `var` — `state::PropertyValue` adds Array/Object/Node (LOW for now)
Pulp's `PropertyValue = variant<monostate,bool,int64_t,double,string>` (no Array/Object/
`var::Native`). Nested arrays-of-records must be child StateTree nodes; arrays-of-primitives need
an encoding. Low impact for our parameter/flat-state use; medium for structured plugin state
(mod matrices as data). **Defer until a concrete need surfaces.**

### 4. Block-style DSP adapter — `signal/block_adapter.hpp` (LOW, cosmetic)
Pulp DSP is per-sample (`float process(float)`). Our yssDSP uses JUCE's block-template style
(`AudioBlock`/`ProcessSpec`/`ProcessContextReplacing`). Migrating a JUCE DSP graph to per-sample
is high-effort, no benefit. **Proposal:** an adapter wrapping `float process(float)` processors
into a `juce::dsp`-style `process(AudioBlock)` API. Mostly cosmetic — Pulp's per-sample style is
fine for greenfield; this is only for JUCE-origin ports. (For our Phase-2 we'd rather define our
own neutral `ProcessSpec` + `AudioSpan` than port JUCE's block template — so this is lowest
priority.)

### 5. Cream theme as a `Theme::preset` (LOW)
We have a cream theme for one product. We can keep it product-local as a `Theme` JSON, but if
there's appetite, contributing it as a `Theme::preset` alongside `dark`/`light`/`pro_audio` is
easy.

---

## Part 3 — What Pulp already nails (no action needed; for our confidence)

So Daniel knows we're not asking for things that already exist — verified by reading headers:

- **Parameter primitive (the "APVTS equivalent"):** `StateStore` (layout + RT value store) +
  `ParamInfo`/`ParamRange` (bit-identical skew to `NormalisableRange`) + `ParameterEdit`
  (multi-param gestures) + `view::bind_parameter()` (the slider-attachment) + gestures + RT/Main
  listeners + versioned `state_migration`. Drop-in, richer than APVTS.
- **Formats:** single `pulp_add_plugin(... FORMATS vst3 auv3 au_v2 clap lv2 aax standalone)`
  from one `Processor`. Plus ARA 2.x + iOS AUv3.
- **DSP `core/signal`:** 60+ processors; **exceeds** JUCE+SoundTouch (realtime pitch/time phase
  vocoder, STN decomposer, spectral envelope shifter).
- **Streaming sampler:** `audio/streaming_sample_source.hpp` + zone/keymap/voice allocator —
  **supersedes** JUCE's whole-sample `SamplerSound`/`SamplerVoice`.
- **Plugin hosting:** `core/host/*` (VST3/AU/AUv3/CLAP/LV2 scanner + host) — far beyond JUCE for
  our `yssPluginHost` use.
- **Crypto/licensing:** `runtime/{license,crypto,identity}.hpp` — beyond JUCE (we may adopt it).
- **Gesture canvas:** `Kind::custom` + factory + `View::paint` + `on_mouse_event` +
  `set_pointer_capture` + Canvas 2D — appears first-class (Q1 just needs confirmation).
- **Concurrency:** `runtime/AbstractFifo` + `SpscQueue`/`TripleBuffer`/`Seqlock` — drop-in
  superset of `juce::AbstractFifo`.

---

*Daniel — happy to jump on a call to go through Q1–Q5. The contribute-back items (1–2 especially)
we can scope and start on whenever you're ready. — Matthew*

---

## Addendum (2026-07-06) — Q1 evidence from our side: PASS-leaning

Since writing Q1 we did an API-level recon against the SDK (v0.551 pin): everything the gesture
canvas needs appears present in the View API — `virtual void paint(canvas::Canvas&)`
(`view.hpp:171`, procedural per-frame draw), `set_pointer_capture`/`release_pointer_capture`/
`has_pointer_capture` (`view.hpp:207-209`, the W3C setPointerCapture model), input events with
modifiers + pointer ID (`view.hpp:176`), and `Kind::custom` overlay factories
(`design_frame_view.hpp:122-169`). All five hard points of our StepGrid spec are expressible.
So Q1 reduces to: **confirm this is the blessed, supported pattern** (we still owe ourselves the
empirical 4-gesture + 60 fps Release spike, but the API question looks answered). Q2–Q5 stand
as written.
