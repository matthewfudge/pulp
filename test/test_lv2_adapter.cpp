#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <pulp/format/lv2_adapter.hpp>
#include <pulp/format/processor.hpp>
#include <pulp/state/store.hpp>

using namespace pulp::format;
using namespace pulp::format::lv2_adapter;
using Catch::Matchers::ContainsSubstring;

// Test processor for LV2 adapter tests
class TestLv2Processor : public Processor {
public:
    PluginDescriptor descriptor() const override {
        return {
            .name = "TestLv2",
            .manufacturer = "Pulp",
            .bundle_id = "com.pulp.test-lv2",
            .version = "1.0.0",
            .category = PluginCategory::Effect,
            .input_buses = {{"Audio In", 2}},
            .output_buses = {{"Audio Out", 2}},
            .accepts_midi = false,
            .produces_midi = false,
        };
    }

    void define_parameters(state::StateStore& store) override {
        store.add_parameter({
            .id = 1,
            .name = "Gain",
            .unit = "dB",
            .range = {-60.0f, 24.0f, 0.0f, 0.1f},
        });
        store.add_parameter({
            .id = 2,
            .name = "Mix",
            .unit = "%",
            .range = {0.0f, 100.0f, 100.0f},
        });
    }

    void prepare(const PrepareContext&) override {}

    void process(
        audio::BufferView<float>&,
        const audio::BufferView<const float>&,
        midi::MidiBuffer&,
        midi::MidiBuffer&,
        const ProcessContext&) override {}
};

TEST_CASE("LV2 TTL generation produces valid plugin.ttl", "[format][lv2]") {
    TestLv2Processor proc;
    state::StateStore store;
    proc.define_parameters(store);
    auto desc = proc.descriptor();

    auto ttl = generate_plugin_ttl(desc, store, "http://pulp.audio/plugins/test-lv2");

    // Check prefixes
    REQUIRE_THAT(ttl, ContainsSubstring("@prefix lv2:"));
    REQUIRE_THAT(ttl, ContainsSubstring("@prefix doap:"));

    // Check plugin URI
    REQUIRE_THAT(ttl, ContainsSubstring("<http://pulp.audio/plugins/test-lv2>"));

    // Check plugin metadata
    REQUIRE_THAT(ttl, ContainsSubstring("doap:name \"TestLv2\""));
    REQUIRE_THAT(ttl, ContainsSubstring("doap:name \"Pulp\""));

    // Check audio ports (2 in + 2 out = 4 audio ports)
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:AudioPort"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:InputPort"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:OutputPort"));

    // Check control ports for parameters
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:ControlPort"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:name \"Gain\""));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:name \"Mix\""));

    // Check parameter ranges
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:minimum -60"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:maximum 24"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:default 0"));
}

TEST_CASE("LV2 TTL includes MIDI ports for instruments", "[format][lv2]") {
    PluginDescriptor desc;
    desc.name = "TestSynth";
    desc.manufacturer = "Pulp";
    desc.category = PluginCategory::Instrument;
    desc.input_buses = {};
    desc.output_buses = {{"Audio Out", 2}};
    desc.accepts_midi = true;
    desc.produces_midi = false;

    state::StateStore store;
    auto ttl = generate_plugin_ttl(desc, store, "http://pulp.audio/plugins/test-synth");

    // Should have InstrumentPlugin class
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:InstrumentPlugin"));

    // Should have MIDI input atom port
    REQUIRE_THAT(ttl, ContainsSubstring("atom:AtomPort"));
    REQUIRE_THAT(ttl, ContainsSubstring("midi:MidiEvent"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:symbol \"midi_in\""));
}

TEST_CASE("LV2 manifest.ttl generation", "[format][lv2]") {
    auto ttl = generate_manifest_ttl(
        "http://pulp.audio/plugins/test-lv2", "TestLv2.so");

    REQUIRE_THAT(ttl, ContainsSubstring("<http://pulp.audio/plugins/test-lv2>"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:binary <TestLv2.so>"));
    REQUIRE_THAT(ttl, ContainsSubstring("rdfs:seeAlso <TestLv2.ttl>"));
}

TEST_CASE("LV2 TTL port indices are sequential", "[format][lv2]") {
    TestLv2Processor proc;
    state::StateStore store;
    proc.define_parameters(store);
    auto desc = proc.descriptor();

    auto ttl = generate_plugin_ttl(desc, store, "http://pulp.audio/plugins/test");

    // 2 audio in + 2 audio out + 2 control = indices 0-5
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 0"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 1"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 2"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 3"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 4"));
    REQUIRE_THAT(ttl, ContainsSubstring("lv2:index 5"));
}
