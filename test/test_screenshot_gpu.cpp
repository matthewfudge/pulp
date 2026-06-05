// P1 — smart/unified view capture (capture_view) + offscreen-GPU path.
// Proves the "always-capturable" contract: a native overlay is refused with a
// reason, a blank/clear-only frame fails the content floor, and a real widget
// tree captures non-blank.
#include <catch2/catch_test_macros.hpp>
#include <pulp/view/frame_clock.hpp>
#include <pulp/view/screenshot.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/canvas/canvas.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace pulp::view;

TEST_CASE("capture_view refuses a native-overlay subtree with a reason",
          "[view][screenshot][gpu]") {
    View root;
    auto pane = std::make_unique<View>();
    pane->set_contains_native_overlay(true);  // e.g. a WebView pane
    root.add_child(std::move(pane));

    const CaptureResult r = capture_view(root, 200, 120, 1.0f);
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.reason.find("overlay") != std::string::npos);  // honest, not a blank
}

TEST_CASE("capture_view rejects a blank / clear-only frame (content floor)",
          "[view][screenshot][gpu]") {
    View root;  // nothing painted but the background fill → 1 unique color
    const CaptureResult r = capture_view(root, 200, 120, 1.0f);
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.reason.find("content floor") != std::string::npos);
}

// A reliably content-rich tree: a diagonal gradient background (hundreds of
// interpolated colors at full, opaque coverage — clears the content floor
// regardless of child flex layout) plus a Label. Exercises paint_all → the
// capture canvas → the content floor with real painted content.
static std::unique_ptr<View> make_capture_tree() {
    auto root = std::make_unique<View>();
    root->set_theme(Theme::dark());
    root->set_background_gradient_linear(
        0.0f, 0.0f, 1.0f, 1.0f,
        {pulp::canvas::Color::rgba8(28, 31, 46), pulp::canvas::Color::rgba8(96, 150, 240),
         pulp::canvas::Color::rgba8(168, 140, 250)},
        {0.0f, 0.55f, 1.0f});
    auto label = std::make_unique<Label>("Promptable Accompanist");
    label->set_bounds({16.0f, 80.0f, 480.0f, 32.0f});
    root->add_child(std::move(label));
    return root;
}

TEST_CASE("capture_view passes a non-blank widget tree (raster)", "[view][screenshot][gpu]") {
    auto root = make_capture_tree();
    const CaptureResult r = capture_view(*root, 520, 200, 1.0f);
    INFO("capture_view reason: " << r.reason << " (backend " << static_cast<int>(r.used) << ")");
    REQUIRE(r.ok);
    REQUIRE_FALSE(r.png.empty());
    REQUIRE(r.png.size() > 8);
    REQUIRE(r.png[1] == 'P');  // PNG magic "\x89PNG"
}

TEST_CASE("capture_view routes a requires_gpu_host tree to the GPU backend",
          "[view][screenshot][gpu]") {
    if (!has_gpu_capture()) {
        SUCCEED("GPU capture not compiled in (no pulp-render)");
        return;
    }
    auto root = make_capture_tree();
    root->set_requires_gpu_host(true);  // forces the GPU (HeadlessSurface) path

    const CaptureResult r = capture_view(*root, 520, 200, 1.0f);
    INFO("gpu capture_view reason: " << r.reason);
    // The GPU surface may be absent on a headless CI adapter; if it produced a
    // frame it must pass the content floor (proving the GPU path paints content).
    if (!r.png.empty()) {
        REQUIRE(r.used == ScreenshotBackend::gpu);
        REQUIRE(r.ok);
        REQUIRE(r.png[1] == 'P');
    }
}

// Hidden demo (run with the [.demo] tag): writes a real GPU-captured PNG to disk
// so the capture pipeline can be eyeballed. Not part of the default suite.
TEST_CASE("demo: write a GPU-captured PNG to disk", "[.][demo]") {
    auto root = make_capture_tree();
    root->set_requires_gpu_host(true);
    const CaptureResult r = capture_view(*root, 520, 200, 2.0f);
    INFO("reason: " << r.reason);
    REQUIRE(r.ok);
    const char* out = std::getenv("P1_DEMO_PNG");
    std::ofstream f(out ? out : "/tmp/p1-gpu-capture.png", std::ios::binary);
    f.write(reinterpret_cast<const char*>(r.png.data()),
            static_cast<std::streamsize>(r.png.size()));
}
