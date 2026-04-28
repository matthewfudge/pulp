// Automated test for CanvasWidget (JS-driven custom drawing)
#include <catch2/catch_test_macros.hpp>
#include <pulp/view/canvas_widget.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/canvas/canvas.hpp>

#ifdef PULP_HAS_SKIA
#include <pulp/canvas/skia_canvas.hpp>
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#endif

using namespace pulp::view;
using pulp::canvas::DrawCommand;
using pulp::canvas::RecordingCanvas;

TEST_CASE("CanvasWidget: add and clear commands", "[canvas_widget]") {
    CanvasWidget cw;
    REQUIRE(cw.command_count() == 0);

    CanvasDrawCmd cmd;
    cmd.type = CanvasDrawCmd::Type::fill_rect;
    cmd.x = 0; cmd.y = 0; cmd.w = 100; cmd.h = 50;
    cmd.color = {255, 0, 0, 255};
    cw.add_command(cmd);
    REQUIRE(cw.command_count() == 1);

    cw.clear_commands();
    REQUIRE(cw.command_count() == 0);
}

TEST_CASE("CanvasWidget: paint replays commands", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 100});

    // Add a filled rect
    CanvasDrawCmd rect;
    rect.type = CanvasDrawCmd::Type::fill_rect;
    rect.x = 10; rect.y = 10; rect.w = 80; rect.h = 40;
    rect.color = {255, 0, 0, 255};
    cw.add_command(rect);

    // Add a circle
    CanvasDrawCmd circle;
    circle.type = CanvasDrawCmd::Type::fill_circle;
    circle.x = 50; circle.y = 50; circle.extra = 20; // radius
    circle.color = {0, 255, 0, 255};
    cw.add_command(circle);

    // Add a line
    CanvasDrawCmd line;
    line.type = CanvasDrawCmd::Type::stroke_line;
    line.x = 0; line.y = 0; line.w = 100; line.h = 100; // endpoint
    line.extra = 2.0f; // width
    line.color = {0, 0, 255, 255};
    cw.add_command(line);

    cw.paint(rc);
    // Should have produced fill_rect, fill_circle, stroke_line commands
    REQUIRE(rc.commands().size() >= 3);
}

TEST_CASE("CanvasWidget: fill_text command", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 50});

    CanvasDrawCmd text;
    text.type = CanvasDrawCmd::Type::fill_text;
    text.x = 10; text.y = 20;
    text.extra = 14.0f; // font size
    text.color = {255, 255, 255, 255};
    text.text = "Hello World";
    cw.add_command(text);

    cw.paint(rc);
    REQUIRE(rc.commands().size() > 0);
}

TEST_CASE("CanvasWidget: clear command fills background", "[canvas_widget]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 100, 100});

    CanvasDrawCmd clear;
    clear.type = CanvasDrawCmd::Type::clear;
    clear.color = {30, 30, 46, 255};
    cw.add_command(clear);

    cw.paint(rc);
    REQUIRE(rc.commands().size() > 0);
}

// Issue-897 P1 follow-up: CanvasWidget::paint() must snapshot the inbound
// device matrix at entry so JS-driven setTransform() composes onto the
// parent View transform instead of overwriting it. Without this baseline
// the SkiaCanvas backend's setTransform would wipe the parent translation
// applied by View::paint_all.
TEST_CASE("CanvasWidget::paint captures paint baseline transform at entry",
          "[canvas_widget][issue-897]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 100, 100});

    REQUIRE(rc.baseline_capture_count() == 0);
    cw.paint(rc);
    REQUIRE(rc.baseline_capture_count() == 1);

    cw.paint(rc);
    REQUIRE(rc.baseline_capture_count() == 2);
}

TEST_CASE("CanvasWidget::paint baseline capture is idempotent and ordered",
          "[canvas_widget][issue-897]") {
    // The baseline must be snapshot before any draw work — otherwise an
    // intervening transform command would corrupt it.
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 100, 100});

    CanvasDrawCmd r;
    r.type = CanvasDrawCmd::Type::fill_rect;
    r.x = 0; r.y = 0; r.w = 50; r.h = 50;
    r.color = {255, 0, 0, 255};
    cw.add_command(r);

    REQUIRE(rc.commands().empty());
    REQUIRE(rc.baseline_capture_count() == 0);
    cw.paint(rc);
    REQUIRE(rc.baseline_capture_count() == 1);
    REQUIRE_FALSE(rc.commands().empty());
}

// ── pulp #929 — Canvas widget transparent default + clearRect semantics ──

// Empty-command paint must not introduce a full-bounds fill that would
// hide the parent's painted surface. Asserting against RecordingCanvas
// gives us a backend-independent regression guard, even on platforms
// where Skia raster isn't available in the test build.
TEST_CASE("CanvasWidget::paint with no JS draw commands does not pre-fill bounds",
          "[canvas_widget][issue-929]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 100});

    cw.paint(rc);

    // Allowed bookkeeping commands: capture_paint_baseline_transform()
    // does not add to commands_, so the recorded list should stay empty.
    // What MUST NOT happen: a fill_rect covering the widget bounds. We
    // walk the full command list to be defensive against future state-
    // tracking commands sneaking in (set_blend_mode, save, etc.).
    for (const auto& cmd : rc.commands()) {
        if (cmd.type == DrawCommand::Type::fill_rect) {
            const bool covers_bounds = cmd.f[0] <= 0.0f && cmd.f[1] <= 0.0f &&
                                       cmd.f[2] >= 200.0f && cmd.f[3] >= 100.0f;
            INFO("CanvasWidget::paint emitted a fill_rect that covers the full "
                 "widget bounds — this would mask the parent's paint surface "
                 "(pulp #929).");
            REQUIRE_FALSE(covers_bounds);
        }
    }
}

// JS-issued clearRect should produce a single clear_rect command, not a
// SrcOver fill that is a no-op. RecordingCanvas captures the dedicated
// DrawCommand::Type::clear_rect introduced in pulp #929 so this is a
// straightforward identity assertion.
TEST_CASE("CanvasWidget::paint translates clearRect into a real clear_rect command",
          "[canvas_widget][issue-929]") {
    RecordingCanvas rc;
    CanvasWidget cw;
    cw.set_bounds({0, 0, 200, 100});

    CanvasDrawCmd c;
    c.type = CanvasDrawCmd::Type::clear_rect;
    c.x = 10; c.y = 20; c.w = 80; c.h = 40;
    cw.add_command(c);

    cw.paint(rc);

    // Exactly one clear_rect command, matching the JS-issued bounds.
    size_t clear_count = 0;
    for (const auto& cmd : rc.commands()) {
        if (cmd.type == DrawCommand::Type::clear_rect) {
            ++clear_count;
            REQUIRE(cmd.f[0] == 10.0f);
            REQUIRE(cmd.f[1] == 20.0f);
            REQUIRE(cmd.f[2] == 80.0f);
            REQUIRE(cmd.f[3] == 40.0f);
        }
    }
    REQUIRE(clear_count == 1);

    // And no SrcOver fill_rect snuck in — the previous implementation was
    // save / clip / setFillColor(0,0,0,0) / fill_rect, which is a visual
    // no-op (#929 root cause).
    for (const auto& cmd : rc.commands()) {
        REQUIRE(cmd.type != DrawCommand::Type::fill_rect);
    }
}

#ifdef PULP_HAS_SKIA

namespace {
struct Pixel {
    uint8_t r, g, b, a;
};

// Sample a single RGBA8 pixel (premul) from a Skia raster surface so
// tests can assert on the actual texels CanvasWidget produced.
Pixel sample_pixel(SkSurface* surface, int x, int y) {
    SkPixmap pix;
    REQUIRE(surface->peekPixels(&pix));
    auto* row = static_cast<const uint8_t*>(pix.addr(0, y));
    return {row[4 * x + 0], row[4 * x + 1], row[4 * x + 2], row[4 * x + 3]};
}
}  // namespace

TEST_CASE("CanvasWidget paints transparent by default on a Skia raster surface",
          "[canvas_widget][skia][issue-929]") {
    // Premultiplied RGBA8 raster surface — Skia initializes the backing
    // store to all-zero (fully transparent) on first access. If the
    // CanvasWidget were pre-filling its bounds with white, every sampled
    // pixel would become (255, 255, 255, 255).
    SkImageInfo info = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType,
                                         SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::Raster(info);
    REQUIRE(surface != nullptr);
    auto* sk_canvas = surface->getCanvas();
    REQUIRE(sk_canvas != nullptr);

    pulp::canvas::SkiaCanvas canvas(sk_canvas);
    CanvasWidget cw;
    cw.set_bounds({0, 0, 64, 64});
    cw.paint(canvas);

    // Spot-check four corners + center — all pixels stay fully transparent.
    for (auto [x, y] : std::initializer_list<std::pair<int, int>>{
             {0, 0}, {63, 0}, {0, 63}, {63, 63}, {32, 32}}) {
        auto px = sample_pixel(surface.get(), x, y);
        INFO("Pixel (" << x << "," << y << ") rgba=("
             << int(px.r) << "," << int(px.g) << ","
             << int(px.b) << "," << int(px.a) << ")");
        REQUIRE(px.a == 0);
        REQUIRE(px.r == 0);
        REQUIRE(px.g == 0);
        REQUIRE(px.b == 0);
    }
}

TEST_CASE("CanvasWidget partial fillRect leaves untouched pixels transparent",
          "[canvas_widget][skia][issue-929]") {
    // 64x64 surface; widget fills a 32x64 left half with opaque red. The
    // right half must stay at the surface's initial transparent black.
    SkImageInfo info = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType,
                                         SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::Raster(info);
    REQUIRE(surface != nullptr);
    pulp::canvas::SkiaCanvas canvas(surface->getCanvas());

    CanvasWidget cw;
    cw.set_bounds({0, 0, 64, 64});
    CanvasDrawCmd r;
    r.type = CanvasDrawCmd::Type::fill_rect;
    r.x = 0; r.y = 0; r.w = 32; r.h = 64;
    r.color = {255, 0, 0, 255};
    cw.add_command(r);
    cw.paint(canvas);

    // Inside the fill: opaque red.
    auto inside = sample_pixel(surface.get(), 16, 32);
    REQUIRE(inside.a == 255);
    REQUIRE(inside.r == 255);
    // Outside the fill: still transparent.
    auto outside = sample_pixel(surface.get(), 48, 32);
    REQUIRE(outside.a == 0);
    REQUIRE(outside.r == 0);
}

TEST_CASE("CanvasWidget clearRect actually clears Skia pixels",
          "[canvas_widget][skia][issue-929]") {
    // Pre-fill the surface with opaque white (simulates a parent's paint
    // pass having already drawn the underlying area). A subsequent
    // clearRect issued through CanvasWidget must replace those pixels
    // with transparent black, not SrcOver-blend a transparent fill.
    SkImageInfo info = SkImageInfo::Make(32, 32, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType,
                                         SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::Raster(info);
    REQUIRE(surface != nullptr);
    auto* sk_canvas = surface->getCanvas();
    sk_canvas->clear(SkColorSetARGB(255, 255, 255, 255));

    pulp::canvas::SkiaCanvas canvas(sk_canvas);
    CanvasWidget cw;
    cw.set_bounds({0, 0, 32, 32});
    CanvasDrawCmd c;
    c.type = CanvasDrawCmd::Type::clear_rect;
    c.x = 0; c.y = 0; c.w = 32; c.h = 32;
    cw.add_command(c);
    cw.paint(canvas);

    auto px = sample_pixel(surface.get(), 16, 16);
    INFO("Cleared pixel rgba=("
         << int(px.r) << "," << int(px.g) << ","
         << int(px.b) << "," << int(px.a) << ")");
    REQUIRE(px.a == 0);
    REQUIRE(px.r == 0);
    REQUIRE(px.g == 0);
    REQUIRE(px.b == 0);
}

#endif  // PULP_HAS_SKIA
