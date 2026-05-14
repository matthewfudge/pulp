// pulp #59/#63/#64/#65 — WindowHost::compute_design_viewport_transform.
//
// Unit-tests the pure math behind set_design_viewport so the
// scale + letterbox math is locked down independent of any platform
// host. The mac GPU host delegates to this function on every paint,
// so a regression here would silently break proportional resize for
// every fixed-design import.
//
// Tag [issue-pulp-design-viewport] so the coverage harness can
// attribute these to the slice that introduced them.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/view/window_host.hpp>

using pulp::view::WindowHost;
using Catch::Matchers::WithinAbs;

namespace {

struct Transform { float sx, sy, tx, ty; bool ok; };

Transform xform(float ww, float wh, float dw, float dh) {
    Transform t{};
    t.ok = WindowHost::compute_design_viewport_transform(
        ww, wh, dw, dh, t.sx, t.sy, t.tx, t.ty);
    return t;
}

constexpr float kEps = 1e-4f;

} // namespace

TEST_CASE("design viewport: 1:1 match yields identity", "[view][design-viewport][issue-pulp-design-viewport]") {
    auto t = xform(1320, 860, 1320, 860);
    REQUIRE(t.ok);
    REQUIRE_THAT(t.sx, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(t.sy, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(t.tx, WithinAbs(0.0f, kEps));
    REQUIRE_THAT(t.ty, WithinAbs(0.0f, kEps));
}

TEST_CASE("design viewport: proportional shrink keeps aspect", "[view][design-viewport][issue-pulp-design-viewport]") {
    // Half-size window at the design aspect — uniform 0.5x scale,
    // no letterboxing because aspect matches.
    auto t = xform(660, 430, 1320, 860);
    REQUIRE(t.ok);
    REQUIRE_THAT(t.sx, WithinAbs(0.5f, kEps));
    REQUIRE_THAT(t.sy, WithinAbs(0.5f, kEps));
    REQUIRE(t.sx == t.sy);  // isotropic
    REQUIRE_THAT(t.tx, WithinAbs(0.0f, kEps));
    REQUIRE_THAT(t.ty, WithinAbs(0.0f, kEps));
}

TEST_CASE("design viewport: wider window letterboxes horizontally", "[view][design-viewport][issue-pulp-design-viewport]") {
    // 1600x860 window, 1320x860 design — height is the limiting axis
    // (1.0x), letterbox bars appear on left+right.
    auto t = xform(1600, 860, 1320, 860);
    REQUIRE(t.ok);
    REQUIRE_THAT(t.sx, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(t.sy, WithinAbs(1.0f, kEps));
    // (1600 - 1320) / 2 = 140 px per side
    REQUIRE_THAT(t.tx, WithinAbs(140.0f, kEps));
    REQUIRE_THAT(t.ty, WithinAbs(0.0f, kEps));
}

TEST_CASE("design viewport: taller window letterboxes vertically", "[view][design-viewport][issue-pulp-design-viewport]") {
    // 1320x1000 window — width is the limiting axis (1.0x), letterbox
    // bars appear on top+bottom.
    auto t = xform(1320, 1000, 1320, 860);
    REQUIRE(t.ok);
    REQUIRE_THAT(t.sx, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(t.sy, WithinAbs(1.0f, kEps));
    REQUIRE_THAT(t.tx, WithinAbs(0.0f, kEps));
    // (1000 - 860) / 2 = 70 px top + bottom
    REQUIRE_THAT(t.ty, WithinAbs(70.0f, kEps));
}

TEST_CASE("design viewport: input inverse round-trips a known point",
          "[view][design-viewport][issue-pulp-design-viewport]") {
    // Mouse at the centre of a wider-than-design window should map to
    // the centre of the design surface, not the centre of the window.
    const float ww = 1600.0f, wh = 860.0f, dw = 1320.0f, dh = 860.0f;
    auto t = xform(ww, wh, dw, dh);
    REQUIRE(t.ok);

    // The window-host's input inverse: rx = (wx - tx) / sx, ry = (wy - ty) / sy.
    auto inv = [&](float wx, float wy) {
        return std::pair{(wx - t.tx) / t.sx, (wy - t.ty) / t.sy};
    };

    {
        auto [rx, ry] = inv(ww * 0.5f, wh * 0.5f);
        REQUIRE_THAT(rx, WithinAbs(dw * 0.5f, kEps));
        REQUIRE_THAT(ry, WithinAbs(dh * 0.5f, kEps));
    }
    // Top-left corner of the design surface.
    {
        auto [rx, ry] = inv(t.tx, t.ty);
        REQUIRE_THAT(rx, WithinAbs(0.0f, kEps));
        REQUIRE_THAT(ry, WithinAbs(0.0f, kEps));
    }
    // Bottom-right corner of the design surface.
    {
        auto [rx, ry] = inv(t.tx + dw * t.sx, t.ty + dh * t.sy);
        REQUIRE_THAT(rx, WithinAbs(dw, kEps));
        REQUIRE_THAT(ry, WithinAbs(dh, kEps));
    }
}

TEST_CASE("design viewport: rejects degenerate inputs",
          "[view][design-viewport][issue-pulp-design-viewport]") {
    REQUIRE_FALSE(xform(0, 860, 1320, 860).ok);
    REQUIRE_FALSE(xform(1320, 0, 1320, 860).ok);
    REQUIRE_FALSE(xform(1320, 860, 0, 860).ok);
    REQUIRE_FALSE(xform(1320, 860, 1320, 0).ok);
    REQUIRE_FALSE(xform(-1, 860, 1320, 860).ok);
    REQUIRE_FALSE(xform(1320, 860, 1320, -1).ok);
}
