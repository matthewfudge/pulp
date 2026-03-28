// Flex column layout tests — validates CSS Flexbox column direction behavior

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

TEST_CASE("Flex Col: children laid out top-to-bottom", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 80));
    root.add_child(make_box(100, 60));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().y == 0.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(50.0f, 1.0f));
    REQUIRE_THAT(root.child_at(2)->bounds().y, WithinAbs(130.0f, 1.0f));
}

TEST_CASE("Flex Col: justify-content center", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().justify_content = FlexJustify::center;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    // 400 - 100 = 300, center → start at 150
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(150.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Flex Col: justify-content end", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().justify_content = FlexJustify::end_;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(300.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(350.0f, 1.0f));
}

TEST_CASE("Flex Col: justify-content space-between", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 300});
    root.flex().direction = FlexDirection::column;
    root.flex().justify_content = FlexJustify::space_between;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().y == 0.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(125.0f, 1.0f));
    REQUIRE_THAT(root.child_at(2)->bounds().y, WithinAbs(250.0f, 1.0f));
}

TEST_CASE("Flex Col: justify-content space-evenly", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().justify_content = FlexJustify::space_evenly;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    // 400 - 100 = 300, 3 slots → 100 each
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(100.0f, 2.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(250.0f, 2.0f));
}

TEST_CASE("Flex Col: align-items stretch fills width", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().align_items = FlexAlign::stretch;
    root.add_child(make_box(0, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Flex Col: align-items center", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().align_items = FlexAlign::center;
    root.add_child(make_box(80, 50));
    root.layout_children();

    // (200 - 80) / 2 = 60
    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(60.0f, 1.0f));
}

TEST_CASE("Flex Col: align-items end", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().align_items = FlexAlign::end;
    root.add_child(make_box(80, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(120.0f, 1.0f));
}

TEST_CASE("Flex Col: flex-grow fills remaining height", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;

    auto fixed = make_box(100, 100);
    auto grow = make_box(100, 0);
    grow->flex().flex_grow = 1;

    root.add_child(std::move(fixed));
    root.add_child(std::move(grow));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().height == 100.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().height, WithinAbs(300.0f, 1.0f));
}

TEST_CASE("Flex Col: gap adds space between children", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().gap = 10;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().y == 0.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(60.0f, 1.0f));
}

TEST_CASE("Flex Col: row_gap overrides gap for column direction", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().gap = 10;
    root.flex().row_gap = 30;
    root.add_child(make_box(100, 50));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(80.0f, 1.0f));
}

TEST_CASE("Flex Col: padding insets children", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 20;
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().x, WithinAbs(20.0f, 1.0f));
    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(20.0f, 1.0f));
}

TEST_CASE("Flex Col: margin adds space around child", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;

    auto child = make_box(100, 50);
    child->flex().margin = 15;
    root.add_child(std::move(child));
    root.add_child(make_box(100, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(15.0f, 1.0f));
    // Second child after first(15+50+15) = 80
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(80.0f, 1.0f));
}

TEST_CASE("Flex Col: flex-basis overrides preferred_height", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;

    auto child = make_box(100, 50);
    child->flex().flex_basis = 120;
    root.add_child(std::move(child));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().height, WithinAbs(120.0f, 1.0f));
}

TEST_CASE("Flex Col: single child center", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.flex().justify_content = FlexJustify::center;
    root.add_child(make_box(100, 100));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().y, WithinAbs(150.0f, 1.0f));
}

TEST_CASE("Flex Col: empty container no crash", "[layout][flex][col]") {
    View root;
    root.set_bounds({0, 0, 200, 400});
    root.flex().direction = FlexDirection::column;
    root.layout_children();
    REQUIRE(root.child_count() == 0);
}
