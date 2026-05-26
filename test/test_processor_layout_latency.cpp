// Tests for Workstream 03 items 3.7 and 3.8 — bus-layout validation
// and the processBlock precision contract. Item 3.11 (latency / tail
// change notifications) lands in a follow-up commit and reuses this
// file.
//
// These tests exercise the Processor-side API surface only. Adapter-
// specific wiring (VST3 setBusArrangements, AU PropertyChanged, CLAP
// host_latency->changed()) is covered separately in
// test_vst3_plugin_state.cpp and test_au_plugin_state.mm; here we pin
// the contract those adapters depend on so refactors can't silently
// break the bridge.

#include <catch2/catch_test_macros.hpp>
#include <pulp/format/processor.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>

#include <type_traits>

using namespace pulp::format;

namespace {

// Minimal Processor concrete: descriptor with one stereo main bus.
class StereoEffect : public Processor {
public:
    PluginDescriptor descriptor() const override {
        PluginDescriptor d;
        d.name = "StereoEffect";
        d.input_buses  = {{"Main In",  2, false}};
        d.output_buses = {{"Main Out", 2, false}};
        return d;
    }
    void define_parameters(pulp::state::StateStore&) override {}
    void prepare(const PrepareContext&) override {}
    void process(pulp::audio::BufferView<float>&,
                 const pulp::audio::BufferView<const float>&,
                 pulp::midi::MidiBuffer&,
                 pulp::midi::MidiBuffer&,
                 const ProcessContext&) override {}
};

// Effect with a sidechain bus: main + sidechain in, main out.
class SidechainEffect : public StereoEffect {
public:
    PluginDescriptor descriptor() const override {
        PluginDescriptor d = StereoEffect::descriptor();
        d.name = "SidechainEffect";
        d.input_buses  = {{"Main In", 2, false}, {"Sidechain", 2, true}};
        return d;
    }
};

// Override that requires sidechain channels == main channels (the
// classic "linked sidechain" contract).
class LinkedSidechainEffect : public SidechainEffect {
public:
    bool is_bus_layout_supported(const BusesLayout& l) const override {
        if (l.inputs.size() != 2) return false;
        if (l.outputs.size() != 1) return false;
        // Main and sidechain must agree.
        if (l.inputs[0] != l.inputs[1]) return false;
        // Output mirrors main.
        if (l.outputs[0] != l.inputs[0]) return false;
        return true;
    }
};

} // namespace

// ── Item 3.7 — bus layout validation ──────────────────────────────────────

TEST_CASE("Processor::is_bus_layout_supported default policy accepts mono/stereo "
          "matching the descriptor's bus count",
          "[processor][bus-layout][item-3.7]") {
    StereoEffect p;

    // Matches descriptor (1 in / 1 out, both stereo).
    Processor::BusesLayout ok{{2}, {2}};
    REQUIRE(p.is_bus_layout_supported(ok));

    // Mono main is fine.
    Processor::BusesLayout mono{{1}, {1}};
    REQUIRE(p.is_bus_layout_supported(mono));

    // Stereo in, mono out is allowed by the default policy — the
    // adapter is the one that enforces matching counts via the
    // descriptor; the default validator just confirms channel counts
    // are in {1, 2}.
    Processor::BusesLayout mixed{{2}, {1}};
    REQUIRE(p.is_bus_layout_supported(mixed));
}

TEST_CASE("Processor::is_bus_layout_supported default policy rejects "
          "non-mono/stereo channel counts and bus-count mismatches",
          "[processor][bus-layout][item-3.7]") {
    StereoEffect p;

    // 6-channel (5.1) is out of scope for the default validator.
    Processor::BusesLayout surround{{6}, {6}};
    REQUIRE_FALSE(p.is_bus_layout_supported(surround));

    // Wrong input bus count (descriptor declares 1, host proposes 2).
    Processor::BusesLayout wrong_count{{2, 2}, {2}};
    REQUIRE_FALSE(p.is_bus_layout_supported(wrong_count));

    // Empty per-side means "no opinion" — must be accepted so adapters
    // that only pass the side they care about don't trip the validator.
    Processor::BusesLayout empty_in{{}, {2}};
    REQUIRE(p.is_bus_layout_supported(empty_in));
    Processor::BusesLayout empty_out{{2}, {}};
    REQUIRE(p.is_bus_layout_supported(empty_out));
}

TEST_CASE("Processor::is_bus_layout_supported override can enforce a "
          "linked-sidechain contract",
          "[processor][bus-layout][item-3.7]") {
    LinkedSidechainEffect p;

    // Stereo main + stereo sidechain + stereo out — all linked.
    Processor::BusesLayout linked_stereo{{2, 2}, {2}};
    REQUIRE(p.is_bus_layout_supported(linked_stereo));

    // Mono everywhere — also linked.
    Processor::BusesLayout linked_mono{{1, 1}, {1}};
    REQUIRE(p.is_bus_layout_supported(linked_mono));

    // Stereo main / mono sidechain — rejected, sidechain must match main.
    Processor::BusesLayout mismatched_sc{{2, 1}, {2}};
    REQUIRE_FALSE(p.is_bus_layout_supported(mismatched_sc));

    // Stereo main / stereo sidechain / mono out — rejected by the
    // mirror-output rule the plugin's override added.
    Processor::BusesLayout mismatched_out{{2, 2}, {1}};
    REQUIRE_FALSE(p.is_bus_layout_supported(mismatched_out));
}

// ── Item 3.8 — processBlock precision contract ────────────────────────────

TEST_CASE("Processor::process is declared with float-precision BufferView. "
          "All four adapters today route only float buffers; double-"
          "precision support is opt-in per future Processor overload.",
          "[processor][precision][item-3.8]") {
    // The contract under audit: the single virtual `process()` on
    // pulp::format::Processor is `BufferView<float>` only — there is
    // no `BufferView<double>` overload. Adapters wire float input ↔
    // output buffers in every format (VST3 channelBuffers32, AU
    // float32 render block, CLAP audio_buffer_t.data32, AAX float
    // pages). A regression that adds a second virtual or that
    // changes the element type to a wider scalar would silently
    // un-implement every adapter; this static_assert pins the
    // contract until item 3.8's follow-up adds an explicit double
    // overload.
    using ProcessSig = void (Processor::*)(
        pulp::audio::BufferView<float>&,
        const pulp::audio::BufferView<const float>&,
        pulp::midi::MidiBuffer&,
        pulp::midi::MidiBuffer&,
        const ProcessContext&);
    static_assert(std::is_assignable_v<ProcessSig&, decltype(&Processor::process)>,
                  "Processor::process() must remain a float-precision "
                  "virtual until item 3.8's follow-up adds a double "
                  "overload deliberately; do not silently change the "
                  "element type.");

    // Runtime smoke: invoke process() with a float buffer to prove
    // the adapter-facing path is callable (not a compile-only check).
    StereoEffect p;
    float buf_in[2][8] = {};
    float buf_out[2][8] = {};
    const float* in_ptrs[2]  = {buf_in[0],  buf_in[1]};
    float*       out_ptrs[2] = {buf_out[0], buf_out[1]};
    pulp::audio::BufferView<const float> in_view(in_ptrs, 2, 8);
    pulp::audio::BufferView<float>       out_view(out_ptrs, 2, 8);
    pulp::midi::MidiBuffer mi, mo;
    ProcessContext ctx; ctx.sample_rate = 48000.0; ctx.num_samples = 8;
    p.process(out_view, in_view, mi, mo, ctx);
    SUCCEED("process() ran with float buffers");
}

// Item 3.11 — latency / tail change notifications land in a follow-up
// commit and append their cases below.
