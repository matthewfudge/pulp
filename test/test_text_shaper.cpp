#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/canvas/text_shaper.hpp>

using namespace pulp::canvas;
using Catch::Matchers::WithinAbs;

// ── Prepare ─────────────────────────────────────────────────────────────

TEST_CASE("TextShaper prepare empty string", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("", "system", 14);
    REQUIRE(prepared.empty());
}

TEST_CASE("TextShaper prepare single word", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Hello", "system", 14);
    REQUIRE_FALSE(prepared.empty());
    REQUIRE(prepared.segments().size() == 1);
    REQUIRE(prepared.segments()[0].text == "Hello");
    REQUIRE(prepared.segments()[0].width > 0);
}

TEST_CASE("TextShaper prepare splits on spaces", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Hello World", "system", 14);
    // "Hello" + " " + "World" = 3 segments
    REQUIRE(prepared.segments().size() == 3);
    REQUIRE(prepared.segments()[0].text == "Hello");
    REQUIRE(prepared.segments()[1].is_whitespace);
    REQUIRE(prepared.segments()[2].text == "World");
}

TEST_CASE("TextShaper prepare handles newlines", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Line1\nLine2", "system", 14);
    // "Line1" + "\n" + "Line2"
    REQUIRE(prepared.segments().size() == 3);
    REQUIRE(prepared.segments()[1].is_newline);
}

TEST_CASE("TextShaper prepare caches measurements", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto p1 = shaper.prepare("Hello World", "system", 14);
    auto p2 = shaper.prepare("Hello World", "system", 14);

    // Same segments, same widths (from cache)
    REQUIRE(p1.segments().size() == p2.segments().size());
    for (size_t i = 0; i < p1.segments().size(); ++i)
        REQUIRE(p1.segments()[i].width == p2.segments()[i].width);
}

// ── Layout (the cheap path — just arithmetic) ───────────────────────────

TEST_CASE("TextShaper layout single line", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Short", "system", 14);
    auto layout = shaper.layout(prepared, 1000.0f);

    REQUIRE(layout.line_count == 1);
    REQUIRE(layout.total_height > 0);
}

TEST_CASE("TextShaper layout wraps long text", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare(
        "This is a long sentence that should wrap to multiple lines", "system", 14);

    auto layout = shaper.layout(prepared, 100.0f);
    REQUIRE(layout.line_count > 1);
}

TEST_CASE("TextShaper layout respects newlines", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Line1\nLine2\nLine3", "system", 14);

    auto layout = shaper.layout(prepared, 1000.0f);
    REQUIRE(layout.line_count == 3);
}

TEST_CASE("TextShaper layout_with_lines materializes text", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Hello World", "system", 14);

    auto layout = shaper.layout_with_lines(prepared, 1000.0f);
    REQUIRE(layout.line_count == 1);
    REQUIRE(layout.lines[0].text.find("Hello") != std::string::npos);
}

TEST_CASE("TextShaper measure_height", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("Line1\nLine2", "system", 14);

    float height = shaper.measure_height(prepared, 1000.0f, 20.0f);
    REQUIRE_THAT(height, WithinAbs(40.0f, 1.0f));  // 2 lines * 20px
}

TEST_CASE("TextShaper count_lines", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare("A\nB\nC", "system", 14);
    REQUIRE(shaper.count_lines(prepared, 1000.0f) == 3);
}

TEST_CASE("TextShaper reflow is cheap (same prepared, different width)", "[canvas][text_shaper]") {
    TextShaper shaper;
    auto prepared = shaper.prepare(
        "This text should reflow at different widths without re-shaping", "system", 14);

    auto wide = shaper.layout(prepared, 500.0f);
    auto narrow = shaper.layout(prepared, 100.0f);

    // Narrow layout should have more lines
    REQUIRE(narrow.line_count > wide.line_count);
    // But both use the same segments (no re-measurement)
}

TEST_CASE("TextShaper clear_cache", "[canvas][text_shaper]") {
    TextShaper shaper;
    shaper.prepare("test", "system", 14);
    shaper.clear_cache();
    // Should still work after clearing cache (just re-measures)
    auto p = shaper.prepare("test", "system", 14);
    REQUIRE_FALSE(p.empty());
}

TEST_CASE("TextShaper uses_real_shaping reports backend", "[canvas][text_shaper]") {
    TextShaper shaper;
    // In test builds without GPU, this returns false (estimation fallback)
    // In GPU builds with Skia, this returns true
    // Either way, the API works identically
    bool has_shaping = shaper.uses_real_shaping();
    (void)has_shaping;  // Don't assert — depends on build config
}

TEST_CASE("TextShaper attributed string", "[canvas][text_shaper]") {
    TextShaper shaper;
    AttributedString text;
    text.append("Bold ");
    text.append("Normal");

    auto prepared = shaper.prepare(text);
    REQUIRE_FALSE(prepared.empty());

    auto layout = shaper.layout(prepared, 1000.0f);
    REQUIRE(layout.line_count >= 1);
}

TEST_CASE("Global text shaper singleton", "[canvas][text_shaper]") {
    auto& s1 = global_text_shaper();
    auto& s2 = global_text_shaper();
    REQUIRE(&s1 == &s2);
}
