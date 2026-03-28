// Click event tests — validates click dispatch, target, and coordinates

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;

TEST_CASE("Click: simulate_click fires on_click", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    bool clicked = false;
    child->on_click = [&]() { clicked = true; };
    root.add_child(std::move(child));

    root.simulate_click({75, 75});
    REQUIRE(clicked);
}

TEST_CASE("Click: click misses child outside bounds", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    bool clicked = false;
    child->on_click = [&]() { clicked = true; };
    root.add_child(std::move(child));

    root.simulate_click({10, 10}); // Outside child bounds
    REQUIRE_FALSE(clicked);
}

TEST_CASE("Click: click on correct target among siblings", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 300, 100});

    int target = -1;
    auto a = std::make_unique<View>();
    a->set_bounds({0, 0, 100, 100});
    a->on_click = [&]() { target = 0; };

    auto b = std::make_unique<View>();
    b->set_bounds({100, 0, 100, 100});
    b->on_click = [&]() { target = 1; };

    auto c = std::make_unique<View>();
    c->set_bounds({200, 0, 100, 100});
    c->on_click = [&]() { target = 2; };

    root.add_child(std::move(a));
    root.add_child(std::move(b));
    root.add_child(std::move(c));

    root.simulate_click({150, 50});
    REQUIRE(target == 1);
}

TEST_CASE("Click: nested child receives click", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 300, 300});

    auto parent = std::make_unique<View>();
    parent->set_bounds({50, 50, 200, 200});

    auto child = std::make_unique<View>();
    child->set_bounds({25, 25, 100, 100});
    bool child_clicked = false;
    child->on_click = [&]() { child_clicked = true; };
    parent->add_child(std::move(child));
    root.add_child(std::move(parent));

    root.simulate_click({100, 100}); // Inside nested child
    REQUIRE(child_clicked);
}

TEST_CASE("Click: invisible child does not receive click", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    child->set_visible(false);
    bool clicked = false;
    child->on_click = [&]() { clicked = true; };
    root.add_child(std::move(child));

    root.simulate_click({75, 75});
    REQUIRE_FALSE(clicked);
}

TEST_CASE("Click: on_mouse_down fires on simulate_click", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    struct ClickTracker : View {
        bool down = false;
        void on_mouse_down(Point) override { down = true; }
    };

    auto child = std::make_unique<ClickTracker>();
    child->set_bounds({0, 0, 200, 200});
    auto* cp = child.get();
    root.add_child(std::move(child));

    root.simulate_click({100, 100});
    REQUIRE(cp->down);
}

TEST_CASE("Click: multiple clicks accumulate", "[events][click]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    auto child = std::make_unique<View>();
    child->set_bounds({0, 0, 200, 200});
    int count = 0;
    child->on_click = [&]() { count++; };
    root.add_child(std::move(child));

    root.simulate_click({100, 100});
    root.simulate_click({100, 100});
    root.simulate_click({100, 100});
    REQUIRE(count == 3);
}

TEST_CASE("Click: JS registerClick dispatches event", "[events][click]") {
    TestEnvironment env(200, 200);
    env.eval(R"JS(
        createPanel("btn");
        setFlex("btn", "width", 200);
        setFlex("btn", "height", 200);
        registerClick("btn");
        var clicked = false;
        on("btn", "click", function() { clicked = true; });
    )JS");
    env.root.layout_children();

    env.root.simulate_click({100, 100});
    auto result = env.engine.evaluate("clicked");
    REQUIRE(result.getWithDefault<bool>(false) == true);
}
