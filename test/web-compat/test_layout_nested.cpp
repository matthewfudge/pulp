// Nested layout tests — validates multi-level flex/grid nesting and complex scenarios

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

TEST_CASE("Nested: row inside column", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 300, 400});
    root.flex().direction = FlexDirection::column;

    auto row = std::make_unique<View>();
    row->flex().direction = FlexDirection::row;
    row->flex().preferred_height = 50;
    row->flex().flex_grow = 0;

    auto a = make_box(100, 50);
    auto b = make_box(100, 50);
    row->add_child(std::move(a));
    row->add_child(std::move(b));

    auto* row_ptr = row.get();
    root.add_child(std::move(row));
    root.layout_children();

    REQUIRE_THAT(row_ptr->child_at(0)->bounds().x, WithinAbs(0.0f, 1.0f));
    REQUIRE_THAT(row_ptr->child_at(1)->bounds().x, WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Nested: column inside row", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 400, 300});
    root.flex().direction = FlexDirection::row;

    auto col = std::make_unique<View>();
    col->flex().direction = FlexDirection::column;
    col->flex().preferred_width = 200;

    auto a = make_box(200, 50);
    auto b = make_box(200, 50);
    col->add_child(std::move(a));
    col->add_child(std::move(b));

    auto* col_ptr = col.get();
    root.add_child(std::move(col));
    root.layout_children();

    REQUIRE_THAT(col_ptr->child_at(0)->bounds().y, WithinAbs(0.0f, 1.0f));
    REQUIRE_THAT(col_ptr->child_at(1)->bounds().y, WithinAbs(50.0f, 1.0f));
}

TEST_CASE("Nested: 3-level nesting row > col > row", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 400, 300});
    root.flex().direction = FlexDirection::row;

    auto col = std::make_unique<View>();
    col->flex().direction = FlexDirection::column;
    col->flex().preferred_width = 200;

    auto inner_row = std::make_unique<View>();
    inner_row->flex().direction = FlexDirection::row;
    inner_row->flex().preferred_height = 40;
    auto leaf1 = make_box(80, 40);
    auto leaf2 = make_box(80, 40);
    inner_row->add_child(std::move(leaf1));
    inner_row->add_child(std::move(leaf2));

    auto* inner_ptr = inner_row.get();
    col->add_child(std::move(inner_row));
    root.add_child(std::move(col));
    root.layout_children();

    REQUIRE_THAT(inner_ptr->child_at(0)->bounds().x, WithinAbs(0.0f, 1.0f));
    REQUIRE_THAT(inner_ptr->child_at(1)->bounds().x, WithinAbs(80.0f, 1.0f));
}

TEST_CASE("Nested: sidebar + main content layout", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 800, 600});
    root.flex().direction = FlexDirection::row;

    auto sidebar = make_box(200, 0);
    auto main = make_box(0, 0);
    main->flex().flex_grow = 1;

    root.add_child(std::move(sidebar));
    root.add_child(std::move(main));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width == 200.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(600.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().x, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Nested: header + content + footer layout", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 400, 600});
    root.flex().direction = FlexDirection::column;

    auto header = make_box(0, 60);
    auto content = make_box(0, 0);
    content->flex().flex_grow = 1;
    auto footer = make_box(0, 40);

    root.add_child(std::move(header));
    root.add_child(std::move(content));
    root.add_child(std::move(footer));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().height == 60.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().height, WithinAbs(500.0f, 1.0f));
    REQUIRE(root.child_at(2)->bounds().height == 40.0f);
    REQUIRE_THAT(root.child_at(2)->bounds().y, WithinAbs(560.0f, 1.0f));
}

TEST_CASE("Nested: holy grail layout (header/sidebar/main/sidebar/footer)", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 800, 600});
    root.flex().direction = FlexDirection::column;

    auto header = make_box(0, 50);
    auto middle = std::make_unique<View>();
    middle->flex().direction = FlexDirection::row;
    middle->flex().flex_grow = 1;

    auto left_bar = make_box(150, 0);
    auto main_content = make_box(0, 0);
    main_content->flex().flex_grow = 1;
    auto right_bar = make_box(150, 0);

    auto* mid_ptr = middle.get();
    middle->add_child(std::move(left_bar));
    middle->add_child(std::move(main_content));
    middle->add_child(std::move(right_bar));

    auto footer = make_box(0, 50);

    root.add_child(std::move(header));
    root.add_child(std::move(middle));
    root.add_child(std::move(footer));
    root.layout_children();

    // Header
    REQUIRE(root.child_at(0)->bounds().height == 50.0f);
    // Middle section fills remaining
    REQUIRE_THAT(root.child_at(1)->bounds().height, WithinAbs(500.0f, 1.0f));
    // Footer at bottom
    REQUIRE_THAT(root.child_at(2)->bounds().y, WithinAbs(550.0f, 1.0f));

    // Inner sidebar widths
    REQUIRE(mid_ptr->child_at(0)->bounds().width == 150.0f);
    REQUIRE_THAT(mid_ptr->child_at(1)->bounds().width, WithinAbs(500.0f, 1.0f));
    REQUIRE(mid_ptr->child_at(2)->bounds().width == 150.0f);
}

TEST_CASE("Nested: padding propagates through nesting", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 300, 300});
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 10;

    auto child = make_box(0, 50);
    child->flex().flex_grow = 0;
    auto* cp = child.get();
    root.add_child(std::move(child));
    root.layout_children();

    REQUIRE_THAT(cp->bounds().x, WithinAbs(10.0f, 1.0f));
    REQUIRE_THAT(cp->bounds().y, WithinAbs(10.0f, 1.0f));
}

TEST_CASE("Nested: gap in both levels", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 400, 300});
    root.flex().direction = FlexDirection::column;
    root.flex().gap = 10;

    auto row = std::make_unique<View>();
    row->flex().direction = FlexDirection::row;
    row->flex().preferred_height = 50;
    row->flex().gap = 20;
    row->add_child(make_box(80, 50));
    row->add_child(make_box(80, 50));
    auto* row_ptr = row.get();

    root.add_child(std::move(row));
    root.add_child(make_box(0, 50));
    root.layout_children();

    // Inner gap: children spaced by 20
    REQUIRE_THAT(row_ptr->child_at(1)->bounds().x, WithinAbs(100.0f, 1.0f));
    // Outer gap: second root child offset by row height + gap
    REQUIRE_THAT(root.child_at(1)->bounds().y, WithinAbs(60.0f, 1.0f));
}

TEST_CASE("Nested: mixed flex-grow at multiple levels", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 600, 400});
    root.flex().direction = FlexDirection::row;

    auto left = make_box(0, 0);
    left->flex().flex_grow = 1;
    auto right = make_box(0, 0);
    right->flex().flex_grow = 2;
    right->flex().direction = FlexDirection::column;

    auto top_half = make_box(0, 0);
    top_half->flex().flex_grow = 1;
    auto bot_half = make_box(0, 0);
    bot_half->flex().flex_grow = 1;
    auto* right_ptr = right.get();
    right->add_child(std::move(top_half));
    right->add_child(std::move(bot_half));

    root.add_child(std::move(left));
    root.add_child(std::move(right));
    root.layout_children();

    // Left: 1/3 of 600 = 200, Right: 2/3 = 400
    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(400.0f, 1.0f));

    // Right's children split height equally
    REQUIRE_THAT(right_ptr->child_at(0)->bounds().height, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(right_ptr->child_at(1)->bounds().height, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Nested: JS API creates nested containers", "[layout][nested]") {
    TestEnvironment env(400, 300);
    env.run(R"JS(
        createCol("outer");
        setFlex("outer", "width", 400);
        setFlex("outer", "height", 300);
        setFlex("outer", "gap", 10);

        createRow("inner", "outer");
        setFlex("inner", "height", 50);
        setFlex("inner", "gap", 5);

        createPanel("a", "inner");
        setFlex("a", "width", 80);
        setFlex("a", "height", 50);

        createPanel("b", "inner");
        setFlex("b", "width", 80);
        setFlex("b", "height", 50);
    )JS");

    auto* inner = env.widget("inner");
    REQUIRE(inner != nullptr);
    REQUIRE(inner->child_count() == 2);
}

TEST_CASE("Nested: deeply nested views don't crash", "[layout][nested]") {
    View root;
    root.set_bounds({0, 0, 400, 400});
    root.flex().direction = FlexDirection::column;

    View* current = &root;
    for (int i = 0; i < 10; ++i) {
        auto child = std::make_unique<View>();
        child->flex().direction = (i % 2 == 0) ? FlexDirection::row : FlexDirection::column;
        child->flex().flex_grow = 1;
        auto* ptr = child.get();
        current->add_child(std::move(child));
        current = ptr;
    }
    // Add a leaf
    current->add_child(make_box(50, 50));

    root.layout_children(); // Should not crash or infinite loop
    REQUIRE(root.child_count() >= 1);
}
