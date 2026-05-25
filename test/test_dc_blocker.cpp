#include <catch2/catch_test_macros.hpp>
#include <pulp/signal/dc_blocker.hpp>

#include <cmath>
#include <numeric>
#include <vector>

using namespace pulp::signal;

TEST_CASE("DcBlocker removes DC offset", "[signal][dc-blocker]") {
    DcBlocker<float> blocker;

    // 4096 samples of constant +0.5 should converge toward zero output.
    std::vector<float> samples(4096, 0.5f);
    blocker.process(samples.data(), static_cast<int>(samples.size()));

    // The tail half should be well below the input DC level (>60 dB attenuation
    // is a very loose bound; the actual attenuation at the default pole is
    // far stronger).
    const float tail_sum = std::accumulate(samples.begin() + 2048, samples.end(), 0.0f);
    const float tail_mean = tail_sum / 2048.0f;
    REQUIRE(std::abs(tail_mean) < 0.0005f);
}

TEST_CASE("DcBlocker preserves audio-band signal", "[signal][dc-blocker]") {
    DcBlocker<float> blocker;
    blocker.set_pole(0.995f);

    // 1 kHz sine at 44.1 kHz should pass through with magnitude close to 1.
    constexpr int kSampleRate = 44100;
    constexpr int kBlockSize = 4096;
    constexpr float kFreq = 1000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    std::vector<float> input(kBlockSize);
    for (int i = 0; i < kBlockSize; ++i)
        input[i] = std::sin(kTwoPi * kFreq * float(i) / float(kSampleRate));

    std::vector<float> output(input);
    blocker.process(output.data(), kBlockSize);

    // Skip the first 1024 samples (filter warmup). Measure RMS of input vs output.
    auto rms = [](const float* s, int from, int to) {
        double acc = 0.0;
        for (int i = from; i < to; ++i) acc += double(s[i]) * double(s[i]);
        return std::sqrt(acc / double(to - from));
    };
    const double in_rms = rms(input.data(), 1024, kBlockSize);
    const double out_rms = rms(output.data(), 1024, kBlockSize);
    // Expect <= 0.5 dB attenuation at 1 kHz with pole=0.995.
    const double ratio = out_rms / in_rms;
    REQUIRE(ratio > 0.94);
    REQUIRE(ratio < 1.01);
}

TEST_CASE("DcBlocker reset clears state", "[signal][dc-blocker]") {
    DcBlocker<float> blocker;
    for (int i = 0; i < 100; ++i)
        (void) blocker.process(1.0f);
    blocker.reset();
    // First sample after reset of a constant input is the input itself.
    const float first = blocker.process(0.5f);
    REQUIRE(first == 0.5f);
}
