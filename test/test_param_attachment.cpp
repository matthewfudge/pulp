#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/view/param_attachment.hpp>

using namespace pulp::view;
using namespace pulp::state;
using Catch::Matchers::WithinAbs;

static void setup_store(StateStore& store) {
    store.add_parameter({.id = 1, .name = "Cutoff", .unit = "Hz",
        .range = {20, 20000, 1000, 0.01f}});
    store.add_parameter({.id = 2, .name = "Resonance", .unit = "",
        .range = {0, 1, 0.5f, 0.01f}});
    store.add_parameter({.id = 3, .name = "Type", .unit = "",
        .range = {0, 3, 0, 1}});
    store.add_parameter({.id = 4, .name = "Bypass", .unit = "",
        .range = {0, 1, 0, 1}});
}

TEST_CASE("attach_knob creates knob with correct label", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [knob, binding] = attach_knob(store, 1, 48.0f);
    REQUIRE(knob != nullptr);
    REQUIRE(knob->id() == "Cutoff");
}

TEST_CASE("attach_knob initial value matches param", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    store.set_value(1, 5000.0f);
    auto [knob, binding] = attach_knob(store, 1);
    float expected_norm = store.get_normalized(1);
    REQUIRE_THAT(knob->value(), WithinAbs(expected_norm, 0.01));
}

TEST_CASE("attach_knob size is set", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [knob, binding] = attach_knob(store, 1, 80.0f);
    REQUIRE_THAT(knob->flex().preferred_width, WithinAbs(80.0, 0.1));
    REQUIRE_THAT(knob->flex().preferred_height, WithinAbs(80.0, 0.1));
}

TEST_CASE("attach_fader creates fader with label", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [fader, binding] = attach_fader(store, 2);
    REQUIRE(fader != nullptr);
    REQUIRE(fader->id() == "Resonance");
}

TEST_CASE("attach_fader initial value matches param", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    store.set_value(2, 0.75f);
    auto [fader, binding] = attach_fader(store, 2);
    float expected_norm = store.get_normalized(2);
    REQUIRE_THAT(fader->value(), WithinAbs(expected_norm, 0.01));
}

TEST_CASE("attach_toggle creates toggle with label", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [toggle, binding] = attach_toggle(store, 4);
    REQUIRE(toggle != nullptr);
    REQUIRE(toggle->id() == "Bypass");
}

TEST_CASE("attach_toggle reflects boolean state", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    store.set_value(4, 1.0f);
    auto [toggle, binding] = attach_toggle(store, 4);
    REQUIRE(toggle->is_on());

    StateStore store2;
    setup_store(store2);
    store2.set_value(4, 0.0f);
    auto [toggle2, binding2] = attach_toggle(store2, 4);
    REQUIRE_FALSE(toggle2->is_on());
}

TEST_CASE("attach_combo creates combobox with items", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    store.set_value(3, 2.0f);
    auto [combo, binding] = attach_combo(store, 3, {"LP", "HP", "BP", "Notch"});
    REQUIRE(combo != nullptr);
    REQUIRE(combo->selected() == 2);
    REQUIRE(combo->selected_text() == "BP");
}

TEST_CASE("poll_bindings does not crash", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [k, b1] = attach_knob(store, 1);
    auto [f, b2] = attach_fader(store, 2);
    std::vector<Binding> bindings;
    bindings.push_back(std::move(b1));
    bindings.push_back(std::move(b2));
    poll_bindings(bindings); // should not crash
}

TEST_CASE("attach_knob on_change writes to store", "[view][attachment]") {
    StateStore store;
    setup_store(store);
    auto [knob, binding] = attach_knob(store, 1);
    // Simulate user dragging knob to 0.5 normalized
    if (knob->on_change) knob->on_change(0.5f);
    REQUIRE_THAT(store.get_normalized(1), WithinAbs(0.5, 0.01));
}
