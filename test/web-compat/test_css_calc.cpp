// CSS calc-equivalent tests — Pulp uses JS expressions instead of CSS calc(),
// so we test that JS arithmetic + setFlex produces correct layout results.
// Also tests grid template parsing edge cases.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

// ═══════════════════════════════════════════════════════════════════════════════
// JS-based "calc" — using JS expressions for computed dimensions
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Calc: JS arithmetic for width (100 - 20)", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "width", 100 - 20);
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 80.0f);
}

TEST_CASE("Calc: JS arithmetic for height (50 * 2)", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "height", 50 * 2);
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_height == 100.0f);
}

TEST_CASE("Calc: JS Math.min(a, b)", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "width", Math.min(200, 150));
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 150.0f);
}

TEST_CASE("Calc: JS Math.max(a, b)", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "width", Math.max(200, 150));
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 200.0f);
}

TEST_CASE("Calc: JS clamp via Math.min/max", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        var val = 300;
        setFlex("box", "width", Math.max(100, Math.min(val, 250)));
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 250.0f);
}

TEST_CASE("Calc: JS clamp lower bound", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        var val = 50;
        setFlex("box", "width", Math.max(100, Math.min(val, 250)));
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 100.0f);
}

TEST_CASE("Calc: JS nested expression", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        var parent_w = 400;
        var margin = 20;
        setFlex("box", "width", parent_w - margin * 2);
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 360.0f);
}

TEST_CASE("Calc: JS percentage simulation", "[parser][calc]") {
    TestEnvironment env(400, 300);
    env.run(R"JS(
        createPanel("box");
        var pct = 0.5;
        setFlex("box", "width", 400 * pct);
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 200.0f);
}

TEST_CASE("Calc: JS division for equal spacing", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "width", 300 / 3);
    )JS");
    REQUIRE(env.widget("box")->flex().preferred_width == 100.0f);
}

TEST_CASE("Calc: JS float precision", "[parser][calc]") {
    TestEnvironment env;
    env.run(R"JS(
        createPanel("box");
        setFlex("box", "width", 100 / 3);
    )JS");
    REQUIRE_THAT(env.widget("box")->flex().preferred_width, WithinAbs(33.333f, 0.01f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Layout-level percentage behavior: flex_grow distributes remaining space
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Calc: flex_grow 1 on child fills remaining space in 400px row", "[parser][calc]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;

    auto fixed = make_box(100, 100);
    auto grow = make_box(0, 100);
    grow->flex().flex_grow = 1;

    root.add_child(std::move(fixed));
    root.add_child(std::move(grow));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width == 100.0f);
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(300.0f, 1.0f));
}

TEST_CASE("Calc: two flex_grow children split space equally", "[parser][calc]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(0, 100);
    a->flex().flex_grow = 1;
    auto b = make_box(0, 100);
    b->flex().flex_grow = 1;

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Calc: flex_grow 2:1 ratio", "[parser][calc]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(0, 100);
    a->flex().flex_grow = 2;
    auto b = make_box(0, 100);
    b->flex().flex_grow = 1;

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Calc: min_width prevents shrink below threshold", "[parser][calc]") {
    View root;
    root.set_bounds({0, 0, 200, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(150, 100);
    a->flex().min_width = 100;
    auto b = make_box(150, 100);
    b->flex().min_width = 100;

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width >= 100.0f);
    REQUIRE(root.child_at(1)->bounds().width >= 100.0f);
}

TEST_CASE("Calc: max_width caps growth", "[parser][calc]") {
    View root;
    root.set_bounds({0, 0, 400, 100});
    root.flex().direction = FlexDirection::row;

    auto a = make_box(0, 100);
    a->flex().flex_grow = 1;
    a->flex().max_width = 150;

    root.add_child(std::move(a));
    root.layout_children();

    REQUIRE(root.child_at(0)->bounds().width <= 150.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Grid fractional resolution (CSS fr unit)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Calc: grid 1fr 1fr splits width equally", "[parser][calc][grid]") {
    View root;
    root.set_bounds({0, 0, 400, 200});
    root.set_layout_mode(LayoutMode::grid);
    root.grid().template_columns = GridStyle::parse_template("1fr 1fr");

    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Calc: grid 1fr 2fr splits width 1:2", "[parser][calc][grid]") {
    View root;
    root.set_bounds({0, 0, 300, 200});
    root.set_layout_mode(LayoutMode::grid);
    root.grid().template_columns = GridStyle::parse_template("1fr 2fr");

    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(200.0f, 1.0f));
}

TEST_CASE("Calc: grid fixed + fr allocates remainder to fr", "[parser][calc][grid]") {
    View root;
    root.set_bounds({0, 0, 400, 200});
    root.set_layout_mode(LayoutMode::grid);
    root.grid().template_columns = GridStyle::parse_template("100 1fr");

    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(300.0f, 1.0f));
}

TEST_CASE("Calc: grid with gap subtracts from available space", "[parser][calc][grid]") {
    View root;
    root.set_bounds({0, 0, 410, 200});
    root.set_layout_mode(LayoutMode::grid);
    root.grid().template_columns = GridStyle::parse_template("1fr 1fr");
    root.grid().column_gap = 10;

    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.layout_children();

    // 410 - 10 gap = 400, split equally → 200 each
    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(200.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(200.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Grid template edge cases
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Calc: grid parse_template with leading/trailing spaces", "[parser][calc][grid]") {
    auto tracks = GridStyle::parse_template("  1fr  2fr  ");
    REQUIRE(tracks.size() == 2);
    REQUIRE(tracks[0].value == 1.0f);
    REQUIRE(tracks[1].value == 2.0f);
}

TEST_CASE("Calc: grid parse_template single fr", "[parser][calc][grid]") {
    auto tracks = GridStyle::parse_template("1fr");
    REQUIRE(tracks.size() == 1);
    REQUIRE(tracks[0].type == GridTrack::Type::fr);
}

TEST_CASE("Calc: grid parse_template many columns", "[parser][calc][grid]") {
    auto tracks = GridStyle::parse_template("1fr 1fr 1fr 1fr 1fr");
    REQUIRE(tracks.size() == 5);
    for (auto& t : tracks) {
        REQUIRE(t.type == GridTrack::Type::fr);
        REQUIRE(t.value == 1.0f);
    }
}

TEST_CASE("Calc: grid 3fr in 300px container", "[parser][calc][grid]") {
    View root;
    root.set_bounds({0, 0, 300, 100});
    root.set_layout_mode(LayoutMode::grid);
    root.grid().template_columns = GridStyle::parse_template("1fr 1fr 1fr");

    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.add_child(make_box(0, 50));
    root.layout_children();

    REQUIRE_THAT(root.child_at(0)->bounds().width, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(root.child_at(1)->bounds().width, WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(root.child_at(2)->bounds().width, WithinAbs(100.0f, 1.0f));
}
