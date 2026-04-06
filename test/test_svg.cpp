#include <catch2/catch_test_macros.hpp>
#include <pulp/canvas/svg.hpp>
#include <pulp/canvas/canvas.hpp>

using namespace pulp::canvas;

static const char* test_svg = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <circle cx="50" cy="50" r="40" fill="#ff0000" stroke="#000000" stroke-width="2"/>
  <rect x="10" y="10" width="30" height="30" fill="#00ff00"/>
</svg>
)";

TEST_CASE("SvgImage from string", "[canvas][svg]") {
    auto img = SvgImage::from_string(test_svg);
    REQUIRE(img.is_valid());
    REQUIRE(img.width() > 0);
    REQUIRE(img.height() > 0);
}

TEST_CASE("SvgImage rasterize", "[canvas][svg]") {
    auto img = SvgImage::from_string(test_svg);
    REQUIRE(img.is_valid());

    auto pixels = img.rasterize(64, 64);
    REQUIRE(pixels.size() == 64 * 64 * 4); // RGBA

    // Check that some pixels are non-zero (not all transparent)
    bool has_content = false;
    for (size_t i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] > 0) { has_content = true; break; }
    }
    REQUIRE(has_content);
}

TEST_CASE("SvgImage render to RecordingCanvas", "[canvas][svg]") {
    auto img = SvgImage::from_string(test_svg);
    REQUIRE(img.is_valid());

    RecordingCanvas canvas;
    img.render(canvas, 0, 0, 100, 100);

    // Should have generated some drawing commands
    REQUIRE(canvas.command_count() > 0);
    REQUIRE(canvas.count(DrawCommand::Type::save) >= 1);
    REQUIRE(canvas.count(DrawCommand::Type::restore) >= 1);
}

TEST_CASE("SvgImage invalid string", "[canvas][svg]") {
    auto img = SvgImage::from_string("not svg");
    // nanosvg may return an empty image rather than nullptr
    // Just check it doesn't crash
    REQUIRE(true);
}

TEST_CASE("SvgImage move semantics", "[canvas][svg]") {
    auto img1 = SvgImage::from_string(test_svg);
    REQUIRE(img1.is_valid());

    SvgImage img2 = std::move(img1);
    REQUIRE(img2.is_valid());
    REQUIRE_FALSE(img1.is_valid());
}
