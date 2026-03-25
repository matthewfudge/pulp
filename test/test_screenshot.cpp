#include <catch2/catch_test_macros.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <filesystem>

using namespace pulp::view;

TEST_CASE("Screenshot renders to PNG buffer", "[view][screenshot]") {
    View root;
    root.set_theme(Theme::dark());

    auto knob = std::make_unique<Knob>();
    knob->set_bounds({10, 10, 48, 48});
    knob->set_value(0.5f);
    knob->set_label("Gain");
    root.add_child(std::move(knob));

    auto label = std::make_unique<Label>("Test Plugin");
    label->set_bounds({70, 20, 120, 20});
    root.add_child(std::move(label));

    auto png = render_to_png(root, 200, 80, 1.0f);

#ifdef __APPLE__
    REQUIRE_FALSE(png.empty());
    // PNG magic bytes: 0x89 P N G
    REQUIRE(png.size() > 8);
    REQUIRE(png[0] == 0x89);
    REQUIRE(png[1] == 'P');
    REQUIRE(png[2] == 'N');
    REQUIRE(png[3] == 'G');
#else
    // Non-Apple platforms return empty (no CoreGraphics)
    REQUIRE(png.empty());
#endif
}

TEST_CASE("Screenshot renders to file", "[view][screenshot]") {
    View root;
    root.set_theme(Theme::dark());

    auto fader = std::make_unique<Fader>();
    fader->set_bounds({10, 10, 24, 100});
    fader->set_value(0.7f);
    root.add_child(std::move(fader));

    auto output_path = std::filesystem::temp_directory_path() / "pulp_screenshot_test.png";

    bool ok = render_to_file(root, 60, 120, output_path.string(), 1.0f);

#ifdef __APPLE__
    REQUIRE(ok);
    REQUIRE(std::filesystem::exists(output_path));
    REQUIRE(std::filesystem::file_size(output_path) > 100); // Not empty

    // Clean up
    std::filesystem::remove(output_path);
#else
    REQUIRE_FALSE(ok);
#endif
}
