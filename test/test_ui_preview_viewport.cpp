// WYSIWYG T3 — settle-probe responsive auto-sizing for ui-preview.
//
// Verifies examples/ui-preview/design_viewport_probe.hpp resolves a sensible
// landscape design viewport for fill-width / responsive imported trees, rather
// than collapsing to the requested 360px portrait column (the bug T3 fixes).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <pulp/view/view.hpp>

#include "../examples/ui-preview/design_viewport_probe.hpp"

#include <memory>

using namespace pulp::view;
using pulp::ui_preview::probe_design_viewport;

namespace {

// A fixed-size child (explicit width + height, no grow).
std::unique_ptr<View> make_fixed(float w, float h) {
    auto v = std::make_unique<View>();
    v->flex().preferred_width = w;
    v->flex().preferred_height = h;
    v->flex().flex_shrink = 0.0f;
    return v;
}

}  // namespace

TEST_CASE("T3: a fixed-size landscape design opens landscape, not portrait",
          "[ui-preview][wysiwyg][t3]") {
    // A 1000x300 fixed-size card. The probe must report ~1000 wide, not the
    // 360px portrait the old single-pass-at-render-width path produced.
    View root;
    root.flex().direction = FlexDirection::column;
    root.add_child(make_fixed(1000.0f, 300.0f));

    const auto vp = probe_design_viewport(root);

    REQUIRE(vp.reliable);
    REQUIRE_FALSE(vp.responsive);            // no wrap/reflow
    REQUIRE(vp.width >= 1000.0f);            // true authored width, not 360
    REQUIRE(vp.width > vp.height);           // landscape
}

TEST_CASE("T3: a flex-wrap responsive design settles at its natural width",
          "[ui-preview][wysiwyg][t3]") {
    // Eight 200x100 cards in a wrapping row. At a narrow width they stack into
    // many rows (tall); at a wide width they fit on one row (short). The probe
    // must detect the reflow and settle near 8*200 = 1600 wide.
    View root;
    root.flex().direction = FlexDirection::row;
    root.flex().flex_wrap = FlexWrap::wrap;
    for (int i = 0; i < 8; ++i) root.add_child(make_fixed(200.0f, 100.0f));

    const auto vp = probe_design_viewport(root);

    REQUIRE(vp.reliable);
    REQUIRE(vp.responsive);                  // narrow reflowed taller than wide
    // Natural single-row width is ~1600; allow generous slack for the binary
    // search epsilon. The key assertion is "wide, not the 200px narrow probe".
    REQUIRE(vp.width >= 1400.0f);
    REQUIRE(vp.width > vp.height);           // landscape, not portrait
}

TEST_CASE("T3: a fill-width band does not collapse to a portrait column",
          "[ui-preview][wysiwyg][t3]") {
    // The Chainer-style failure: a column of full-width bands. Each band is a
    // ROW that flex-grows across the width (cross-axis stretch) and holds a
    // fixed-size content block that establishes the natural width. The old
    // single-pass-at-render-width path laid this out at 360 wide and produced
    // a 360px portrait column; the probe must report the content's true width.
    View root;
    root.flex().direction = FlexDirection::column;
    for (int i = 0; i < 3; ++i) {
        auto band = std::make_unique<View>();
        band->flex().direction = FlexDirection::row;
        band->flex().preferred_height = 120.0f;  // bounded height (no v-grow)
        band->flex().flex_shrink = 0.0f;
        // A wide content block inside the band sets the natural width.
        band->add_child(make_fixed(900.0f, 80.0f));
        root.add_child(std::move(band));
    }

    const auto vp = probe_design_viewport(root);

    REQUIRE(vp.reliable);
    REQUIRE(vp.width >= 900.0f);   // content width drives it, not 360
    REQUIRE(vp.width > vp.height); // landscape, not portrait
}
