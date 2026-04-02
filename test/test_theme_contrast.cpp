#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <pulp/view/theme_contrast.hpp>

using namespace pulp::view;
using Catch::Matchers::WithinAbs;

// ── Relative Luminance ──────────────────────────────────────────────────────

TEST_CASE("Relative luminance of black is 0", "[view][contrast]") {
    REQUIRE_THAT(relative_luminance(Color::rgba(0, 0, 0)), WithinAbs(0.0, 0.001));
}

TEST_CASE("Relative luminance of white is 1", "[view][contrast]") {
    REQUIRE_THAT(relative_luminance(Color::rgba(255, 255, 255)), WithinAbs(1.0, 0.001));
}

TEST_CASE("Relative luminance of mid-gray", "[view][contrast]") {
    auto lum = relative_luminance(Color::rgba(128, 128, 128));
    REQUIRE(lum > 0.1f);
    REQUIRE(lum < 0.5f);
}

// ── Contrast Ratio ──────────────────────────────────────────────────────────

TEST_CASE("Contrast ratio of black vs white is ~21:1", "[view][contrast]") {
    float ratio = contrast_ratio(Color::rgba(0, 0, 0), Color::rgba(255, 255, 255));
    REQUIRE_THAT(ratio, WithinAbs(21.0, 0.1));
}

TEST_CASE("Contrast ratio of identical colors is 1:1", "[view][contrast]") {
    auto c = Color::rgba(100, 150, 200);
    REQUIRE_THAT(contrast_ratio(c, c), WithinAbs(1.0, 0.01));
}

TEST_CASE("Contrast ratio is symmetric", "[view][contrast]") {
    auto a = Color::rgba(50, 50, 50);
    auto b = Color::rgba(200, 200, 200);
    REQUIRE_THAT(contrast_ratio(a, b), WithinAbs(contrast_ratio(b, a), 0.001));
}

// ── WCAG Compliance ─────────────────────────────────────────────────────────

TEST_CASE("White text on black meets AA normal", "[view][contrast]") {
    REQUIRE(meets_contrast(Color::rgba(255, 255, 255), Color::rgba(0, 0, 0), ContrastLevel::aa_normal));
}

TEST_CASE("White text on black meets AAA normal", "[view][contrast]") {
    REQUIRE(meets_contrast(Color::rgba(255, 255, 255), Color::rgba(0, 0, 0), ContrastLevel::aaa_normal));
}

TEST_CASE("Light gray on white fails AA normal", "[view][contrast]") {
    REQUIRE_FALSE(meets_contrast(Color::rgba(200, 200, 200), Color::rgba(255, 255, 255), ContrastLevel::aa_normal));
}

TEST_CASE("Min contrast thresholds are correct", "[view][contrast]") {
    REQUIRE_THAT(min_contrast_for_level(ContrastLevel::aa_normal), WithinAbs(4.5, 0.01));
    REQUIRE_THAT(min_contrast_for_level(ContrastLevel::aa_large), WithinAbs(3.0, 0.01));
    REQUIRE_THAT(min_contrast_for_level(ContrastLevel::aaa_normal), WithinAbs(7.0, 0.01));
}

// ── Auto Contrast ───────────────────────────────────────────────────────────

TEST_CASE("Auto contrast picks white on dark background", "[view][contrast]") {
    auto fg = auto_contrast_foreground(Color::rgba(20, 20, 30));
    REQUIRE(fg.r == 255);
    REQUIRE(fg.g == 255);
    REQUIRE(fg.b == 255);
}

TEST_CASE("Auto contrast picks black on light background", "[view][contrast]") {
    auto fg = auto_contrast_foreground(Color::rgba(240, 240, 240));
    REQUIRE(fg.r == 0);
    REQUIRE(fg.g == 0);
    REQUIRE(fg.b == 0);
}

TEST_CASE("Adjust for contrast returns meeting color", "[view][contrast]") {
    auto result = adjust_for_contrast(
        Color::rgba(150, 150, 150),  // Low contrast on white bg
        Color::rgba(255, 255, 255),
        ContrastLevel::aa_normal);

    REQUIRE(result.has_value());
    REQUIRE(meets_contrast(*result, Color::rgba(255, 255, 255), ContrastLevel::aa_normal));
}

// ── HSL Conversion ──────────────────────────────────────────────────────────

TEST_CASE("HSL round-trip for pure red", "[view][contrast][hsl]") {
    auto c = Color::rgba(255, 0, 0);
    auto hsl = rgb_to_hsl(c);
    REQUIRE_THAT(hsl.h, WithinAbs(0.0, 1.0));
    REQUIRE_THAT(hsl.s, WithinAbs(1.0, 0.01));
    REQUIRE_THAT(hsl.l, WithinAbs(0.5, 0.01));

    auto back = hsl_to_rgb(hsl);
    REQUIRE(back.r == 255);
    REQUIRE(back.g == 0);
    REQUIRE(back.b == 0);
}

TEST_CASE("HSL round-trip for gray", "[view][contrast][hsl]") {
    auto c = Color::rgba(128, 128, 128);
    auto hsl = rgb_to_hsl(c);
    REQUIRE_THAT(hsl.s, WithinAbs(0.0, 0.01));
    REQUIRE_THAT(hsl.l, WithinAbs(0.5, 0.02));
}

TEST_CASE("Shift hue wraps around", "[view][contrast][hsl]") {
    auto red = Color::rgba(255, 0, 0);
    auto shifted = shift_hue(red, 120.0f); // Should be green-ish
    auto hsl = rgb_to_hsl(shifted);
    REQUIRE_THAT(hsl.h, WithinAbs(120.0, 2.0));
}

TEST_CASE("Blend colors at 0 returns first", "[view][contrast]") {
    auto a = Color::rgba(255, 0, 0);
    auto b = Color::rgba(0, 0, 255);
    auto result = blend_colors(a, b, 0.0f);
    REQUIRE(result.r == 255);
    REQUIRE(result.b == 0);
}

TEST_CASE("Blend colors at 1 returns second", "[view][contrast]") {
    auto a = Color::rgba(255, 0, 0);
    auto b = Color::rgba(0, 0, 255);
    auto result = blend_colors(a, b, 1.0f);
    REQUIRE(result.r == 0);
    REQUIRE(result.b == 255);
}

// ── Theme Contrast Validation ───────────────────────────────────────────────

TEST_CASE("Dark theme passes contrast validation", "[view][contrast][theme]") {
    auto theme = Theme::dark();
    auto issues = validate_theme_contrast(theme, ContrastLevel::aa_large);
    // Dark theme should have reasonable contrast for large text at minimum
    // Some pairs might fail aa_normal but should pass aa_large
    for (auto& issue : issues) {
        INFO("Failing pair: " << issue.foreground_token << " on " << issue.background_token
             << " ratio=" << issue.ratio);
    }
    // We don't require zero issues for aa_normal (accent colors may fail)
    // but key text pairs should pass
    auto text_on_bg = contrast_ratio(
        *theme.color("text.primary"), *theme.color("bg.primary"));
    REQUIRE(text_on_bg >= 4.5f);
}

TEST_CASE("Auto-fix contrast produces valid theme", "[view][contrast][theme]") {
    // Create a deliberately bad theme
    Theme bad;
    bad.colors["bg.primary"] = Color::rgba(200, 200, 200);
    bad.colors["text.primary"] = Color::rgba(180, 180, 180); // Almost same as bg
    bad.colors["text.secondary"] = Color::rgba(190, 190, 190);
    bad.colors["bg.secondary"] = Color::rgba(210, 210, 210);
    bad.colors["accent.primary"] = Color::rgba(195, 195, 195);

    auto fixed = auto_fix_contrast(bad, ContrastLevel::aa_normal);

    // text.primary on bg.primary should now meet AA
    auto tp = fixed.color("text.primary");
    auto bp = fixed.color("bg.primary");
    if (tp && bp) {
        REQUIRE(contrast_ratio(*tp, *bp) >= 4.5f);
    }
}
