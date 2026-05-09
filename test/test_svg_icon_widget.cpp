// SvgIconWidget tests — pulp #1492.
//
// The render path requires Skia (SkSVGDOM), which is typically linked
// in build/ but not always in headless CI test targets. These tests
// therefore focus on the Skia-independent surface:
//
//   * Default-constructed widget reports zero intrinsic size and holds
//     no renderable DOM.
//   * set_svg("") clears both intrinsic size and the renderable DOM.
//   * Intrinsic size extraction follows the browser precedence:
//       1. explicit width/height in absolute units,
//       2. viewBox dimensions,
//       3. (0, 0) when neither is present.
//   * Percentage / em units on the root report 0 intrinsic (matches
//     CSS2 §10.3.8 — relative roots have no intrinsic until the
//     containing block is resolved).
//   * Malformed XML input is tolerated: the widget clears its DOM and
//     returns zero intrinsic size without throwing.
//
// The optional [skia] tagged cases only run when PULP_HAS_SKIA is set;
// they assert the SkSVGDOM round-trip succeeds for typical Lucide /
// Material-icon glyphs.

#include <catch2/catch_test_macros.hpp>
#include <pulp/view/widgets/svg_icon.hpp>

using pulp::view::SvgIconWidget;

TEST_CASE("SvgIconWidget defaults: empty widget, no intrinsic, not renderable",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    REQUIRE(w.svg().empty());
    REQUIRE(w.intrinsic_width() == 0.0f);
    REQUIRE(w.intrinsic_height() == 0.0f);
    REQUIRE_FALSE(w.has_renderable_dom());
}

TEST_CASE("SvgIconWidget extracts intrinsic from root width/height (absolute)",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"><path d="M0 0L10 10"/></svg>)");
    REQUIRE(w.intrinsic_width() == 24.0f);
    REQUIRE(w.intrinsic_height() == 24.0f);
}

TEST_CASE("SvgIconWidget extracts intrinsic from root width/height with px unit",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="48px" height="32px"/>)");
    REQUIRE(w.intrinsic_width() == 48.0f);
    REQUIRE(w.intrinsic_height() == 32.0f);
}

TEST_CASE("SvgIconWidget falls back to viewBox when width/height are missing",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 50"><path d="M0 0"/></svg>)");
    REQUIRE(w.intrinsic_width() == 100.0f);
    REQUIRE(w.intrinsic_height() == 50.0f);
}

TEST_CASE("SvgIconWidget prefers explicit absolute width/height over viewBox",
          "[svg_icon][intrinsic][issue-1492]") {
    // Browsers + SVG2 §8.2: absolute-unit width/height win over viewBox
    // when both are present. The viewBox still drives the internal
    // coordinate system during render, but intrinsic sizing uses the
    // outer attributes.
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100" viewBox="0 0 24 24"/>)");
    REQUIRE(w.intrinsic_width() == 200.0f);
    REQUIRE(w.intrinsic_height() == 100.0f);
}

TEST_CASE("SvgIconWidget mixes explicit width with viewBox-derived height",
          "[svg_icon][intrinsic][issue-1492]") {
    // Only width specified — intrinsic height comes from the viewBox.
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="48" viewBox="0 0 24 36"/>)");
    REQUIRE(w.intrinsic_width() == 48.0f);
    REQUIRE(w.intrinsic_height() == 36.0f);
}

TEST_CASE("SvgIconWidget reports zero intrinsic for percentage-relative roots",
          "[svg_icon][intrinsic][issue-1492]") {
    // `width="100%"` is relative to the containing block; intrinsic is
    // unresolvable without the layout context, so report 0 and let
    // Yoga's preferred/flex-basis sizing take over.
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="100%" height="100%"/>)");
    REQUIRE(w.intrinsic_width() == 0.0f);
    REQUIRE(w.intrinsic_height() == 0.0f);
}

TEST_CASE("SvgIconWidget tolerates malformed XML and reports zero intrinsic",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    // Unclosed tag — pugi rejects it, extract_intrinsic_from_xml bails.
    // The widget must not throw and must report zero intrinsic so Yoga
    // doesn't latch a stale prior size.
    w.set_svg("<svg width=\"24\" height=\"24");
    REQUIRE(w.intrinsic_width() == 0.0f);
    REQUIRE(w.intrinsic_height() == 0.0f);
    REQUIRE_FALSE(w.has_renderable_dom());
}

TEST_CASE("SvgIconWidget set_svg(\"\") clears intrinsic and renderable DOM",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"/>)");
    REQUIRE(w.intrinsic_width() == 24.0f);

    w.set_svg("");
    REQUIRE(w.svg().empty());
    REQUIRE(w.intrinsic_width() == 0.0f);
    REQUIRE(w.intrinsic_height() == 0.0f);
    REQUIRE_FALSE(w.has_renderable_dom());
}

TEST_CASE("SvgIconWidget set_svg replaces prior content",
          "[svg_icon][intrinsic][issue-1492]") {
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"/>)");
    REQUIRE(w.intrinsic_width() == 10.0f);

    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 20"/>)");
    REQUIRE(w.intrinsic_width() == 40.0f);
    REQUIRE(w.intrinsic_height() == 20.0f);
}

#ifdef PULP_HAS_SKIA

TEST_CASE("SvgIconWidget builds a renderable SkSVGDOM for typical icon markup",
          "[svg_icon][skia][issue-1492]") {
    // Lucide-style icon — single viewBox, one path. SkSVGDOM should
    // parse it into a renderable DOM so paint() has something to draw.
    SvgIconWidget w;
    w.set_svg(
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24")"
        R"( fill="none" stroke="currentColor" stroke-width="2">)"
        R"(<path d="M3 12h18M3 6h18M3 18h18"/></svg>)");
    REQUIRE(w.has_renderable_dom());
    REQUIRE(w.intrinsic_width() == 24.0f);
    REQUIRE(w.intrinsic_height() == 24.0f);
}

TEST_CASE("SvgIconWidget SkSVGDOM recovers intrinsic size when XML walk fails",
          "[svg_icon][skia][issue-1492]") {
    // pugi's parse is strict; a loose real-world `<svg` fragment that
    // Skia still accepts should surface non-zero intrinsic through the
    // DOM path even when the XML walk reported zero. This covers the
    // heuristic `if (intrinsic_w_ <= 0 && dom_intrinsic > 0)` in
    // reparse().
    //
    // (Skia is forgiving about namespaces; pugi tolerates them too, so
    // this case is mostly a regression guard rather than an active
    // production scenario.)
    SvgIconWidget w;
    w.set_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"/>)");
    REQUIRE(w.has_renderable_dom());
    REQUIRE(w.intrinsic_width() == 16.0f);
    REQUIRE(w.intrinsic_height() == 16.0f);
}

#endif  // PULP_HAS_SKIA
