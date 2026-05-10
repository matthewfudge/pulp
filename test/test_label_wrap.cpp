// pulp #1714 — Label::set_multi_line(true) actually wraps.
//
// Before: setWhiteSpace(id, 'normal') flipped multi_line_=true but
// Label::intrinsic_height() returned the single-line height regardless,
// so Yoga locked the height before knowing the wrap width and the
// Label paint path single-lined + truncated.
//
// After: intrinsic_height() returns 0 for multi-line, the yoga_measure
// callback delegates to Label::measure_height_for_width(w) which wraps
// to the parent's allocated content-width and returns line_height *
// (number of wrapped lines).

#include <catch2/catch_test_macros.hpp>
#include <pulp/view/widgets.hpp>

using pulp::view::Label;

TEST_CASE("Label::intrinsic_height is 0 when multi_line_ is set [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("Color palette for bands and glow");
    l.set_font_size(10);
    l.set_multi_line(true);
    REQUIRE(l.intrinsic_height() == 0.0f);
}

TEST_CASE("Label::intrinsic_height is non-zero for single-line [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("Single line");
    l.set_font_size(10);
    // multi_line_ defaults to false
    REQUIRE(l.intrinsic_height() > 0.0f);
}

TEST_CASE("Label::wrap_lines splits at word boundaries [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("Color palette for bands and glow");
    l.set_font_size(10);
    l.set_multi_line(true);

    // 100px wide: should wrap to multiple lines
    auto narrow = l.wrap_lines(100.0f);
    REQUIRE(narrow.size() > 1);

    // 1000px wide: easily fits in one line
    auto wide = l.wrap_lines(1000.0f);
    REQUIRE(wide.size() == 1);
    REQUIRE(wide[0] == "Color palette for bands and glow");
}

TEST_CASE("Label::wrap_lines honors hard newlines [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("First\nSecond\nThird");
    l.set_font_size(10);
    l.set_multi_line(true);

    auto lines = l.wrap_lines(1000.0f);
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0] == "First");
    REQUIRE(lines[1] == "Second");
    REQUIRE(lines[2] == "Third");
}

TEST_CASE("Label::measure_height_for_width grows with line count [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("Color palette for bands and glow");
    l.set_font_size(10);
    l.set_multi_line(true);

    float h_wide = l.measure_height_for_width(1000.0f);
    float h_narrow = l.measure_height_for_width(80.0f);
    REQUIRE(h_narrow > h_wide);
}

TEST_CASE("Label::measure_height_for_width with single-line falls back [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("Single line text");
    l.set_font_size(10);
    // multi_line_ stays false; should return the standard line-height.
    float h = l.measure_height_for_width(50.0f);
    REQUIRE(h > 0.0f);
    REQUIRE(h == l.intrinsic_height());
}

// Codex P2 follow-up on #1714: the wrap-measurement path must use the
// Label's effective font, not a hardcoded "Inter". A label with a
// custom set_font_family / weight that measures wrong-width text
// would lay out at the wrong height.
TEST_CASE("Label::wrap_lines uses set_font_family for measurement [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("alpha bravo charlie delta");
    l.set_font_size(10);
    l.set_multi_line(true);
    // Default family — baseline.
    auto baseline = l.wrap_lines(100.0f);
    // Switch font_family — must not crash and must still produce
    // some reasonable line breakdown (we don't assert the exact
    // glyph metrics differ, only that font_family_ is honored).
    l.set_font_family("MonoTest");
    auto with_family = l.wrap_lines(100.0f);
    REQUIRE_FALSE(with_family.empty());
    // Same content, just measured under a different family — total
    // characters across all lines must equal the input character
    // count (no token loss).
    std::size_t baseline_chars = 0;
    for (auto& s : baseline) baseline_chars += s.size();
    std::size_t with_family_chars = 0;
    for (auto& s : with_family) with_family_chars += s.size();
    // Authored spacing is preserved across either family — only
    // line-break positions may shift.
    REQUIRE(baseline_chars == with_family_chars);
}

// Codex P2 follow-up on #1714: authored spacing must survive the wrap
// pass. The previous implementation collapsed multiple consecutive
// spaces and dropped leading indent.
TEST_CASE("Label::wrap_lines preserves multiple consecutive spaces [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("foo  bar");  // double space between tokens
    l.set_font_size(10);
    l.set_multi_line(true);
    // Wide enough to fit on one line — the rendered string must
    // preserve the double space.
    auto lines = l.wrap_lines(1000.0f);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0] == "foo  bar");
}

TEST_CASE("Label::wrap_lines preserves leading indent on a single line [issue-1714]",
          "[label][wrap]") {
    Label l;
    l.set_text("    indented");  // four-space indent
    l.set_font_size(10);
    l.set_multi_line(true);
    auto lines = l.wrap_lines(1000.0f);
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0] == "    indented");
}
