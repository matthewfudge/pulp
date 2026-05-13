// pulp #1899 — Spectr import-flow visual gap fixes #4 and #5.
//
// (#4) The web-compat shim previously routed `<path>` into the
// unknown-tag default (createCol), producing an empty layout box.
// Spectr's React-rendered icon glyphs emit raw `<svg viewBox="0 0 24 24">
// <path d="M..." stroke="currentColor" stroke-width="2" fill="none"/></svg>`
// markup, so every glyph painted blank. This test pins the new routing
// (createSvgPath under the hood) plus the d / stroke / stroke-width /
// fill / viewBox attribute replay.
//
// (#5) `<input type="range">` was hard-coded to "vertical" orientation
// in the createFader call, which is the wrong default for HTML. The
// HTML / MDN / Web-Audio convention is horizontal-by-default; Spectr's
// MorphSlider, FilterBank, and almost every imported web slider expect
// a 90px-wide horizontal fader. This test pins the new heuristic:
// default horizontal, vertical only when an explicit hint (aria-
// orientation or style.height > style.width) says so.
//
// Both gaps are part of the Spectr webview-baseline visual chase
// (#1899). Closing them is expected to bump the native-vs-webview
// diff score from ~0.662 toward 0.70+.

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.hpp"
#include <pulp/view/svg_path_widget.hpp>
#include <pulp/view/widgets.hpp>

using namespace pulp::test;
using namespace pulp::view;

namespace {

inline std::string js_id(TestEnvironment& env, const std::string& js_var) {
    return std::string(env.engine.evaluate(js_var + "._id")
                            .getWithDefault<std::string_view>(""));
}

inline View* resolve(TestEnvironment& env, const std::string& js_var) {
    auto id = js_id(env, js_var);
    REQUIRE_FALSE(id.empty());
    auto* w = env.widget(id);
    REQUIRE(w != nullptr);
    return w;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Gap #4 — `<path>` routes to SvgPathWidget; d / stroke / stroke-width /
// fill / viewBox propagate through the bridge.
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("svg-1899 (#4): <svg><path>...</path></svg> creates an SvgPathWidget",
          "[svg][import][spectr][issue-1899]") {
    TestEnvironment env(400, 200);
    env.eval(R"JS(
        var __svg = document.createElement('svg');
        __svg.setAttribute('viewBox', '0 0 24 24');
        __svg.setAttribute('width',  '22');
        __svg.setAttribute('height', '16');

        var __path = document.createElement('path');
        __path.setAttribute('d', 'M 2 12 C 8 12 8 4 12 4');
        __path.setAttribute('stroke', '#ffffff');
        __path.setAttribute('stroke-width', '2');
        __path.setAttribute('fill', 'none');

        __svg.appendChild(__path);
        document.body.appendChild(__svg);
    )JS");
    env.root.layout_children();

    auto* path = resolve(env, "__path");
    auto* svg_widget = dynamic_cast<SvgPathWidget*>(path);
    REQUIRE(svg_widget != nullptr);                       // routed, not createCol
    REQUIRE(svg_widget->path_data() == "M 2 12 C 8 12 8 4 12 4");
    REQUIRE(svg_widget->has_stroke());                    // stroke="#ffffff" applied
    REQUIRE(svg_widget->stroke_width() == 2.0f);
    REQUIRE_FALSE(svg_widget->has_fill());                // fill="none" → cleared
    REQUIRE(svg_widget->viewbox_width()  == 24.0f);       // inherited from parent <svg>
    REQUIRE(svg_widget->viewbox_height() == 24.0f);
}

TEST_CASE("svg-1899 (#4): camelCase strokeWidth attribute also routes",
          "[svg][import][spectr][issue-1899]") {
    // JSX historically passes `strokeWidth` rather than the HTML
    // `stroke-width`. Either spelling must reach setSvgStrokeWidth so
    // raw-React imports without a build-time prop-applier still work.
    TestEnvironment env(400, 200);
    env.eval(R"JS(
        var __svg = document.createElement('svg');
        __svg.setAttribute('viewBox', '0 0 16 16');
        var __path = document.createElement('path');
        __path.setAttribute('d', 'M0 0 L 16 16');
        __path.setAttribute('stroke', '#ff0000');
        __path.setAttribute('strokeWidth', '3');
        __svg.appendChild(__path);
        document.body.appendChild(__svg);
    )JS");
    env.root.layout_children();

    auto* widget = dynamic_cast<SvgPathWidget*>(resolve(env, "__path"));
    REQUIRE(widget != nullptr);
    REQUIRE(widget->stroke_width() == 3.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Gap #5 — `<input type="range">` defaults to horizontal orientation.
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("range-1899 (#5): <input type=range> defaults to HORIZONTAL",
          "[input][range][spectr][issue-1899]") {
    TestEnvironment env(400, 200);
    env.eval(R"JS(
        var __slider = document.createElement('input');
        __slider.type = 'range';
        __slider.min  = '0';
        __slider.max  = '1';
        __slider.value = '0.5';
        __slider.style.width = '90px';
        document.body.appendChild(__slider);
    )JS");
    env.root.layout_children();

    auto* fader = dynamic_cast<Fader*>(resolve(env, "__slider"));
    REQUIRE(fader != nullptr);
    REQUIRE(fader->orientation() == Fader::Orientation::horizontal);
}

TEST_CASE("range-1899 (#5): aria-orientation=vertical opts into vertical",
          "[input][range][spectr][issue-1899]") {
    // An explicit `aria-orientation="vertical"` is the only documented
    // way for an author to flip the slider to vertical. Pin that hook
    // so vertical mixer-style faders keep working.
    TestEnvironment env(400, 400);
    env.eval(R"JS(
        var __slider = document.createElement('input');
        __slider.type = 'range';
        __slider.setAttribute('aria-orientation', 'vertical');
        __slider.style.height = '160px';
        document.body.appendChild(__slider);
    )JS");
    env.root.layout_children();

    auto* fader = dynamic_cast<Fader*>(resolve(env, "__slider"));
    REQUIRE(fader != nullptr);
    REQUIRE(fader->orientation() == Fader::Orientation::vertical);
}

TEST_CASE("range-1899 (#5): style.height > style.width also flips to vertical",
          "[input][range][spectr][issue-1899]") {
    // Authors who shape the input element as a tall column without
    // setting aria-orientation should still get a vertical fader.
    TestEnvironment env(400, 400);
    env.eval(R"JS(
        var __slider = document.createElement('input');
        __slider.type = 'range';
        __slider.style.width = '20px';
        __slider.style.height = '200px';
        document.body.appendChild(__slider);
    )JS");
    env.root.layout_children();

    auto* fader = dynamic_cast<Fader*>(resolve(env, "__slider"));
    REQUIRE(fader != nullptr);
    REQUIRE(fader->orientation() == Fader::Orientation::vertical);
}
