#pragma once

#include <cmath>

namespace pulp::format::detail {

/// What a per-block host↔store parameter reconcile decided to do for one
/// parameter. See @ref reconcile_host_param.
enum class ParamSyncAction {
    None,            ///< host and store agree (or a UI edit is pending push) — leave the store.
    AdoptHostValue,  ///< host genuinely moved the param → write it into the store.
};

struct ParamSyncDecision {
    ParamSyncAction action;
    float value; ///< the value to apply (store value for adopt, store value to push).
};

/// Sentinel for "no host value observed yet" — the first reconcile after
/// construction/Initialize adopts whatever the host currently holds (default,
/// or restored-state value). NaN compares unequal to everything, so the first
/// `reconcile_host_param` call always takes the AdoptHostValue branch.
inline float host_param_unseen() { return std::nanf(""); }

/// Reconcile ONE parameter between the host's parameter cache (AU `Globals()`,
/// VST3 host queue, CLAP host) and the plugin `StateStore`, once per audio
/// block. `last_host` is the value this reconciler last knew the host to hold;
/// it is audio-thread-only state (no locking) and is updated in place.
///
/// The bug this fixes: an unconditional per-block `host_value → store` pull
/// reverts UI-thread parameter edits (drag, type-in, toggle) on the very next
/// block, because the UI writes the store but the host's parameter cache hasn't
/// been told. Logic/host generic controls work precisely because the host
/// writes its own cache. The reconcile distinguishes the two directions:
///
///   - host_value != last_host  → the host genuinely moved it (automation
///     playback, host generic UI, preset/state load) → AdoptHostValue.
///   - otherwise → None: leave the store ALONE. This is what prevents the
///     snap-back — a UI edit that diverged the store from the (stale) host
///     value is preserved here, NOT clobbered. The edit reaches the host
///     separately, on the MAIN thread (the adapter pushes it via
///     AudioUnitSetParameter from a store listener). Critically, this function
///     performs NO host-parameter write itself: the only host↔plugin coupling
///     on the audio thread is the GetParameter read by the caller plus the
///     set_value_rt the caller does on AdoptHostValue. Writing the host's
///     parameter store (Globals()->SetParameter / AudioUnitSetParameter) from
///     the render thread can contend with the host's own parameter locks and
///     stall the render thread in some hosts.
///
/// Adopting a host change updates `last_host` to the host value. Once the
/// main-thread push lands (Globals now holds the UI value), the next call sees
/// host_value != last_host and adopts it — a no-op against the store, which
/// already holds that value — so the loop converges without ever pushing from
/// the audio thread.
inline ParamSyncDecision reconcile_host_param(float host_value,
                                              float store_value,
                                              float& last_host) {
    // NaN-safe: when last_host is the unseen sentinel, `!=` is true and we
    // adopt the host value. When both are real numbers, exact float equality
    // is correct here — these are the same float round-tripped through the
    // host/store, not the result of arithmetic.
    const bool host_moved = !(host_value == last_host);
    if (host_moved) {
        last_host = host_value;
        return {ParamSyncAction::AdoptHostValue, host_value};
    }
    return {ParamSyncAction::None, store_value};
}

} // namespace pulp::format::detail
