// Host↔store parameter reconcile — the fix for UI parameter edits reverting
// in a real host (XY snap-back, type-in not taking, toggles not engaging).
//
// The format adapters pull the host's parameter cache into the StateStore once
// per audio block. The original code did this UNCONDITIONALLY, so a UI-thread
// edit (drag/type/toggle) — which writes the store but not the host's cache —
// was reverted on the very next block. The host's own generic controls worked
// because the host writes its own cache; the plugin's editor did not.
//
// These tests model the adapter's exact per-block loop with a fake host
// parameter cache + a real StateStore + a real ParameterEdit (the same gesture
// path the editor uses), so the regression is caught here instead of in Logic.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <pulp/format/detail/param_host_sync.hpp>
#include <pulp/state/parameter_edit.hpp>
#include <pulp/state/store.hpp>

#include <unordered_map>

using namespace pulp;
using pulp::format::detail::ParamSyncAction;
using pulp::format::detail::reconcile_host_param;
using Catch::Matchers::WithinAbs;

namespace {

// A minimal stand-in for the AU host's parameter cache (Globals()) plus the
// adapter's audio-thread reconcile state. `render_block()` is a faithful copy
// of the adapter's per-block host↔store sync using the real reconcile_host_param.
struct FakeHost {
    state::StateStore& store;
    std::unordered_map<state::ParamID, float> host_cache;             // what the host holds
    std::unordered_map<state::ParamID, float> last_host;             // reconciler state
    std::unordered_map<state::ParamID, int> recorded_changes;       // host automation writes seen

    explicit FakeHost(state::StateStore& s) : store(s) {
        for (const auto& p : store.all_params()) {
            host_cache[p.id] = p.range.default_value;
            last_host[p.id] = pulp::format::detail::host_param_unseen();
        }
    }

    // The host moves a parameter (automation playback / generic UI / preset).
    void host_writes(state::ParamID id, float v) { host_cache[id] = v; }

    // One audio block (render thread): adopt genuine host moves into the store;
    // never write the host or clobber a pending UI edit. Exactly the adapter's
    // ProcessBufferLists reconcile.
    void render_block() {
        for (const auto& p : store.all_params()) {
            const float hv = host_cache[p.id];
            const float sv = store.get_value(p.id);
            auto d = reconcile_host_param(hv, sv, last_host[p.id]);
            if (d.action == ParamSyncAction::AdoptHostValue)
                store.set_value(p.id, d.value); // (set_value_rt in the RT adapter)
        }
    }

    // Main thread: the adapter's ui_push_listener_ — push any store value the
    // host doesn't yet have into the host (AudioUnitSetParameter), recording it.
    void pump_ui_to_host() {
        for (const auto& p : store.all_params()) {
            const float sv = store.get_value(p.id);
            if (host_cache[p.id] != sv) {
                host_cache[p.id] = sv;
                recorded_changes[p.id]++;
            }
        }
    }
};

void populate(state::StateStore& s) {
    s.add_parameter({.id = 1, .name = "Pitch", .unit = "st", .range = {-12.0f, 12.0f, 0.0f, 0.0f}});
    s.add_parameter({.id = 2, .name = "Formant", .unit = "st", .range = {-12.0f, 12.0f, 0.0f, 0.0f}});
    s.add_parameter({.id = 3, .name = "Freeze", .unit = "", .range = {0.0f, 1.0f, 0.0f, 1.0f}});
}

} // namespace

TEST_CASE("reconcile_host_param: first block adopts the host value",
          "[format][param-sync]") {
    float last = pulp::format::detail::host_param_unseen();
    auto d = reconcile_host_param(/*host=*/0.0f, /*store=*/0.0f, last);
    REQUIRE(d.action == ParamSyncAction::AdoptHostValue);
    REQUIRE(last == 0.0f);
}

TEST_CASE("reconcile_host_param: a UI edit is left alone on the render thread (None)",
          "[format][param-sync]") {
    float last = 0.0f; // host known to hold 0
    // UI wrote 7 into the store; host still holds 0. The render thread must NOT
    // touch it (no clobber, no host write) — the edit is pushed to the host on
    // the main thread. last_host stays at the host's value (0), unchanged.
    auto d = reconcile_host_param(/*host=*/0.0f, /*store=*/7.0f, last);
    REQUIRE(d.action == ParamSyncAction::None);
    REQUIRE_THAT(last, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("reconcile_host_param: genuine host automation wins over the store",
          "[format][param-sync]") {
    float last = 7.0f;
    auto d = reconcile_host_param(/*host=*/3.0f, /*store=*/7.0f, last);
    REQUIRE(d.action == ParamSyncAction::AdoptHostValue);
    REQUIRE_THAT(d.value, WithinAbs(3.0f, 1e-6f));
}

TEST_CASE("XY drag value survives the next render block (no snap-back)",
          "[format][param-sync][issue-pending]") {
    state::StateStore store;
    populate(store);
    FakeHost host(store);
    host.render_block(); // first block: adopt defaults (0,0)
    REQUIRE_THAT(store.get_value(1), WithinAbs(0.0f, 1e-6f));

    // UI drag: ParameterEdit brackets the writes (the editor's exact path).
    {
        state::ParameterEdit edit(store);
        edit.begin({1, 2});
        edit.set(1, 7.0f);
        edit.set(2, -5.0f);
        edit.finish();
    }
    REQUIRE_THAT(store.get_value(1), WithinAbs(7.0f, 1e-6f));

    // A render block BEFORE the main-thread push must still not revert (the
    // render thread leaves a diverged store alone).
    host.render_block();
    REQUIRE_THAT(store.get_value(1), WithinAbs(7.0f, 1e-6f));   // was snapping to 0
    REQUIRE_THAT(store.get_value(2), WithinAbs(-5.0f, 1e-6f));

    // The main-thread listener pushes the edits to the host (records automation).
    host.pump_ui_to_host();
    REQUIRE(host.recorded_changes[1] == 1);
    REQUIRE(host.recorded_changes[2] == 1);

    // Stable across further blocks (adopt is now a no-op against the store).
    host.render_block();
    host.render_block();
    REQUIRE_THAT(store.get_value(1), WithinAbs(7.0f, 1e-6f));
}

TEST_CASE("Both pitch and formant can be set to 0.0 without one snapping",
          "[format][param-sync][issue-pending]") {
    state::StateStore store;
    populate(store);
    FakeHost host(store);
    // Start away from zero (as if previously dragged).
    host.host_writes(1, 5.0f);
    host.host_writes(2, -4.0f);
    host.render_block();
    REQUIRE_THAT(store.get_value(1), WithinAbs(5.0f, 1e-6f));

    // Type 0 into pitch, commit. Then 0 into formant, commit. Separate gestures.
    { state::ParameterEdit e(store); e.begin(1); e.set(1, 0.0f); e.finish(); }
    host.render_block();
    { state::ParameterEdit e(store); e.begin(2); e.set(2, 0.0f); e.finish(); }
    host.render_block();
    host.render_block();

    // Neither value was snapped back by setting the other.
    REQUIRE_THAT(store.get_value(1), WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(store.get_value(2), WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("Freeze toggle stays engaged after a render block",
          "[format][param-sync][issue-pending]") {
    state::StateStore store;
    populate(store);
    FakeHost host(store);
    host.render_block();
    REQUIRE(store.get_value(3) < 0.5f);

    { state::ParameterEdit e(store); e.begin(3); e.set(3, 1.0f); e.finish(); }
    host.render_block();
    REQUIRE(store.get_value(3) >= 0.5f); // was reverting to 0 (button "not responsive")
    host.render_block();
    REQUIRE(store.get_value(3) >= 0.5f);
}

TEST_CASE("Host automation playback still drives the store after the fix",
          "[format][param-sync][issue-pending]") {
    state::StateStore store;
    populate(store);
    FakeHost host(store);
    host.render_block();

    // Host plays back recorded automation: pitch ramps 0 → 6.
    for (float v : {1.0f, 2.5f, 4.0f, 6.0f}) {
        host.host_writes(1, v);
        host.render_block();
        REQUIRE_THAT(store.get_value(1), WithinAbs(v, 1e-6f)); // widget follows
    }
}
