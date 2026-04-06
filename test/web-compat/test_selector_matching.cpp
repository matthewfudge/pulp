// Selector/ID matching tests — validates widget lookup by ID and type checking

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;

TEST_CASE("Selector: widget lookup by ID", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(
        createPanel("header");
        createPanel("content");
        createPanel("footer");
    )JS");

    REQUIRE(env.widget("header") != nullptr);
    REQUIRE(env.widget("content") != nullptr);
    REQUIRE(env.widget("footer") != nullptr);
    REQUIRE(env.widget("nonexistent") == nullptr);
}

TEST_CASE("Selector: createKnob creates Knob type", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(createKnob("k1");)JS");
    auto* w = env.widget("k1");
    REQUIRE(w != nullptr);
    REQUIRE(dynamic_cast<Knob*>(w) != nullptr);
}

TEST_CASE("Selector: createFader creates Fader type", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(createFader("f1", "vertical");)JS");
    auto* w = env.widget("f1");
    REQUIRE(w != nullptr);
    REQUIRE(dynamic_cast<Fader*>(w) != nullptr);
}

TEST_CASE("Selector: createToggle creates Toggle type", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(createToggle("t1");)JS");
    auto* w = env.widget("t1");
    REQUIRE(w != nullptr);
    REQUIRE(dynamic_cast<Toggle*>(w) != nullptr);
}

TEST_CASE("Selector: createLabel creates Label type", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(createLabel("lbl");)JS");
    auto* w = env.widget("lbl");
    REQUIRE(w != nullptr);
    REQUIRE(dynamic_cast<Label*>(w) != nullptr);
}

TEST_CASE("Selector: widget ID matches set_id", "[events][selector]") {
    View v;
    v.set_id("my-widget");
    REQUIRE(v.id() == "my-widget");
}

TEST_CASE("Selector: ID empty by default", "[events][selector]") {
    View v;
    REQUIRE(v.id().empty());
}

TEST_CASE("Selector: removeWidget removes from lookup", "[events][selector]") {
    TestEnvironment env;
    env.eval(R"JS(
        createPanel("temp");
        removeWidget("temp");
    )JS");
    REQUIRE(env.widget("temp") == nullptr);
}

TEST_CASE("Selector: parent() returns correct parent", "[events][selector]") {
    View root;
    auto child = std::make_unique<View>();
    auto* cp = child.get();
    root.add_child(std::move(child));
    REQUIRE(cp->parent() == &root);
}

TEST_CASE("Selector: parent of root is null", "[events][selector]") {
    View root;
    REQUIRE(root.parent() == nullptr);
}

TEST_CASE("Selector: child_at returns correct child", "[events][selector]") {
    View root;
    auto a = std::make_unique<View>();
    auto* ap = a.get();
    auto b = std::make_unique<View>();
    auto* bp = b.get();
    root.add_child(std::move(a));
    root.add_child(std::move(b));
    REQUIRE(root.child_at(0) == ap);
    REQUIRE(root.child_at(1) == bp);
}

TEST_CASE("Selector: child_count tracks additions", "[events][selector]") {
    View root;
    REQUIRE(root.child_count() == 0);
    root.add_child(std::make_unique<View>());
    REQUIRE(root.child_count() == 1);
    root.add_child(std::make_unique<View>());
    REQUIRE(root.child_count() == 2);
}
