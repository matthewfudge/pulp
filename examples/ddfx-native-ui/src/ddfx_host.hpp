#pragma once
#include <string>

// Bridge from the native DDFX editor view to the host's effect-chain ENGINE.
// Structural actions — which effect occupies a slot, order, bypass, presets —
// are NOT APVTS parameters, so they can't ride the param_key value bridge. The
// view calls these instead; the JUCE plugin implements them by forwarding to
// InstrumentProcessor (setEffectTypeForSlot, reorderEffect, …). Null in the
// standalone sandbox (there the rack is purely visual local state).

namespace knobpg {

struct EffectHost {
    virtual ~EffectHost() = default;

    // FX slots are 1..6. `displayName` is the Pulp effect label ("Delay",
    // "Reverb", "Bit Crusher", …); "" or "---" clears the slot.
    virtual void setEffectType(int slot, const std::string& displayName) = 0;

    // Current effect display name in slot 1..6 ("" if empty), so the view can
    // reflect the engine's real chain (e.g. after loading a preset / session).
    virtual std::string getEffectType(int slot) = 0;

    // Effect PARAM values by engine param id ("time","feedback","mix",…), slot
    // 1..6, normalized 0..1. `get` reflects the engine (incl. host automation);
    // `set` writes the engine + notifies the host (records automation) and
    // un-bypasses. `paramId` is the FxControl's engine id, not the APVTS key —
    // the plugin resolves the mapping. Mod-depth ring uses the *ModDepth pair.
    virtual float getEffectParam    (int slot, const std::string& paramId) = 0;
    virtual void  setEffectParam    (int slot, const std::string& paramId, float value) = 0;
    virtual float getEffectModDepth (int slot, const std::string& paramId) = 0;   // -1..1
    virtual void  setEffectModDepth (int slot, const std::string& paramId, float depth) = 0;

    // Formatted display value for a control (the effect's own formatter — "500 ms",
    // "43%", "Sync"…), shown as a readout on hover/drag. "" if none.
    virtual std::string getEffectParamText (int slot, const std::string& paramId) = 0;
};

} // namespace knobpg
