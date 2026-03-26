#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "pulp_sampler.hpp"
#include <cmath>
#include <vector>

using namespace pulp;
using namespace pulp::examples;
using Catch::Matchers::WithinAbs;

// Generate a 1-second sine wave at 440 Hz
static std::vector<float> make_sine(float freq = 440.0f, float sr = 44100.0f, int samples = 44100) {
    std::vector<float> data(static_cast<size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        data[static_cast<size_t>(i)] = std::sin(2.0f * 3.14159f * freq * static_cast<float>(i) / sr);
    }
    return data;
}

struct SamplerFixture {
    state::StateStore store;
    std::unique_ptr<PulpSamplerProcessor> proc;

    SamplerFixture() {
        proc = std::make_unique<PulpSamplerProcessor>();
        proc->set_state_store(&store);
        proc->define_parameters(store);

        auto sample = make_sine();
        proc->load_sample(sample.data(), static_cast<int>(sample.size()), 44100.0f);

        format::PrepareContext ctx;
        ctx.sample_rate = 44100;
        ctx.max_buffer_size = 512;
        ctx.input_channels = 0;
        ctx.output_channels = 2;
        proc->prepare(ctx);
    }
};

TEST_CASE("PulpSampler descriptor", "[sampler]") {
    PulpSamplerProcessor proc;
    auto d = proc.descriptor();
    REQUIRE(d.name == "PulpSampler");
    REQUIRE(d.category == format::PluginCategory::Instrument);
    REQUIRE(d.accepts_midi);
    REQUIRE(d.input_buses.empty());
    REQUIRE(d.output_buses.size() == 1);
}

TEST_CASE("PulpSampler has 7 parameters", "[sampler]") {
    SamplerFixture f;
    REQUIRE(f.store.param_count() == 7);
}

TEST_CASE("PulpSampler loads sample", "[sampler]") {
    SamplerFixture f;
    REQUIRE(f.proc->has_sample());
    REQUIRE(f.proc->sample_length() == 44100);
}

TEST_CASE("PulpSampler silence without MIDI", "[sampler]") {
    SamplerFixture f;

    std::vector<float> out_l(512, 0), out_r(512, 0);
    float* out_ptrs[] = {out_l.data(), out_r.data()};
    audio::BufferView<float> out(out_ptrs, 2, 512);

    const float* in_ptrs[] = {nullptr, nullptr};
    audio::BufferView<const float> in(in_ptrs, 0, 512);

    midi::MidiBuffer midi_in, midi_out;
    format::ProcessContext ctx{44100, 512};

    f.proc->process(out, in, midi_in, midi_out, ctx);

    // No MIDI input → silence
    float sum = 0;
    for (int i = 0; i < 512; ++i) sum += std::abs(out_l[static_cast<size_t>(i)]);
    REQUIRE_THAT(sum, WithinAbs(0.0, 0.001));
}

TEST_CASE("PulpSampler produces audio on note-on", "[sampler]") {
    SamplerFixture f;

    std::vector<float> out_l(512, 0), out_r(512, 0);
    float* out_ptrs[] = {out_l.data(), out_r.data()};
    audio::BufferView<float> out(out_ptrs, 2, 512);

    const float* in_ptrs[] = {nullptr, nullptr};
    audio::BufferView<const float> in(in_ptrs, 0, 512);

    midi::MidiBuffer midi_in, midi_out;
    midi_in.add(midi::MidiEvent::note_on(0, 60, 100)); // Middle C
    format::ProcessContext ctx{44100, 512};

    f.proc->process(out, in, midi_in, midi_out, ctx);

    // Should produce non-zero output
    float peak = 0;
    for (int i = 0; i < 512; ++i) {
        peak = std::max(peak, std::abs(out_l[static_cast<size_t>(i)]));
    }
    REQUIRE(peak > 0.01f);
}

TEST_CASE("PulpSampler state round-trip", "[sampler]") {
    SamplerFixture f;

    f.store.set_value(kSamplerGain, -12.0f);
    f.store.set_value(kSamplerAttack, 50.0f);

    auto saved = f.store.serialize();
    REQUIRE_FALSE(saved.empty());

    f.store.reset_all_to_defaults();
    REQUIRE(f.store.deserialize(saved));
    REQUIRE(std::abs(f.store.get_value(kSamplerGain) - (-12.0f)) < 0.01f);
}
