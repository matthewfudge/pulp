# Porting a JUCE plugin to a native Pulp UI — what it took, and how to make it easy

**From:** Matthew (Dream Date Designs) — a full end-to-end port of our shipping JUCE
plugin **Dream Date FX** to a native `pulp::view::DesignFrameView` UI, with the
existing JUCE/YSS engine kept as the backend. Goal of this note: capture exactly
where the friction was so **porting any JUCE plugin's UI to Pulp becomes easy**.

Everything here works today: AU / VST3 / Standalone, Release CPU on par with the
JUCE build (editor closed identical; ~+2% only while the GPU editor is open),
effects insert/remove + audible processing, macros + Delay's full control set
bound bidirectionally with automation, value readouts, native-window full-screen
+ scaling. Snapshot in `src/` (the Pulp view) and `juce-consumer/` (the JUCE-side
glue we had to hand-write).

---

## The shape of the integration

```
JUCE plugin (unchanged DSP/state)                 Pulp
┌────────────────────────────┐                    ┌───────────────────────────┐
│ AudioProcessor (APVTS,      │   param_key bind   │ DesignFrameView            │
│  effect chain, presets)     │◄──(static, mount)──│  (DdfxEditorView)          │
│                             │                    │                           │
│ PulpEmbedComponent          │   HAND-ROLLED:     │  - macros: param_key ✔    │
│  create_from_view(factory)  │◄─ ParamHost ──────►│  - advanced pages: ???    │
│                             │   EffectHost ─────►│  - effect insert/reorder  │
└────────────────────────────┘                    └───────────────────────────┘
```

The two arrows on the left that say **HAND-ROLLED** are the gap: everything the
`param_key` bridge doesn't cover, we had to build ourselves.

---

## What was easy (thanks to the recent SDK work)

- **`pulp_embed_create_from_view`** + the `PulpEmbedComponent(NativeViewFactory,…)`
  ctor (issue #5373) — mounting a hand-built `DesignFrameView` in the JUCE plugin
  was clean. This is the foundation; thank you.
- **`param_key` on `DesignFrameElement`** — for the *static* top-level controls
  (our 8 macro knobs + threshold) binding to the APVTS "just worked": set
  `element.param_key = "<APVTS id>"`, done. `boundParameterCount()==9` first try.
- **`DesignFrameView` scales its design to its view bounds** — so UI scaling is
  free *if* you size the embed to the window.

## Where the friction was (in priority order)

### 1. `param_key` can't bind dynamic / paged controls  ← biggest gap
`param_key` is harvested **once at mount**. Our advanced effect pages are dynamic
(the controls, and which params exist, change with the loaded effect and the open
page), so none of them could use `param_key`. We hand-rolled a **runtime host
param accessor** the view calls every frame / on drag:

```cpp
// ddfx_host.hpp — the surface we needed but had to invent
struct EffectHost {
    virtual float       getEffectParam    (int slot, const std::string& id) = 0;
    virtual void        setEffectParam    (int slot, const std::string& id, float v) = 0;
    virtual float       getEffectModDepth (int slot, const std::string& id) = 0;
    virtual void        setEffectModDepth (int slot, const std::string& id, float v) = 0;
    virtual std::string getEffectParamText(int slot, const std::string& id) = 0; // "500 ms"
    ...
};
```
**Ask:** expose a first-class **runtime host-param accessor** to native views
(get / set / begin+end gesture / **display-text** by key), so dynamic controls
bind the same way static ones do — no per-plugin bridge, no polling glue.

### 2. Params aren't structure — no host *action* bridge
Insert an effect into a slot, remove it, reorder, load a preset — none of these
are parameters, and the embed only bridges values. We hand-rolled the structural
half of `EffectHost` (`setEffectType`, `getEffectType`, …) forwarding to the
processor. **Ask:** a general **host action/command channel** the native view can
call (opaque command + args, or registered callbacks), so structural UI (racks,
tabs, browsers) doesn't each reinvent this.

### 3. Value formatting
We route each control's normalized value through the *effect's own formatter*
(`"500 ms"`, `"43%"`, `"Sync"`) via `getEffectParamText`. **Ask:** standardize a
`display_text(key)` on the param accessor above so readouts are one call.

### 4. Heavyweight-NSView interaction hazards (cost us the most debugging)
The embed is an opaque heavyweight `NSView` over the JUCE editor. Consequences we
hit and had to solve by hand:
- **Covered JUCE affordances.** The JUCE resize-corner grip (and would-be
  tooltips / native menus) sit *under* the NSView and can't be clicked through.
  We had to make the editor **host-window-resizable with a locked aspect** and let
  the DAW resize the window (the embed tracks `getLocalBounds`, so the design
  scales). A lightweight component can never overlay the embed.
- **Reopen double-render.** `PulpEmbedComponent` is constructed at base (1×) size
  and only resizes to the editor's real bounds on the *first host `resized()`*.
  Reopening while zoomed showed a base-size copy over the scaled one until you
  nudged the window. Fix was `setBounds(getLocalBounds())` right after mount.
  **Ask:** size the embed to the host component's current bounds on open by
  default, or document this loudly.
- **Debug Skia/Dawn is ~3× CPU.** Obvious in hindsight, but a Debug embed build
  reads as a huge regression. Worth a prominent "measure in Release" note.

**Ask:** a **"resizable native-view embed" helper** (host-window resize → design
scale, aspect handling, size-on-open) + a short doc on the NSView-overlay
implications, would remove the single biggest source of our debugging.

### 5. Docs / example for the *whole* native pattern
`create_from_view` exists but the end-to-end recipe — native view + param bridge +
actions + resize + Release — had to be reverse-engineered. This folder is that
example; a canonical one in the SDK would help the next consumer.

---

## What made *our* port faster (reusable pattern)

Once the two hand-rolled bridges existed, the per-plugin cost dropped to: lay out
the `DesignFrameView`, and supply a param map. The bridges (`EffectHost`/param
accessor, native mount, formatter readout) are plugin-agnostic — the next YSS
instrument reuses them. If the SDK provided #1–#4, that reusable layer would be
*yours*, and porting would be: build the design, tag controls with keys, done.

See `src/` (Pulp view) and `juce-consumer/` (the JUCE-side glue: the standalone
window + `ScalableEditorHost` resize seam). Happy to walk through any of it.
