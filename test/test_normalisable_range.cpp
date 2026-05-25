#include <catch2/catch_test_macros.hpp>
#include <pulp/state/parameter.hpp>

#include <cmath>

using namespace pulp::state;

TEST_CASE("NormalisableRange linear behaves like ParamRange", "[state][normalisable]") {
    NormalisableRange<float> r{0.0f, 10.0f};
    REQUIRE(r.normalize(0.0f) == 0.0f);
    REQUIRE(r.normalize(5.0f) == 0.5f);
    REQUIRE(r.normalize(10.0f) == 1.0f);
    REQUIRE(r.denormalize(0.5f) == 5.0f);
}

TEST_CASE("NormalisableRange clamps out-of-range inputs", "[state][normalisable]") {
    NormalisableRange<float> r{-1.0f, 1.0f};
    REQUIRE(r.normalize(-5.0f) == 0.0f);
    REQUIRE(r.normalize(5.0f) == 1.0f);
    REQUIRE(r.denormalize(-1.0f) == -1.0f);
    REQUIRE(r.denormalize(2.0f) == 1.0f);
}

TEST_CASE("NormalisableRange skew weights low end", "[state][normalisable]") {
    // skew < 1 should pull the curve toward `min` — so denormalize(0.5)
    // returns a value below the linear midpoint.
    NormalisableRange<float> r{0.0f, 100.0f, /*step*/ 0.0f, /*skew*/ 0.3f};
    const float mid = r.denormalize(0.5f);
    REQUIRE(mid < 50.0f);
    REQUIRE(mid > 0.0f);
    // Round-trip: normalize(denormalize(n)) == n within tolerance.
    REQUIRE(std::abs(r.normalize(mid) - 0.5f) < 1e-5f);
}

TEST_CASE("NormalisableRange with_centre places midpoint at 0.5", "[state][normalisable]") {
    // Classic frequency knob: 20 Hz min, 20000 Hz max, 1000 Hz centre.
    auto r = NormalisableRange<float>::with_centre(20.0f, 20000.0f, 1000.0f);
    const float at_half = r.denormalize(0.5f);
    REQUIRE(std::abs(at_half - 1000.0f) < 0.5f);
    // Endpoints still snap.
    REQUIRE(r.denormalize(0.0f) == 20.0f);
    REQUIRE(r.denormalize(1.0f) == 20000.0f);
}

TEST_CASE("NormalisableRange step quantization", "[state][normalisable]") {
    NormalisableRange<float> r{0.0f, 10.0f, /*step*/ 0.5f};
    REQUIRE(r.denormalize(0.5f) == 5.0f);
    REQUIRE(r.denormalize(0.05f) == 0.5f);
    // 0.07 of 10 = 0.7, snaps to 0.5.
    REQUIRE(r.denormalize(0.07f) == 0.5f);
    REQUIRE(r.snap(2.3f) == 2.5f);
    REQUIRE(r.snap(0.2f) == 0.0f);
}

TEST_CASE("NormalisableRange symmetric skew is mirrored", "[state][normalisable]") {
    NormalisableRange<float> r{-1.0f, 1.0f, /*step*/ 0.0f, /*skew*/ 0.5f, /*symmetric*/ true};
    // 0.0 in normalized space maps to -1, 1.0 maps to 1.
    REQUIRE(std::abs(r.denormalize(0.0f) - (-1.0f)) < 1e-5f);
    REQUIRE(std::abs(r.denormalize(1.0f) - 1.0f) < 1e-5f);
    // 0.5 in normalized space maps near the midpoint (0).
    REQUIRE(std::abs(r.denormalize(0.5f)) < 1e-5f);
    // Mirror property: denormalize(0.5 + d) == -denormalize(0.5 - d) within tolerance.
    const float plus = r.denormalize(0.7f);
    const float minus = r.denormalize(0.3f);
    REQUIRE(std::abs(plus + minus) < 1e-5f);
}
