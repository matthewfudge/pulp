#include <catch2/catch_test_macros.hpp>
#include <pulp/signal/denormal.hpp>

#include <cmath>
#include <limits>

using namespace pulp::signal;

TEST_CASE("snap_to_zero passes through audible values", "[signal][denormal]") {
    REQUIRE(snap_to_zero(1.0f) == 1.0f);
    REQUIRE(snap_to_zero(-1.0f) == -1.0f);
    REQUIRE(snap_to_zero(1e-10f) == 1e-10f);  // -200 dB FS, still passes
    REQUIRE(snap_to_zero(-1e-10f) == -1e-10f);
}

TEST_CASE("snap_to_zero zeroes denormals and tiny values", "[signal][denormal]") {
    // Below 1e-15f threshold — should snap to zero.
    REQUIRE(snap_to_zero(1e-20f) == 0.0f);
    REQUIRE(snap_to_zero(-1e-20f) == 0.0f);
    REQUIRE(snap_to_zero(std::numeric_limits<float>::denorm_min()) == 0.0f);
    REQUIRE(snap_to_zero(-std::numeric_limits<float>::denorm_min()) == 0.0f);
}

TEST_CASE("snap_to_zero is identity at exact zero", "[signal][denormal]") {
    REQUIRE(snap_to_zero(0.0f) == 0.0f);
    REQUIRE(snap_to_zero(-0.0f) == 0.0f);
}

TEST_CASE("snap_to_zero buffer overload", "[signal][denormal]") {
    float buf[8] = {1.0f, -1.0f, 1e-20f, -1e-20f, 0.0f, 0.5f, -0.5f,
                    std::numeric_limits<float>::denorm_min()};
    snap_to_zero(buf, 8);
    REQUIRE(buf[0] == 1.0f);
    REQUIRE(buf[1] == -1.0f);
    REQUIRE(buf[2] == 0.0f);
    REQUIRE(buf[3] == 0.0f);
    REQUIRE(buf[4] == 0.0f);
    REQUIRE(buf[5] == 0.5f);
    REQUIRE(buf[6] == -0.5f);
    REQUIRE(buf[7] == 0.0f);
}

TEST_CASE("is_denormal classifies subnormals", "[signal][denormal]") {
    REQUIRE_FALSE(is_denormal(0.0f));
    REQUIRE_FALSE(is_denormal(1.0f));
    REQUIRE_FALSE(is_denormal(1e-10f));   // tiny but normal
    REQUIRE(is_denormal(std::numeric_limits<float>::denorm_min()));
}

TEST_CASE("snap_to_zero double precision", "[signal][denormal]") {
    REQUIRE(snap_to_zero(1.0) == 1.0);
    REQUIRE(snap_to_zero(1e-20) == 0.0);
    REQUIRE(snap_to_zero(0.5) == 0.5);
}
