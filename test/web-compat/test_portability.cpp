// Portability tests — common web UI patterns that should work in Pulp's
// CSS/JS bridge without JS errors and with correct layout.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.hpp"

using namespace pulp::test;
using namespace pulp::view;
using Catch::Matchers::WithinAbs;

// Helper: run JS and verify no console errors
static bool run_without_errors(TestEnvironment& env, const std::string& js) {
    std::vector<std::string> errors;
    env.engine.set_log_callback([&](std::string_view level, std::string_view msg) {
        if (level == "error") errors.emplace_back(msg);
    });
    env.bridge->load_script(js);
    env.root.layout_children();
    return errors.empty();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Holy grail layout
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: holy grail layout", "[portability]") {
    TestEnvironment env(800, 600);
    bool ok = run_without_errors(env, R"JS(
        createCol("root");
        setFlex("root", "width", 800); setFlex("root", "height", 600);

        createRow("header", "root"); setFlex("header", "height", 50);
        setBackground("header", "#2d2d44");

        createRow("body", "root"); setFlex("body", "flex_grow", 1);
        createPanel("left", "body"); setFlex("left", "width", 150);
        setBackground("left", "#3d3d54");
        createPanel("main", "body"); setFlex("main", "flex_grow", 1);
        setBackground("main", "#1a1a2e");
        createPanel("right", "body"); setFlex("right", "width", 150);
        setBackground("right", "#3d3d54");

        createRow("footer", "root"); setFlex("footer", "height", 40);
        setBackground("footer", "#2d2d44");
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("root")->child_count() == 3);

    // Verify body fills remaining space
    auto body_h = env.widget("body")->bounds().height;
    REQUIRE_THAT(body_h, WithinAbs(510.0f, 1.0f)); // 600 - 50 - 40

    auto png = render_to_png(env.root, 800, 600, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Card grid
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: card grid", "[portability]") {
    TestEnvironment env(400, 300);
    bool ok = run_without_errors(env, R"JS(
        createGrid("grid");
        setGrid("grid", "template_columns", "1fr 1fr 1fr");
        setGrid("grid", "gap", 12);
        setFlex("grid", "width", 400); setFlex("grid", "height", 300);
        setFlex("grid", "padding", 12);

        for (var i = 0; i < 6; i++) {
            createPanel("card" + i, "grid");
            setBackground("card" + i, "#2a2a3e");
        }
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("grid")->child_count() == 6);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Nav bar
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: horizontal nav bar", "[portability]") {
    TestEnvironment env(600, 50);
    bool ok = run_without_errors(env, R"JS(
        createRow("nav");
        setFlex("nav", "width", 600); setFlex("nav", "height", 50);
        setFlex("nav", "gap", 4); setFlex("nav", "padding_left", 16);
        setFlex("nav", "align_items", "center");
        setBackground("nav", "#1a1a2e");

        createLabel("logo", "Pulp", "nav"); setFontSize("logo", 18);
        createLabel("n1", "Home", "nav"); setFontSize("n1", 14);
        createLabel("n2", "About", "nav"); setFontSize("n2", 14);
        createLabel("n3", "Contact", "nav"); setFontSize("n3", 14);
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("nav")->child_count() == 4);

    auto png = render_to_png(env.root, 600, 50, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Accordion
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: accordion sections", "[portability]") {
    TestEnvironment env(300, 400);
    bool ok = run_without_errors(env, R"JS(
        createCol("acc");
        setFlex("acc", "width", 300); setFlex("acc", "height", 400); setFlex("acc", "gap", 2);

        for (var i = 0; i < 5; i++) {
            createRow("hdr" + i, "acc"); setFlex("hdr" + i, "height", 36);
            setBackground("hdr" + i, "#333344");
            createLabel("lbl" + i, "Section " + (i+1), "hdr" + i);
        }
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("acc")->child_count() == 5);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Toast notifications
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: toast notification stack", "[portability]") {
    TestEnvironment env(400, 300);
    bool ok = run_without_errors(env, R"JS(
        createCol("toasts");
        setFlex("toasts", "width", 250); setFlex("toasts", "height", 300);
        setFlex("toasts", "gap", 8); setFlex("toasts", "padding", 8);
        setFlex("toasts", "justify_content", "end");

        createPanel("t1", "toasts"); setFlex("t1", "height", 50);
        setBackground("t1", "#2d6a2d");
        createPanel("t2", "toasts"); setFlex("t2", "height", 50);
        setBackground("t2", "#8b2222");
        createPanel("t3", "toasts"); setFlex("t3", "height", 50);
        setBackground("t3", "#2255aa");
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("toasts")->child_count() == 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Audio EQ curve
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: audio EQ panel layout", "[portability]") {
    TestEnvironment env(500, 300);
    env.root.set_theme(Theme::pro_audio());
    bool ok = run_without_errors(env, R"JS(
        createCol("eq");
        setFlex("eq", "width", 500); setFlex("eq", "height", 300);

        createPanel("display", "eq"); setFlex("display", "flex_grow", 1);
        setBackground("display", "#0a0a14");

        createRow("knobs", "eq"); setFlex("knobs", "height", 80);
        setFlex("knobs", "gap", 8); setFlex("knobs", "padding", 8);
        setFlex("knobs", "justify_content", "space-evenly");
        setBackground("knobs", "#1a1a28");

        for (var i = 0; i < 5; i++) {
            createKnob("band" + i, "knobs");
            setFlex("band" + i, "width", 48); setFlex("band" + i, "height", 48);
            setValue("band" + i, 0.5);
        }
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("knobs")->child_count() == 5);

    auto png = render_to_png(env.root, 500, 300, 1.0f);
    REQUIRE_FALSE(png.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Resizable panels (layout correctness)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: resizable panel proportions", "[portability]") {
    TestEnvironment env(600, 400);
    bool ok = run_without_errors(env, R"JS(
        createRow("panels");
        setFlex("panels", "width", 600); setFlex("panels", "height", 400);

        createPanel("left", "panels"); setFlex("left", "flex_grow", 1);
        setBackground("left", "#222233");
        createPanel("divider", "panels"); setFlex("divider", "width", 4);
        setBackground("divider", "#555555");
        createPanel("right", "panels"); setFlex("right", "flex_grow", 2);
        setBackground("right", "#222244");
    )JS");
    REQUIRE(ok);

    // Left should be ~1/3 of (600-4), right ~2/3
    auto left_w = env.widget("left")->bounds().width;
    auto right_w = env.widget("right")->bounds().width;
    REQUIRE_THAT(left_w, WithinAbs(198.67f, 2.0f));
    REQUIRE_THAT(right_w, WithinAbs(397.33f, 2.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Sidebar toggle pattern
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: sidebar visibility toggle", "[portability]") {
    TestEnvironment env(600, 400);
    env.run(R"JS(
        createRow("app");
        setFlex("app", "width", 600); setFlex("app", "height", 400);
        createPanel("sidebar", "app"); setFlex("sidebar", "width", 200);
        setBackground("sidebar", "#2a2a3e");
        createPanel("main", "app"); setFlex("main", "flex_grow", 1);
        setBackground("main", "#1a1a2e");
    )JS");

    // Sidebar visible → main should be ~400px
    REQUIRE_THAT(env.widget("main")->bounds().width, WithinAbs(400.0f, 1.0f));

    // Hide sidebar → main should expand to fill
    env.eval(R"JS(setVisible("sidebar", false);)JS");
    env.root.layout_children();
    REQUIRE_THAT(env.widget("main")->bounds().width, WithinAbs(600.0f, 1.0f));

    // Show sidebar → main shrinks back
    env.eval(R"JS(setVisible("sidebar", true);)JS");
    env.root.layout_children();
    REQUIRE_THAT(env.widget("main")->bounds().width, WithinAbs(400.0f, 1.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// JS loop creates many widgets without error
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Portability: JS loop creates 100 items", "[portability]") {
    TestEnvironment env(400, 3000);
    bool ok = run_without_errors(env, R"JS(
        createCol("list");
        setFlex("list", "width", 400); setFlex("list", "height", 3000);
        setFlex("list", "gap", 2);
        for (var i = 0; i < 100; i++) {
            createPanel("item" + i, "list");
            setFlex("item" + i, "height", 28);
        }
    )JS");
    REQUIRE(ok);
    REQUIRE(env.widget("list")->child_count() == 100);
}
