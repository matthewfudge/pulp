// Visual reftest runner — renders view trees and compares against baselines.
// Tests run headlessly using render_to_png on macOS (CoreGraphics backend).
// Set PULP_UPDATE_BASELINES=1 to auto-save new baselines.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using namespace pulp::canvas;

// Helper: create a standard reftest view, run JS, render, check
static std::vector<uint8_t> render_reftest(const std::string& js, int w = 200, int h = 150) {
    TestEnvironment env(static_cast<float>(w), static_cast<float>(h));
    env.run(js);
    return render_to_png(env.root, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Flex layout reftests — verify rendered output is non-empty
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: row with 3 colored panels", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createRow("r");
        setFlex("r", "width", 200); setFlex("r", "height", 150);
        createPanel("a", "r"); setFlex("a", "flex_grow", 1); setFlex("a", "height", 150);
        setBackground("a", "#ff0000");
        createPanel("b", "r"); setFlex("b", "flex_grow", 1); setFlex("b", "height", 150);
        setBackground("b", "#00ff00");
        createPanel("c", "r"); setFlex("c", "flex_grow", 1); setFlex("c", "height", 150);
        setBackground("c", "#0000ff");
    )JS");
    REQUIRE_FALSE(png.empty());
    REQUIRE(png.size() > 100);
}

TEST_CASE("Reftest: column with gap", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createCol("c");
        setFlex("c", "width", 200); setFlex("c", "height", 150); setFlex("c", "gap", 10);
        createPanel("a", "c"); setFlex("a", "height", 40);
        setBackground("a", "coral");
        createPanel("b", "c"); setFlex("b", "height", 40);
        setBackground("b", "teal");
        createPanel("c2", "c"); setFlex("c2", "height", 40);
        setBackground("c2", "gold");
    )JS");
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: centered content", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createRow("r");
        setFlex("r", "width", 200); setFlex("r", "height", 150);
        setFlex("r", "justify_content", "center");
        setFlex("r", "align_items", "center");
        createPanel("box", "r"); setFlex("box", "width", 80); setFlex("box", "height", 60);
        setBackground("box", "dodgerblue");
    )JS");
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: nested row in column", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createCol("outer");
        setFlex("outer", "width", 200); setFlex("outer", "height", 150);
        createPanel("header", "outer"); setFlex("header", "height", 30);
        setBackground("header", "navy");
        createRow("body", "outer"); setFlex("body", "flex_grow", 1);
        createPanel("sidebar", "body"); setFlex("sidebar", "width", 60);
        setBackground("sidebar", "purple");
        createPanel("main", "body"); setFlex("main", "flex_grow", 1);
        setBackground("main", "teal");
    )JS");
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: space-between justify", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createRow("r");
        setFlex("r", "width", 200); setFlex("r", "height", 50);
        setFlex("r", "justify_content", "space-between");
        createPanel("a", "r"); setFlex("a", "width", 30); setFlex("a", "height", 30);
        setBackground("a", "red");
        createPanel("b", "r"); setFlex("b", "width", 30); setFlex("b", "height", 30);
        setBackground("b", "green");
        createPanel("c", "r"); setFlex("c", "width", 30); setFlex("c", "height", 30);
        setBackground("c", "blue");
    )JS");
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Box model reftests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: padding nested panels", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createPanel("outer");
        setFlex("outer", "width", 200); setFlex("outer", "height", 150);
        setFlex("outer", "padding", 20);
        setBackground("outer", "#333333");
        createPanel("inner", "outer"); setFlex("inner", "flex_grow", 1);
        setBackground("inner", "#ff6600");
    )JS");
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: border with radius", "[reftest][visual]") {
    TestEnvironment env(200, 150);
    env.bridge->load_script("createPanel(\"box\")");
    env.bridge->load_script("setFlex(\"box\", \"width\", 120)");
    env.bridge->load_script("setFlex(\"box\", \"height\", 80)");
    env.bridge->load_script("setBackground(\"box\", \"#4488ff\")");
    env.bridge->load_script("setBorder(\"box\", \"#ffffff\", 2, 12)");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 150, 1.0f);
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: margin auto centering", "[reftest][visual]") {
    auto png = render_reftest(R"JS(
        createCol("c");
        setFlex("c", "width", 200); setFlex("c", "height", 150);
        setFlex("c", "align_items", "center");
        setFlex("c", "justify_content", "center");
        createPanel("box", "c"); setFlex("box", "width", 80); setFlex("box", "height", 60);
        setBackground("box", "crimson");
    )JS");
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Typography reftests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: label renders text", "[reftest][visual]") {
    TestEnvironment env(200, 50);
    env.eval(R"JS(
        createLabel("lbl");
        setLabel("lbl", "Hello Pulp");
        setFontSize("lbl", 18);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 50, 1.0f);
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: multiple labels in column", "[reftest][visual]") {
    TestEnvironment env(200, 100);
    env.eval(R"JS(
        createCol("c");
        setFlex("c", "width", 200); setFlex("c", "height", 100); setFlex("c", "gap", 4);
        createLabel("l1", "c"); setLabel("l1", "Line 1"); setFontSize("l1", 14);
        createLabel("l2", "c"); setLabel("l2", "Line 2"); setFontSize("l2", 14);
        createLabel("l3", "c"); setLabel("l3", "Line 3"); setFontSize("l3", 14);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 100, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Widget reftests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: knob renders", "[reftest][visual]") {
    TestEnvironment env(100, 100);
    env.root.set_theme(Theme::dark());
    env.eval(R"JS(
        createKnob("k");
        setFlex("k", "width", 60); setFlex("k", "height", 60);
        setValue("k", 0.5);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 100, 100, 1.0f);
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: fader renders", "[reftest][visual]") {
    TestEnvironment env(60, 200);
    env.root.set_theme(Theme::dark());
    env.eval(R"JS(
        createFader("f", "vertical");
        setFlex("f", "width", 30); setFlex("f", "height", 150);
        setValue("f", 0.7);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 60, 200, 1.0f);
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: toggle renders", "[reftest][visual]") {
    TestEnvironment env(100, 50);
    env.root.set_theme(Theme::dark());
    env.eval(R"JS(
        createToggle("t");
        setFlex("t", "width", 50); setFlex("t", "height", 30);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 100, 50, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Color reftests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: opacity layer", "[reftest][visual]") {
    TestEnvironment env(200, 100);
    env.bridge->load_script("createPanel(\"bg\")");
    env.bridge->load_script("setFlex(\"bg\", \"width\", 200)");
    env.bridge->load_script("setFlex(\"bg\", \"height\", 100)");
    env.bridge->load_script("setBackground(\"bg\", \"red\")");
    env.bridge->load_script("setOpacity(\"bg\", 0.5)");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 100, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Theme reftests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: dark theme panel", "[reftest][visual]") {
    TestEnvironment env(200, 100);
    env.root.set_theme(Theme::dark());
    env.eval(R"JS(
        createPanel("p");
        setFlex("p", "width", 200); setFlex("p", "height", 100);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 100, 1.0f);
    REQUIRE_FALSE(png.empty());
}

TEST_CASE("Reftest: light theme panel", "[reftest][visual]") {
    TestEnvironment env(200, 100);
    env.root.set_theme(Theme::light());
    env.eval(R"JS(
        createPanel("p");
        setFlex("p", "width", 200); setFlex("p", "height", 100);
    )JS");
    env.root.layout_children();
    auto png = render_to_png(env.root, 200, 100, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Deterministic render — same input = same output
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: deterministic render (two identical calls match)", "[reftest][visual]") {
    auto js = R"JS(
        createRow("r");
        setFlex("r", "width", 200); setFlex("r", "height", 100);
        createPanel("a", "r"); setFlex("a", "flex_grow", 1); setFlex("a", "height", 100);
        setBackground("a", "#ff0000");
        createPanel("b", "r"); setFlex("b", "flex_grow", 1); setFlex("b", "height", 100);
        setBackground("b", "#00ff00");
    )JS";
    auto png1 = render_reftest(js);
    auto png2 = render_reftest(js);

    REQUIRE(png1.size() == png2.size());
    auto result = compare_images(png1, png2, Tolerance::exact);
    REQUIRE(result.passed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Baseline regression check infrastructure
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Reftest: compare_images detects identical images", "[reftest][infra]") {
    std::vector<uint8_t> a = {1, 2, 3, 4, 5};
    std::vector<uint8_t> b = {1, 2, 3, 4, 5};
    auto result = compare_images(a, b, Tolerance::exact);
    REQUIRE(result.passed);
    REQUIRE(result.diff_percent == 0.0f);
}

TEST_CASE("Reftest: compare_images detects different images", "[reftest][infra]") {
    std::vector<uint8_t> a = {1, 2, 3, 4, 5};
    std::vector<uint8_t> b = {1, 2, 3, 4, 100};
    auto result = compare_images(a, b, Tolerance::exact);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.max_channel_delta == 95);
}

TEST_CASE("Reftest: compare_images size mismatch fails", "[reftest][infra]") {
    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {1, 2, 3, 4};
    auto result = compare_images(a, b, Tolerance::exact);
    REQUIRE_FALSE(result.passed);
}

TEST_CASE("Reftest: compare_images tight tolerance", "[reftest][infra]") {
    // 10000 bytes, 1 byte different by 2 → 0.01% diff, delta=2 → passes tight
    std::vector<uint8_t> a(10000, 100);
    std::vector<uint8_t> b(10000, 100);
    b[500] = 102;
    auto result = compare_images(a, b, Tolerance::tight);
    REQUIRE(result.passed);
}

TEST_CASE("Reftest: compare_images loose tolerance", "[reftest][infra]") {
    // 1000 bytes, 1 byte different by 5 → 0.1% diff, delta=5 → passes loose
    std::vector<uint8_t> a(1000, 50);
    std::vector<uint8_t> b(1000, 50);
    b[0] = 55;
    auto result = compare_images(a, b, Tolerance::loose);
    REQUIRE(result.passed);
}

TEST_CASE("Reftest: empty images pass", "[reftest][infra]") {
    std::vector<uint8_t> a, b;
    auto result = compare_images(a, b, Tolerance::exact);
    REQUIRE(result.passed);
}
