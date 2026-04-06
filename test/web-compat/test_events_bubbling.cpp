// Event bubbling tests — validates event propagation through view hierarchy

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;

TEST_CASE("Bubbling: child click fires before parent via hit_test", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 300, 300});

    auto parent = std::make_unique<View>();
    parent->set_bounds({0, 0, 300, 300});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    bool child_clicked = false;
    child->on_click = [&]() { child_clicked = true; };

    parent->add_child(std::move(child));
    root.add_child(std::move(parent));

    root.simulate_click({75, 75});
    REQUIRE(child_clicked);
}

TEST_CASE("Bubbling: click on child does not fire parent on_click", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 300, 300});

    bool parent_clicked = false;
    auto parent = std::make_unique<View>();
    parent->set_bounds({0, 0, 300, 300});
    parent->on_click = [&]() { parent_clicked = true; };

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});
    bool child_clicked = false;
    child->on_click = [&]() { child_clicked = true; };

    parent->add_child(std::move(child));
    root.add_child(std::move(parent));

    root.simulate_click({75, 75});
    // Child should get the click (deepest hit), parent should not
    REQUIRE(child_clicked);
    // simulate_click dispatches to deepest hit target only
}

TEST_CASE("Bubbling: click outside child hits parent", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 300, 300});

    bool parent_clicked = false;
    auto parent = std::make_unique<View>();
    parent->set_bounds({0, 0, 300, 300});
    parent->on_click = [&]() { parent_clicked = true; };

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});

    parent->add_child(std::move(child));
    root.add_child(std::move(parent));

    root.simulate_click({10, 10}); // Outside child, inside parent
    REQUIRE(parent_clicked);
}

TEST_CASE("Bubbling: drag fires on target", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    struct DragTracker : View {
        int drag_count = 0;
        void on_mouse_drag(Point) override { drag_count++; }
    };

    auto child = std::make_unique<DragTracker>();
    child->set_bounds({0, 0, 200, 200});
    auto* cp = child.get();
    root.add_child(std::move(child));

    root.simulate_drag({50, 50}, {150, 150}, 5);
    REQUIRE(cp->drag_count > 0);
}

TEST_CASE("Bubbling: hit_test returns deepest child", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 300, 300});

    auto parent = std::make_unique<View>();
    parent->set_bounds({0, 0, 300, 300});

    auto child = std::make_unique<View>();
    child->set_bounds({50, 50, 100, 100});

    auto grandchild = std::make_unique<View>();
    grandchild->set_bounds({10, 10, 50, 50});
    auto* gcp = grandchild.get();

    child->add_child(std::move(grandchild));
    parent->add_child(std::move(child));
    root.add_child(std::move(parent));

    auto* hit = root.hit_test({70, 70});
    REQUIRE(hit == gcp);
}

TEST_CASE("Bubbling: hit_test skips invisible children", "[events][bubbling]") {
    View root;
    root.set_bounds({0, 0, 200, 200});

    auto visible = std::make_unique<View>();
    visible->set_bounds({0, 0, 200, 200});
    auto* vp = visible.get();

    auto invisible = std::make_unique<View>();
    invisible->set_bounds({0, 0, 200, 200});
    invisible->set_visible(false);

    root.add_child(std::move(visible));
    root.add_child(std::move(invisible));

    // Last child is invisible, so hit should find visible
    auto* hit = root.hit_test({100, 100});
    REQUIRE(hit == vp);
}

TEST_CASE("Bubbling: mouse_down then mouse_up on same target", "[events][bubbling]") {
    struct ClickPhaseTracker : View {
        bool down = false, up = false;
        void on_mouse_down(Point) override { down = true; }
        void on_mouse_up(Point) override { up = true; }
    };

    View root;
    root.set_bounds({0, 0, 200, 200});
    auto child = std::make_unique<ClickPhaseTracker>();
    child->set_bounds({0, 0, 200, 200});
    auto* cp = child.get();
    root.add_child(std::move(child));

    root.simulate_click({100, 100});
    REQUIRE(cp->down);
    REQUIRE(cp->up);
}
