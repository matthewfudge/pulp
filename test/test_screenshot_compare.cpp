#include <catch2/catch_test_macros.hpp>
#include <pulp/view/screenshot_compare.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace pulp::view;

namespace {

std::vector<uint8_t> render_label_png(const std::string& text,
                                      const Theme& theme,
                                      int width = 100,
                                      int height = 50) {
    View root;
    root.set_theme(theme);
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 8;

    auto label = std::make_unique<Label>(text);
    label->flex().preferred_height = 24;
    root.add_child(std::move(label));

    return render_to_png(root, width, height, 1.0f);
}

std::filesystem::path temp_png_path(const char* stem) {
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           (std::string(stem) + "-" + std::to_string(tick) + ".png");
}

void write_png_file(const std::filesystem::path& path, const std::vector<uint8_t>& png) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(reinterpret_cast<const char*>(png.data()),
              static_cast<std::streamsize>(png.size()));
}

}  // namespace

TEST_CASE("CompareResult passes with high similarity", "[view][compare]") {
    CompareResult r;
    r.valid = true;
    r.similarity = 0.95f;
    REQUIRE(r.passes(0.85f));
    REQUIRE_FALSE(r.passes(0.99f));
}

TEST_CASE("CompareResult fails when invalid", "[view][compare]") {
    CompareResult r;
    r.valid = false;
    r.similarity = 1.0f;
    REQUIRE_FALSE(r.passes());
}

TEST_CASE("compare_screenshots identical images", "[view][compare]") {
    // Render the same view twice — should be identical
    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 10;

    auto label = std::make_unique<Label>("Test");
    label->flex().preferred_height = 20;
    root.add_child(std::move(label));

    auto png1 = render_to_png(root, 100, 50, 1.0f);
    auto png2 = render_to_png(root, 100, 50, 1.0f);

    REQUIRE_FALSE(png1.empty());
    REQUIRE_FALSE(png2.empty());

    auto result = compare_screenshots(png1, png2);
    REQUIRE(result.valid);
    REQUIRE(result.similarity >= 0.99f);
    REQUIRE(result.diff_pixels == 0);
    REQUIRE(result.passes());
}

TEST_CASE("compare_screenshots different images", "[view][compare]") {
    // Render two visually distinct views
    View root1;
    root1.set_theme(Theme::dark());
    root1.flex().direction = FlexDirection::column;
    root1.flex().padding = 8;
    auto l1 = std::make_unique<Label>("Dark theme text");
    l1->flex().preferred_height = 30;
    root1.add_child(std::move(l1));

    View root2;
    root2.set_theme(Theme::light());
    root2.flex().direction = FlexDirection::column;
    root2.flex().padding = 8;
    auto l2 = std::make_unique<Label>("Light theme text");
    l2->flex().preferred_height = 30;
    root2.add_child(std::move(l2));

    auto png1 = render_to_png(root1, 100, 50, 1.0f);
    auto png2 = render_to_png(root2, 100, 50, 1.0f);

    REQUIRE_FALSE(png1.empty());
    REQUIRE_FALSE(png2.empty());

    auto result = compare_screenshots(png1, png2, 16);  // Tighter tolerance
    REQUIRE(result.valid);
    REQUIRE(result.similarity < 0.95f);
    REQUIRE(result.diff_pixels > 0);
}

TEST_CASE("compare_screenshots handles empty input", "[view][compare]") {
    std::vector<uint8_t> empty;
    std::vector<uint8_t> valid = {1, 2, 3};  // Not a real PNG

    auto r1 = compare_screenshots(empty, valid);
    REQUIRE_FALSE(r1.valid);

    auto r2 = compare_screenshots(valid, empty);
    REQUIRE_FALSE(r2.valid);
}

TEST_CASE("compare_screenshots reports rendered decode failure",
          "[view][compare][coverage][phase3]") {
    auto png = render_label_png("Reference", Theme::dark());
    REQUIRE_FALSE(png.empty());

    auto result = compare_screenshots(png, {1, 2, 3});

    REQUIRE_FALSE(result.valid);
    REQUIRE(result.error.find("rendered") != std::string::npos);
}

TEST_CASE("compare_screenshots penalizes size mismatch",
          "[view][compare][coverage][phase3]") {
    auto small = render_label_png("Same", Theme::dark(), 40, 20);
    auto large = render_label_png("Same", Theme::dark(), 80, 40);
    REQUIRE_FALSE(small.empty());
    REQUIRE_FALSE(large.empty());

    auto result = compare_screenshots(small, large, 255);

    REQUIRE(result.valid);
    REQUIRE(result.total_pixels == 800);
    REQUIRE(result.similarity < 1.0f);
}

TEST_CASE("compare_screenshot_files covers file IO success and failures",
          "[view][compare][coverage][phase3]") {
    auto ref_png = render_label_png("File A", Theme::dark());
    auto ren_png = render_label_png("File A", Theme::dark());
    REQUIRE_FALSE(ref_png.empty());
    REQUIRE_FALSE(ren_png.empty());

    const auto ref_path = temp_png_path("pulp-ref");
    const auto ren_path = temp_png_path("pulp-ren");
    write_png_file(ref_path, ref_png);
    write_png_file(ren_path, ren_png);

    auto missing_ref = compare_screenshot_files(ref_path.string() + ".missing", ren_path.string());
    REQUIRE_FALSE(missing_ref.valid);
    REQUIRE(missing_ref.error.find("reference") != std::string::npos);

    auto missing_ren = compare_screenshot_files(ref_path.string(), ren_path.string() + ".missing");
    REQUIRE_FALSE(missing_ren.valid);
    REQUIRE(missing_ren.error.find("rendered") != std::string::npos);

    auto ok = compare_screenshot_files(ref_path.string(), ren_path.string());
    REQUIRE(ok.valid);
    REQUIRE(ok.diff_pixels == 0);

    std::filesystem::remove(ref_path);
    std::filesystem::remove(ren_path);
}

TEST_CASE("generate_diff_image produces output", "[view][compare]") {
    View root1;
    root1.set_theme(Theme::dark());
    root1.flex().direction = FlexDirection::column;

    View root2;
    root2.set_theme(Theme::light());
    root2.flex().direction = FlexDirection::column;

    auto png1 = render_to_png(root1, 80, 40, 1.0f);
    auto png2 = render_to_png(root2, 80, 40, 1.0f);

    auto diff = generate_diff_image(png1, png2);
    REQUIRE_FALSE(diff.empty());
    // Diff should be a valid PNG (starts with PNG signature)
    REQUIRE(diff.size() > 8);
    REQUIRE(diff[0] == 0x89);
    REQUIRE(diff[1] == 'P');
    REQUIRE(diff[2] == 'N');
    REQUIRE(diff[3] == 'G');
}

TEST_CASE("generate_diff_image handles invalid changed and size mismatch inputs",
          "[view][compare][coverage][phase3]") {
    auto dark = render_label_png("Diff", Theme::dark(), 80, 40);
    auto light = render_label_png("Diff", Theme::light(), 80, 40);
    auto small = render_label_png("Diff", Theme::dark(), 40, 20);
    REQUIRE_FALSE(dark.empty());
    REQUIRE_FALSE(light.empty());
    REQUIRE_FALSE(small.empty());

    REQUIRE(generate_diff_image({}, light).empty());

    auto changed = generate_diff_image(dark, light, 0);
    REQUIRE_FALSE(changed.empty());

    auto mismatch = generate_diff_image(small, dark, 0);
    REQUIRE_FALSE(mismatch.empty());
}

TEST_CASE("crop_png extracts a requested region", "[view][compare]") {
    View root;
    root.set_theme(Theme::dark());
    root.flex().direction = FlexDirection::column;
    root.flex().padding = 8;

    auto label = std::make_unique<Label>("Crop me");
    label->flex().preferred_height = 24;
    root.add_child(std::move(label));

    auto png = render_to_png(root, 120, 60, 1.0f);
    REQUIRE_FALSE(png.empty());

    auto crop = crop_png(png, 10, 10, 30, 20);
    REQUIRE_FALSE(crop.empty());

    auto result = compare_screenshots(crop, crop);
    REQUIRE(result.valid);
    REQUIRE(result.total_pixels == 600);
    REQUIRE(result.diff_pixels == 0);
}

TEST_CASE("crop_png clamps regions to image bounds", "[view][compare]") {
    View root;
    root.set_theme(Theme::light());
    root.flex().direction = FlexDirection::column;

    auto png = render_to_png(root, 80, 40, 1.0f);
    REQUIRE_FALSE(png.empty());

    auto crop = crop_png(png, 70, 30, 40, 40);
    REQUIRE_FALSE(crop.empty());

    auto result = compare_screenshots(crop, crop);
    REQUIRE(result.valid);
    REQUIRE(result.total_pixels == 100);
}

TEST_CASE("crop_png rejects invalid and non-intersecting regions",
          "[view][compare][coverage][phase3]") {
    auto png = render_label_png("Crop guards", Theme::dark(), 80, 40);
    REQUIRE_FALSE(png.empty());

    REQUIRE(crop_png({}, 0, 0, 10, 10).empty());
    REQUIRE(crop_png(png, 0, 0, 0, 10).empty());
    REQUIRE(crop_png(png, 100, 0, 10, 10).empty());
}

TEST_CASE("diff_bounds locates changed pixels", "[view][compare]") {
    View root1;
    root1.set_theme(Theme::dark());
    root1.flex().direction = FlexDirection::column;
    root1.flex().padding = 8;
    auto label1 = std::make_unique<Label>("Changed region");
    label1->flex().preferred_height = 24;
    root1.add_child(std::move(label1));

    View root2;
    root2.set_theme(Theme::light());
    root2.flex().direction = FlexDirection::column;
    root2.flex().padding = 8;
    auto label2 = std::make_unique<Label>("Changed region");
    label2->flex().preferred_height = 24;
    root2.add_child(std::move(label2));

    auto png1 = render_to_png(root1, 80, 50, 1.0f);
    auto png2 = render_to_png(root2, 80, 50, 1.0f);
    REQUIRE_FALSE(png1.empty());
    REQUIRE_FALSE(png2.empty());

    auto bounds = diff_bounds(png1, png2, 8);
    REQUIRE(bounds.valid);
    REQUIRE(bounds.diff_pixels > 0);
    REQUIRE(bounds.width > 0);
    REQUIRE(bounds.height > 0);
}

TEST_CASE("diff_bounds is invalid for identical or undecodable images",
          "[view][compare][coverage][phase3]") {
    auto png = render_label_png("Same", Theme::dark(), 80, 40);
    REQUIRE_FALSE(png.empty());

    auto same = diff_bounds(png, png);
    REQUIRE_FALSE(same.valid);
    REQUIRE(same.diff_pixels == 0);

    auto invalid = diff_bounds({}, png);
    REQUIRE_FALSE(invalid.valid);
}
