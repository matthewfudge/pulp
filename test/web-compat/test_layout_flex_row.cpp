// Flex row layout tests — validates CSS Flexbox Level 1 row direction behavior

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

// ═══════════════════════════════════════════════════════════════════════════════
// Basic row layout
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: children laid out left-to-right", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().x == 0.0f);
    REQUIRE(root.child_at(1)->bounds().x == 50.0f);
    REQUIRE(root.child_at(2)->bounds().x == 100.0f);
}

TEST_CASE("Flex Row: children get correct widths", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.add_child(make_box(80, 40));
    root.add_child(make_box(120, 40));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width == 80.0f);
    REQUIRE(root.child_at(1)->bounds().width == 120.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// justify-content (6 values)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: justify-content start", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::start;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().x == 0.0f);
    REQUIRE(root.child_at(1)->bounds().x == 100.0f);
}

TEST_CASE("Flex Row: justify-content center", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::center;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    // 400 - 200 = 200 free, center → start at 100
    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Flex Row: justify-content end", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::end_;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(300.0f, 1.0f));
}

TEST_CASE("Flex Row: justify-content space-between", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::space_between;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    // 400 - 150 = 250, 2 gaps → 125 each
    REQUIRE(root.child_at(0)->bounds().x == 0.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(175.0f, 1.0f));
    REQUIRE_THAT(root.child_at(2)->bounds().x, WithinAbs(350.0f, 1.0f));
}

TEST_CASE("Flex Row: justify-content space-around", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::space_around;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    // 400 - 100 = 300 free, 2 items → 75px margin each side
    // First item at 75, second at 75+50+150 = 275
    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(75.0f, 2.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(275.0f, 2.0f));
}

TEST_CASE("Flex Row: justify-content space-evenly", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::space_evenly;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    // 400 - 100 = 300 free, 3 slots → 100 each
    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(100.0f, 2.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(250.0f, 2.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// align-items (4 values)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: align-items stretch fills cross-axis", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().align_items = FlexAlign::stretch;
    root.add_child(make_box(50, 0));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().height, WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Flex Row: align-items start", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().align_items = FlexAlign::start;
    root.add_child(make_box(50, 30));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().y == 0.0f);
    REQUIRE(root.child_at(0)->bounds().height == 30.0f);
}

TEST_CASE("Flex Row: align-items center", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().align_items = FlexAlign::center;
    root.add_child(make_box(50, 30));
    root.layout_children();

    // (100 - 30) / 2 = 35
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(35.0f, 1.0f));
}

TEST_CASE("Flex Row: align-items end", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().align_items = FlexAlign::end;
    root.add_child(make_box(50, 30));
    root.layout_children();

    // 100 - 30 = 70
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(70.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// flex-grow
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: flex-grow 1 fills remaining space", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto fixed = make_box(100, 50);
    auto grow = make_box(0, 50);
    grow->flex().flex_grow = 1;

    root.add_child(std::move(fixed));
    root.add_child(std::move(grow));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width == 100.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Flex Row: flex-grow 2:1 ratio", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(0, 50);
    a->flex().flex_grow = 2;
    auto b = make_box(0, 50);
    b->flex().flex_grow = 1;

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Flex Row: flex-grow with fixed + growing children", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 500, 100});
    root.flex().direction = FlexDirection::row;

    auto fixed1 = make_box(100, 50);
    auto grow1 = make_box(0, 50);
    grow1->flex().flex_grow = 1;
    auto fixed2 = make_box(100, 50);
    auto grow2 = make_box(0, 50);
    grow2->flex().flex_grow = 1;

    root.add_child(std::move(fixed1));
    root.add_child(std::move(grow1));
    root.add_child(std::move(fixed2));
    root.add_child(std::move(grow2));
    root.layout_children();

    // 500 - 200 fixed = 300, split 1:1 → 150 each
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(150.0f, 1.0f));
    REQUIRE_THAT(root.child_at(3)->bounds().width, WithinAbs(150.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// flex-shrink
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: flex-shrink prevents overflow", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 200, 100});
    root.flex().direction = FlexDirection::row;

    root.add_child(make_box(150, 50));
    root.add_child(make_box(150, 50));
    root.layout_children();

    // Both should shrink to fit 200px
    float total = root.child_at(0)->bounds().width + root.child_at(1)->bounds().width;
    REQUIRE_THAT(total, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Flex Row: flex-shrink 0 prevents item from shrinking", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 200, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(150, 50);
    a->flex().flex_shrink = 0;
    auto b = make_box(150, 50);

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    // First child should keep 150 (shrink=0), second absorbs all shrinkage
    REQUIRE(root.child_at(0)->bounds().width == 150.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Gap
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: gap adds space between children", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().gap = 10;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().x == 0.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(60.0f, 1.0f));
    REQUIRE_THAT(root.child_at(2)->bounds().x, WithinAbs(120.0f, 1.0f));
}

TEST_CASE("Flex Row: column_gap overrides gap in row direction", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().gap = 10;
    root.flex().column_gap = 20;
    root.add_child(make_box(50, 50));
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(70.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Padding
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: padding insets children", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().padding = 10;
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(10.0f, 1.0f));
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(10.0f, 1.0f));
}

TEST_CASE("Flex Row: per-side padding", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().padding_left = 20;
    root.flex().padding_top = 15;
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(20.0f, 1.0f));
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(15.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Margin
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: margin adds space around child", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto child = make_box(50, 50);
    child->flex().margin = 10;
    root.add_child(std::move(child));
    root.add_child(make_box(50, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(10.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(70.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// min/max constraints
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: min_width prevents shrink below threshold", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 200, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(150, 50);
    a->flex().min_width = 120;
    auto b = make_box(150, 50);

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width >= 120.0f);
}

TEST_CASE("Flex Row: max_width caps growth", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(0, 50);
    a->flex().flex_grow = 1;
    a->flex().max_width = 150;

    root.add_child(std::move(a));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width <= 150.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// flex-basis
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: flex-basis overrides preferred_width", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto child = make_box(100, 50);
    child->flex().flex_basis = 150;

    root.add_child(std::move(child));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(150.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Order
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: order changes layout sequence", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(50, 50);
    a->flex().order = 2;
    auto b = make_box(50, 50);
    b->flex().order = 1;

    auto* a_ptr = a.get();
    auto* b_ptr = b.get();
    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    // b (order=1) should be before a (order=2) in layout
    REQUIRE(b_ptr->bounds().x < a_ptr->bounds().x);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Single child edge case
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Flex Row: single child justify center", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.flex().justify_content = FlexJustify::center;
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Flex Row: empty container no crash", "[layout][flex][row]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;
    root.layout_children(); // Should not crash
    REQUIRE(root.child_count() == 0);
}
