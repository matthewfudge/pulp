// Automated test for design tool layout structure
// Validates that the JS creates the correct view tree with proper sizing
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/text_editor.hpp>
#include <pulp/view/ui_components.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/view/script_engine.hpp>
#include <pulp/view/widget_bridge.hpp>
#include <pulp/state/store.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace pulp::view;
namespace fs = std::filesystem;

static std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string find_js_file(const std::string& name) {
    // Search relative to the test binary
    auto dir = fs::current_path();
    while (!dir.empty()) {
        auto candidate = dir / "examples" / "design-tool" / name;
        if (fs::exists(candidate)) return candidate.string();
        auto parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

TEST_CASE("Design tool: JS creates three-column layout", "[design-tool]") {
    auto js_path = find_js_file("design-tool.js");
    if (js_path.empty()) {
        SKIP("design-tool.js not found");
        return;
    }

    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.set_bounds({0, 0, 1100, 700});

    state::StateStore store;
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);

    // Load oklch.js first
    auto oklch_path = find_js_file("oklch.js");
    if (!oklch_path.empty()) {
        bridge.load_script(read_file(oklch_path));
    }

    // Load main JS
    bridge.load_script(read_file(js_path));
    root.layout_children();

    // Should have: toolbar, main-area, status-bar as top-level children
    REQUIRE(root.child_count() >= 3);

    // Toolbar should be 44px tall
    auto* toolbar = root.child_at(0);
    REQUIRE_THAT(toolbar->bounds().height, Catch::Matchers::WithinAbs(44.0f, 1.0f));

    // Status bar should be 28px tall
    auto* status = root.child_at(root.child_count() - 1);
    REQUIRE_THAT(status->bounds().height, Catch::Matchers::WithinAbs(28.0f, 1.0f));

    // Main area should fill remaining space
    auto* main_area = root.child_at(1);
    REQUIRE(main_area->bounds().height > 600.0f);
}

TEST_CASE("Design tool: left panel is 310px, right panel is 272px", "[design-tool]") {
    auto js_path = find_js_file("design-tool.js");
    if (js_path.empty()) {
        SKIP("design-tool.js not found");
        return;
    }

    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.set_bounds({0, 0, 1100, 700});

    state::StateStore store;
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);

    auto oklch_path = find_js_file("oklch.js");
    if (!oklch_path.empty()) bridge.load_script(read_file(oklch_path));
    bridge.load_script(read_file(js_path));
    root.layout_children();

    // Main area is child 1, should be a row with 3 children
    auto* main_area = root.child_at(1);
    REQUIRE(main_area->child_count() >= 3);

    // Left panel
    auto* left = main_area->child_at(0);
    REQUIRE_THAT(left->bounds().width, Catch::Matchers::WithinAbs(310.0f, 2.0f));

    // Right panel
    auto* right = main_area->child_at(main_area->child_count() - 1);
    REQUIRE_THAT(right->bounds().width, Catch::Matchers::WithinAbs(272.0f, 2.0f));

    // Center panel should fill remaining
    auto* center = main_area->child_at(1);
    REQUIRE(center->bounds().width > 400.0f);
}

TEST_CASE("Design tool: toolbar has space-between layout", "[design-tool]") {
    auto js_path = find_js_file("design-tool.js");
    if (js_path.empty()) {
        SKIP("design-tool.js not found");
        return;
    }

    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.set_bounds({0, 0, 1100, 700});

    state::StateStore store;
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);

    auto oklch_path = find_js_file("oklch.js");
    if (!oklch_path.empty()) bridge.load_script(read_file(oklch_path));
    bridge.load_script(read_file(js_path));
    root.layout_children();

    auto* toolbar = root.child_at(0);
    REQUIRE(toolbar->child_count() >= 2);

    // First child (toolbar-left) should be at left
    auto* left_group = toolbar->child_at(0);
    REQUIRE(left_group->bounds().x < 50.0f);

    // Last child (toolbar-right) should be near right edge
    auto* right_group = toolbar->child_at(toolbar->child_count() - 1);
    float right_edge = right_group->bounds().x + right_group->bounds().width;
    REQUIRE(right_edge > 1000.0f); // Near the right side of 1100px
}

TEST_CASE("Design tool: plugin chrome has traffic lights", "[design-tool]") {
    auto js_path = find_js_file("design-tool.js");
    if (js_path.empty()) {
        SKIP("design-tool.js not found");
        return;
    }

    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.set_bounds({0, 0, 1100, 700});

    state::StateStore store;
    ScriptEngine engine;
    WidgetBridge bridge(engine, root, store);

    auto oklch_path = find_js_file("oklch.js");
    if (!oklch_path.empty()) bridge.load_script(read_file(oklch_path));
    bridge.load_script(read_file(js_path));
    root.layout_children();

    // Find the traffic light views by walking the tree
    auto* tl_close = bridge.widget("tl-close");
    auto* tl_min = bridge.widget("tl-min");
    auto* tl_max = bridge.widget("tl-max");

    REQUIRE(tl_close != nullptr);
    REQUIRE(tl_min != nullptr);
    REQUIRE(tl_max != nullptr);

    // They should be 12x12
    REQUIRE_THAT(tl_close->bounds().width, Catch::Matchers::WithinAbs(12.0f, 1.0f));
    REQUIRE_THAT(tl_close->bounds().height, Catch::Matchers::WithinAbs(12.0f, 1.0f));
}
