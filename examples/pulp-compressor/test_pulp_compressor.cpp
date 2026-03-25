#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "pulp_compressor.hpp"
#include <pulp/format/headless.hpp>
#include <cmath>

using namespace pulp;
using namespace pulp::examples;
using Catch::Matchers::WithinAbs;

static float rms(const audio::Buffer<float>& buf, int ch) {
    float sum = 0;
    for (std::size_t i = 0; i < buf.num_samples(); ++i) {
        float v = buf.channel(ch)[i];
        sum += v * v;
    }
    return std::sqrt(sum / buf.num_samples());
}

static void process_headless(format::HeadlessHost& host,
                             const audio::Buffer<float>& in,
                             audio::Buffer<float>& out) {
    const float* ptrs[2] = {in.channel(0).data(), in.channel(1).data()};
    audio::BufferView<const float> iv(ptrs, 2, in.num_samples());
    auto ov = out.view();
    host.process(ov, iv);
}

TEST_CASE("PulpCompressor descriptor has sidechain bus", "[compressor]") {
    auto proc = create_pulp_compressor();
    auto desc = proc->descriptor();
    REQUIRE(desc.name == "PulpCompressor");
    REQUIRE(desc.input_buses.size() == 2);
    REQUIRE(desc.input_buses[0].name == "Main In");
    REQUIRE(desc.input_buses[0].optional == false);
    REQUIRE(desc.input_buses[1].name == "Sidechain");
    REQUIRE(desc.input_buses[1].optional == true);
    REQUIRE(desc.output_buses.size() == 1);
}

TEST_CASE("PulpCompressor has 6 parameters", "[compressor]") {
    format::HeadlessHost host(create_pulp_compressor);
    REQUIRE(host.state().param_count() == 6);
    REQUIRE(host.state().info(kThreshold)->name == "Threshold");
    REQUIRE(host.state().info(kRatio)->name == "Ratio");
}

TEST_CASE("PulpCompressor reduces loud signals", "[compressor]") {
    format::HeadlessHost host(create_pulp_compressor);
    host.prepare(48000.0, 4096);
    host.state().set_value(kThreshold, -20.0f);
    host.state().set_value(kRatio, 4.0f);
    host.state().set_value(kAttack, 0.1f);  // Fast attack

    // Loud sine wave (0 dBFS peak)
    audio::Buffer<float> in(2, 4096), out(2, 4096);
    for (std::size_t i = 0; i < 4096; ++i) {
        float v = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
        in.channel(0)[i] = v;
        in.channel(1)[i] = v;
    }

    process_headless(host, in, out);

    float in_rms = rms(in, 0);
    float out_rms = rms(out, 0);

    // With threshold at -20dB and 4:1 ratio, output should be quieter
    REQUIRE(out_rms < in_rms * 0.8f);
}

TEST_CASE("PulpCompressor bypass passes through", "[compressor]") {
    format::HeadlessHost host(create_pulp_compressor);
    host.prepare(48000.0, 512);
    host.state().set_value(kBypass, 1.0f);

    audio::Buffer<float> in(2, 512), out(2, 512);
    for (std::size_t i = 0; i < 512; ++i) {
        in.channel(0)[i] = 0.5f;
        in.channel(1)[i] = 0.5f;
    }

    process_headless(host, in, out);
    REQUIRE_THAT(out.channel(0)[100], WithinAbs(0.5, 0.001));
}

TEST_CASE("PulpCompressor state round-trip", "[compressor]") {
    format::HeadlessHost h1(create_pulp_compressor);
    h1.state().set_value(kThreshold, -30.0f);
    h1.state().set_value(kRatio, 8.0f);
    h1.state().set_value(kMakeupGain, 6.0f);

    auto saved = h1.save_state();

    format::HeadlessHost h2(create_pulp_compressor);
    REQUIRE(h2.load_state(saved));
    REQUIRE_THAT(h2.state().get_value(kThreshold), WithinAbs(-30.0, 0.1));
    REQUIRE_THAT(h2.state().get_value(kRatio), WithinAbs(8.0, 0.1));
    REQUIRE_THAT(h2.state().get_value(kMakeupGain), WithinAbs(6.0, 0.1));
}
