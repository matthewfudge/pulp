#pragma once

#include <pulp/view/view.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace pulp::view {

enum class ScreenshotBackend {
    default_backend,
    coregraphics,
    skia,
};

// Render a view tree to a PNG image buffer (headless, no window needed).
//
// Platform support (#299):
//   - macOS/iOS: native CoreGraphics-backed capture.
//   - Windows/Linux/Android: no built-in backend. The host app
//     (or a future platform-specific module) registers a provider
//     via set_screenshot_provider(). Without one, returns empty
//     vector / false — explicitly "unsupported" rather than the
//     pre-#299 silent-empty-bytes bug.
//
// Callers can probe has_screenshot_provider() to distinguish
// "no backend installed" from "render failed".
std::vector<uint8_t> render_to_png(
    View& root,
    uint32_t width,
    uint32_t height,
    float scale = 2.0f,  // Retina scale factor
    ScreenshotBackend backend = ScreenshotBackend::default_backend
);

// Render a view tree to a PNG file
bool render_to_file(
    View& root,
    uint32_t width,
    uint32_t height,
    const std::string& output_path,
    float scale = 2.0f,
    ScreenshotBackend backend = ScreenshotBackend::default_backend
);

// Raw-RGBA sibling of render_to_png: render a view tree headlessly and hand
// back the decoded pixel buffer instead of PNG bytes — for callers that want
// to composite or upload the frame themselves (e.g. a foreign host that draws
// Pulp's output into its own surface) without paying a PNG encode + decode
// round-trip. The internal Skia raster path already holds these pixels before
// encoding, so this exposes them directly.
//
// Output: tightly packed RGBA8 (R,G,B,A byte order), premultiplied alpha,
// sRGB, top-to-bottom rows, stride == out_width * 4. The pixel dimensions are
// the logical width/height multiplied by `scale`, returned via out_width /
// out_height so the caller can size its buffer exactly. Returns an empty
// vector on failure (no Skia backend, surface alloc failed, read-back failed).
//
// Backend: forces the Skia raster path when available (the only backend that
// produces a stable, host-independent RGBA buffer). Without Skia this returns
// empty (CoreGraphics capture is PNG-only here).
std::vector<uint8_t> render_to_rgba(
    View& root,
    uint32_t width,
    uint32_t height,
    float scale,
    uint32_t* out_width,
    uint32_t* out_height
);

// ── Host-registered screenshot provider (#299) ──────────────────────────
//
// Non-Apple platforms don't have a built-in screenshot backend in
// core/view. A host app (or future platform module) installs a
// provider here — e.g., a Skia-backed raster renderer on Linux/
// Windows — and render_to_png/file will delegate to it. Apple
// platforms' native impls ignore the provider.

using ScreenshotProvider = std::function<std::vector<uint8_t>(
    View& root,
    uint32_t width,
    uint32_t height,
    float scale,
    ScreenshotBackend backend)>;

void set_screenshot_provider(ScreenshotProvider provider);
void clear_screenshot_provider();
bool has_screenshot_provider();

} // namespace pulp::view
