#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/audio/sample_converter.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

using namespace pulp::audio;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ────────────────────────────────────────────────────────────────────────
// macOS plan item 1.5 — SampleConverter
//
// Convert PCM bytes ↔ planar float across:
//   - 6 formats (Int8/16/24/32, Float32/64)
//   - 3 endianness (Little/Big/Native — Native resolves to host)
//   - 2 layouts (Interleaved/Planar)
//
// Tests pin the full matrix: every (format × endian × layout)
// round-trips without bit loss for integer formats, and float
// formats round-trip exactly.
// ────────────────────────────────────────────────────────────────────────

namespace {

// Each test fixture allocates 4 frames × 2 channels of float, encodes
// to bytes via converter, decodes back, and asserts equality.
void round_trip_test(SampleFormat fmt, Endianness e, Layout layout, float epsilon) {
    SampleConverter c(fmt, e, layout);
    constexpr std::size_t frames = 4;
    constexpr std::size_t channels = 2;
    std::vector<float> ch0_in = {-1.0f, -0.5f, 0.0f, 0.5f};
    std::vector<float> ch1_in = {0.999f, 0.25f, -0.25f, -0.75f};
    const float* in_chs[2] = {ch0_in.data(), ch1_in.data()};

    std::vector<uint8_t> bytes(c.bytes_for(frames, channels), 0);
    c.from_float(in_chs, bytes.data(), frames, channels);

    std::vector<float> ch0_out(frames, 99.0f);
    std::vector<float> ch1_out(frames, 99.0f);
    float* out_chs[2] = {ch0_out.data(), ch1_out.data()};
    c.to_float(bytes.data(), out_chs, frames, channels);

    for (std::size_t f = 0; f < frames; ++f) {
        REQUIRE_THAT(ch0_out[f], WithinAbs(ch0_in[f], epsilon));
        REQUIRE_THAT(ch1_out[f], WithinAbs(ch1_in[f], epsilon));
    }
}

} // namespace

TEST_CASE("SampleConverter accessors match construction", "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Big, Layout::Planar);
    REQUIRE(c.format() == SampleFormat::Int16);
    REQUIRE(c.endian() == Endianness::Big);
    REQUIRE(c.layout() == Layout::Planar);
    REQUIRE(c.bytes_per_sample() == 2);
    REQUIRE(c.bytes_for(/*frames=*/100, /*channels=*/4) == 800);
}

TEST_CASE("bytes_per_sample matches format size", "[audio][sample-converter]") {
    REQUIRE(bytes_per_sample(SampleFormat::Int8) == 1);
    REQUIRE(bytes_per_sample(SampleFormat::Int16) == 2);
    REQUIRE(bytes_per_sample(SampleFormat::Int24) == 3);
    REQUIRE(bytes_per_sample(SampleFormat::Int32) == 4);
    REQUIRE(bytes_per_sample(SampleFormat::Float32) == 4);
    REQUIRE(bytes_per_sample(SampleFormat::Float64) == 8);
}

TEST_CASE("is_float identifies floating-point formats", "[audio][sample-converter]") {
    REQUIRE(is_float(SampleFormat::Float32));
    REQUIRE(is_float(SampleFormat::Float64));
    REQUIRE_FALSE(is_float(SampleFormat::Int8));
    REQUIRE_FALSE(is_float(SampleFormat::Int16));
    REQUIRE_FALSE(is_float(SampleFormat::Int24));
    REQUIRE_FALSE(is_float(SampleFormat::Int32));
}

// Int round-trip epsilons are 2 LSB because Pulp uses asymmetric
// encode (scale by 2^(N-1) - 1, e.g. 32767) and decode (scale by
// 2^(N-1), e.g. 32768). The asymmetry buys clean -1.0 representation
// and clamped +1.0 → max-int, at the cost of ~1 LSB round-trip drift
// at extreme positive samples.
TEST_CASE("Int16 LE round-trips through Interleaved layout", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Int16, Endianness::Little, Layout::Interleaved,
                     /*eps=*/2.0f / 32768.0f);
}

TEST_CASE("Int16 BE round-trips through Interleaved layout", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Int16, Endianness::Big, Layout::Interleaved,
                     /*eps=*/2.0f / 32768.0f);
}

TEST_CASE("Int16 Native round-trips through Interleaved layout", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Int16, Endianness::Native, Layout::Interleaved,
                     /*eps=*/2.0f / 32768.0f);
}

TEST_CASE("Int16 LE round-trips through Planar layout", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Int16, Endianness::Little, Layout::Planar,
                     /*eps=*/2.0f / 32768.0f);
}

TEST_CASE("Int8 round-trips through all layouts", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Int8, Endianness::Little, Layout::Interleaved, 2.0f / 128.0f);
    round_trip_test(SampleFormat::Int8, Endianness::Little, Layout::Planar, 2.0f / 128.0f);
}

TEST_CASE("Int24 round-trips through all layouts + endianness", "[audio][sample-converter]") {
    constexpr float eps = 2.0f / (1 << 23);
    round_trip_test(SampleFormat::Int24, Endianness::Little, Layout::Interleaved, eps);
    round_trip_test(SampleFormat::Int24, Endianness::Big, Layout::Interleaved, eps);
    round_trip_test(SampleFormat::Int24, Endianness::Native, Layout::Interleaved, eps);
    round_trip_test(SampleFormat::Int24, Endianness::Little, Layout::Planar, eps);
    round_trip_test(SampleFormat::Int24, Endianness::Big, Layout::Planar, eps);
}

TEST_CASE("Int32 round-trips through all layouts + endianness", "[audio][sample-converter]") {
    constexpr float eps = 1.0f / 8388608.0f;  // float precision, not int32 precision
    round_trip_test(SampleFormat::Int32, Endianness::Little, Layout::Interleaved, eps);
    round_trip_test(SampleFormat::Int32, Endianness::Big, Layout::Interleaved, eps);
    round_trip_test(SampleFormat::Int32, Endianness::Little, Layout::Planar, eps);
}

TEST_CASE("Float32 round-trips exactly", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Float32, Endianness::Little, Layout::Interleaved, 0.0f);
    round_trip_test(SampleFormat::Float32, Endianness::Big, Layout::Interleaved, 0.0f);
    round_trip_test(SampleFormat::Float32, Endianness::Little, Layout::Planar, 0.0f);
    round_trip_test(SampleFormat::Float32, Endianness::Big, Layout::Planar, 0.0f);
}

TEST_CASE("Float64 round-trips within float precision", "[audio][sample-converter]") {
    round_trip_test(SampleFormat::Float64, Endianness::Little, Layout::Interleaved, 1e-6f);
    round_trip_test(SampleFormat::Float64, Endianness::Big, Layout::Interleaved, 1e-6f);
    round_trip_test(SampleFormat::Float64, Endianness::Little, Layout::Planar, 1e-6f);
}

TEST_CASE("Int16 LE byte layout is correct (Interleaved 2 channels)",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Little, Layout::Interleaved);
    // ch0 = 0x1234, ch1 = -0x5678 across 1 frame.
    std::vector<float> ch0 = {0x1234 / 32768.0f};
    std::vector<float> ch1 = {-(0x5678 / 32768.0f)};
    const float* in[2] = {ch0.data(), ch1.data()};
    std::vector<uint8_t> bytes(4, 0);
    c.from_float(in, bytes.data(), 1, 2);
    // Little-endian Int16: low byte first.
    REQUIRE(bytes[0] == 0x34); // ch0 low byte
    REQUIRE(bytes[1] == 0x12); // ch0 high byte
    // -0x5678 == 0xA988 in two's complement int16. Allow ±1 LSB for the
    // float→int rounding.
    const int16_t got = static_cast<int16_t>(bytes[2] | (uint16_t(bytes[3]) << 8));
    REQUIRE(std::abs(static_cast<int>(got) - (-0x5678)) <= 1);
}

TEST_CASE("Int16 BE byte layout is correct (high byte first)",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Big, Layout::Interleaved);
    std::vector<float> ch0 = {0x1234 / 32768.0f};
    const float* in[1] = {ch0.data()};
    std::vector<uint8_t> bytes(2, 0);
    c.from_float(in, bytes.data(), 1, 1);
    REQUIRE(bytes[0] == 0x12); // BE: high byte first
    REQUIRE(bytes[1] == 0x34);
}

TEST_CASE("Int24 LE byte layout — 3 bytes, low-byte first",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int24, Endianness::Little, Layout::Interleaved);
    std::vector<float> ch0 = {0x123456 / 8388608.0f};
    const float* in[1] = {ch0.data()};
    std::vector<uint8_t> bytes(3, 0);
    c.from_float(in, bytes.data(), 1, 1);
    REQUIRE(bytes[0] == 0x56);
    REQUIRE(bytes[1] == 0x34);
    REQUIRE(bytes[2] == 0x12);
}

TEST_CASE("Int24 sign extension preserves negative values",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int24, Endianness::Little, Layout::Interleaved);
    // -1.0f → -8388607 (or -8388608, format-dependent clipping)
    std::vector<float> in_data = {-1.0f};
    const float* in[1] = {in_data.data()};
    std::vector<uint8_t> bytes(3, 0);
    c.from_float(in, bytes.data(), 1, 1);
    std::vector<float> out(1, 99.0f);
    float* outp[1] = {out.data()};
    c.to_float(bytes.data(), outp, 1, 1);
    REQUIRE_THAT(out[0], WithinAbs(-1.0f, 1.0f / 8388608.0f));
}

TEST_CASE("Planar layout encodes channels in separate blocks",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Little, Layout::Planar);
    // 3 frames × 2 channels.
    std::vector<float> ch0 = {1.0f / 32768.0f, 2.0f / 32768.0f, 3.0f / 32768.0f};
    std::vector<float> ch1 = {10.0f / 32768.0f, 20.0f / 32768.0f, 30.0f / 32768.0f};
    const float* in[2] = {ch0.data(), ch1.data()};
    std::vector<uint8_t> bytes(12, 0);
    c.from_float(in, bytes.data(), 3, 2);

    // Read channel 0 block (bytes 0..5) as 3 Int16 LE values: 1, 2, 3.
    auto read_le16 = [&](std::size_t offset) {
        return static_cast<int16_t>(
            bytes[offset] | (uint16_t(bytes[offset + 1]) << 8));
    };
    REQUIRE(read_le16(0) == 1);
    REQUIRE(read_le16(2) == 2);
    REQUIRE(read_le16(4) == 3);
    // Channel 1 block (bytes 6..11): 10, 20, 30.
    REQUIRE(read_le16(6) == 10);
    REQUIRE(read_le16(8) == 20);
    REQUIRE(read_le16(10) == 30);
}

TEST_CASE("Float32 from_float clips at ±1.0 for integer destinations",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Native, Layout::Interleaved);
    std::vector<float> in_data = {2.0f, -2.0f, 0.5f};
    const float* in[1] = {in_data.data()};
    std::vector<uint8_t> bytes(6, 0);
    c.from_float(in, bytes.data(), 3, 1);
    std::vector<float> out(3, 99.0f);
    float* outp[1] = {out.data()};
    c.to_float(bytes.data(), outp, 3, 1);
    REQUIRE_THAT(out[0], WithinAbs(1.0f, 1.0f / 32768.0f));   // clipped from +2
    REQUIRE_THAT(out[1], WithinAbs(-1.0f, 1.0f / 32768.0f));  // clipped from -2
    REQUIRE_THAT(out[2], WithinAbs(0.5f, 1.0f / 32768.0f));   // in range
}

TEST_CASE("Non-finite input float becomes silence when encoding to int",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Native, Layout::Interleaved);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    std::vector<float> in_data = {nan, inf, -inf};
    const float* in[1] = {in_data.data()};
    std::vector<uint8_t> bytes(6, 0);
    c.from_float(in, bytes.data(), 3, 1);
    std::vector<float> out(3, 99.0f);
    float* outp[1] = {out.data()};
    c.to_float(bytes.data(), outp, 3, 1);
    REQUIRE_THAT(out[0], WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(out[1], WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(out[2], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("to_float_scaled applies gain during conversion",
          "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Native, Layout::Interleaved);
    std::vector<float> in_data = {0.5f};
    const float* in[1] = {in_data.data()};
    std::vector<uint8_t> bytes(2, 0);
    c.from_float(in, bytes.data(), 1, 1);

    std::vector<float> out(1, 99.0f);
    float* outp[1] = {out.data()};
    c.to_float_scaled(bytes.data(), outp, 1, 1, /*gain=*/2.0f);
    REQUIRE_THAT(out[0], WithinAbs(1.0f, 1.0f / 32768.0f));
}

TEST_CASE("SampleConverter null inputs are no-op", "[audio][sample-converter]") {
    SampleConverter c(SampleFormat::Int16, Endianness::Native, Layout::Interleaved);
    float dummy = 99.0f;
    float* outp[1] = {&dummy};
    REQUIRE_NOTHROW(c.to_float(nullptr, outp, 1, 1));
    REQUIRE_NOTHROW(c.to_float(&dummy, nullptr, 1, 1));
    REQUIRE_NOTHROW(c.from_float(nullptr, &dummy, 1, 1));
    REQUIRE_NOTHROW(c.from_float(reinterpret_cast<const float* const*>(&dummy),
                                   nullptr, 1, 1));
    // Zero counts are also no-op.
    std::vector<uint8_t> bytes(8, 0xAA);
    REQUIRE_NOTHROW(c.to_float(bytes.data(), outp, 0, 1));
    REQUIRE_NOTHROW(c.to_float(bytes.data(), outp, 1, 0));
}

TEST_CASE("All 6 formats × 3 endians × 2 layouts round-trip",
          "[audio][sample-converter][matrix]") {
    constexpr SampleFormat formats[] = {
        SampleFormat::Int8, SampleFormat::Int16, SampleFormat::Int24,
        SampleFormat::Int32, SampleFormat::Float32, SampleFormat::Float64,
    };
    constexpr Endianness endians[] = {
        Endianness::Little, Endianness::Big, Endianness::Native,
    };
    constexpr Layout layouts[] = {
        Layout::Interleaved, Layout::Planar,
    };
    int cases = 0;
    for (auto fmt : formats) {
        // Pick a per-format tolerance: int formats see ~2 LSB
        // round-trip drift at extreme positives (asymmetric encode/
        // decode scaling — see per-format tests above for rationale).
        // Float formats are exact (Float32) or float-precision (Float64).
        const float eps = is_float(fmt) ? (fmt == SampleFormat::Float64 ? 1e-6f : 0.0f)
                                         : 2.0f / static_cast<float>(
                                               1u << (static_cast<int>(
                                                   bytes_per_sample(fmt)) * 8 - 1));
        for (auto e : endians) {
            for (auto l : layouts) {
                round_trip_test(fmt, e, l, eps);
                ++cases;
            }
        }
    }
    REQUIRE(cases == 36);
}
