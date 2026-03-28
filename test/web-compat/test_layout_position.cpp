// Position layout tests — validates CSS positioned layout (absolute, relative, z-index)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

// ═══════════════════════════════════════════════════════════════════════════════
// Position modes
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: default is static", "[layout][position]") {
    View v;
    REQUIRE(v.position() == View::Position::static_);
}

TEST_CASE("Position: set absolute", "[layout][position]") {
    View v;
    v.set_position(View::Position::absolute);
    REQUIRE(v.position() == View::Position::absolute);
}

TEST_CASE("Position: set relative", "[layout][position]") {
    View v;
    v.set_position(View::Position::relative);
    REQUIRE(v.position() == View::Position::relative);
}

TEST_CASE("Position: set fixed", "[layout][position]") {
    View v;
    v.set_position(View::Position::fixed);
    REQUIRE(v.position() == View::Position::fixed);
}

TEST_CASE("Position: set sticky", "[layout][position]") {
    View v;
    v.set_position(View::Position::sticky);
    REQUIRE(v.position() == View::Position::sticky);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Absolute positioning with TRBL offsets
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: absolute with top/left compiles and sets", "[layout][position]") {
    View v;
    v.set_position(View::Position::absolute);
    v.set_top(10.0f);
    v.set_left(20.0f);
    // Setters exist and don't crash; no public getters for the values,
    // so we verify the position mode was set correctly.
    REQUIRE(v.position() == View::Position::absolute);
}

TEST_CASE("Position: absolute with right/bottom compiles and sets", "[layout][position]") {
    View v;
    v.set_position(View::Position::absolute);
    v.set_right(15.0f);
    v.set_bottom(25.0f);
    REQUIRE(v.position() == View::Position::absolute);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Z-index
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: z-index default is 0", "[layout][position]") {
    View v;
    REQUIRE(v.z_index() == 0);
}

TEST_CASE("Position: set z-index", "[layout][position]") {
    View v;
    v.set_z_index(10);
    REQUIRE(v.z_index() == 10);
}

TEST_CASE("Position: negative z-index", "[layout][position]") {
    View v;
    v.set_z_index(-5);
    REQUIRE(v.z_index() == -5);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Relative offset
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: relative with translate offset", "[layout][position]") {
    View v;
    v.set_position(View::Position::relative);
    v.set_translate(5.0f, 10.0f);
    REQUIRE(v.translate_x() == 5.0f);
    REQUIRE(v.translate_y() == 10.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Overflow
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: overflow hidden", "[layout][position]") {
    View v;
    v.set_overflow(View::Overflow::hidden);
    REQUIRE(v.overflow() == View::Overflow::hidden);
}

TEST_CASE("Position: overflow visible", "[layout][position]") {
    View v;
    v.set_overflow(View::Overflow::visible);
    REQUIRE(v.overflow() == View::Overflow::visible);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Hit test with positioned children
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: hit_test finds child at position", "[layout][position]") {
    View root;
    root.set_bounds({0, 0, 400, 400});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    auto* cp = child.get();
    root.add_child(std::move(child));

    REQUIRE(root.hit_test({75, 75}) == cp);
    REQUIRE(root.hit_test({200, 200}) == &root);
}

TEST_CASE("Position: hit_test returns topmost child at overlapping position", "[layout][position]") {
    View root;
    root.set_bounds({0, 0, 400, 400});

    auto a = std::make_unique<View>();
    a->set_bounds({50, 50, 100, 100});
    auto b = std::make_unique<View>();
    b->set_bounds({80, 80, 100, 100}); // overlaps with a
    auto* bp = b.get();

    root.add_child(std::move(a));
    root.add_child(std::move(b)); // b on top (later child)

    // At overlap point (90, 90), b should win (painted later)
    REQUIRE(root.hit_test({90, 90}) == bp);
}

TEST_CASE("Position: hit_test misses invisible child", "[layout][position]") {
    View root;
    root.set_bounds({0, 0, 400, 400});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    child->set_visible(false);
    root.add_child(std::move(child));

    // Invisible child should not be hit
    REQUIRE(root.hit_test({75, 75}) == &root);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Bounds and geometry
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Position: Rect contains point", "[layout][geometry]") {
    Rect r{10, 20, 100, 50};
    REQUIRE(r.contains({50, 40}));
    REQUIRE_FALSE(r.contains({5, 40}));
    REQUIRE_FALSE(r.contains({50, 5}));
}

TEST_CASE("Position: Rect right and bottom", "[layout][geometry]") {
    Rect r{10, 20, 100, 50};
    REQUIRE(r.right() == 110.0f);
    REQUIRE(r.bottom() == 70.0f);
}

TEST_CASE("Position: Rect center", "[layout][geometry]") {
    Rect r{0, 0, 100, 50};
    auto c = r.center();
    REQUIRE(c.x == 50.0f);
    REQUIRE(c.y == 25.0f);
}

TEST_CASE("Position: Rect inset", "[layout][geometry]") {
    Rect r{10, 10, 100, 80};
    auto inset = r.inset(5);
    REQUIRE(inset.x == 15.0f);
    REQUIRE(inset.y == 15.0f);
    REQUIRE(inset.width == 90.0f);
    REQUIRE(inset.height == 70.0f);
}

TEST_CASE("Position: Rect is_empty", "[layout][geometry]") {
    Rect empty{0, 0, 0, 0};
    REQUIRE(empty.is_empty());
    Rect valid{0, 0, 100, 50};
    REQUIRE_FALSE(valid.is_empty());
}

TEST_CASE("Position: Point arithmetic", "[layout][geometry]") {
    Point a{10, 20};
    Point b{5, 15};
    auto sum = a + b;
    REQUIRE(sum.x == 15.0f);
    REQUIRE(sum.y == 35.0f);
    auto diff = a - b;
    REQUIRE(diff.x == 5.0f);
    REQUIRE(diff.y == 5.0f);
}
