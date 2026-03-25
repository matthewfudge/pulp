#pragma once

#include <pulp/render/gpu_surface.hpp>
#include <pulp/canvas/canvas.hpp>
#include <memory>
#include <functional>

namespace pulp::render {

// Skia Graphite rendering surface
// Connects a GpuSurface (WebGPU device) to Skia's GPU rendering
// Provides an SkCanvas for each frame via the render callback
class SkiaSurface {
public:
    struct Config {
        uint32_t width = 800;
        uint32_t height = 600;
        float scale_factor = 1.0f;  // HiDPI
    };

    // Create a Skia Graphite surface backed by the given GPU device
    static std::unique_ptr<SkiaSurface> create(GpuSurface& gpu, const Config& config);

    virtual ~SkiaSurface() = default;

    // Begin a frame: returns a Canvas to draw into
    // The canvas is valid until end_frame() is called
    virtual canvas::Canvas* begin_frame() = 0;

    // End a frame: submits all drawing commands to the GPU
    virtual void end_frame() = 0;

    // Resize the surface
    virtual void resize(uint32_t width, uint32_t height, float scale = 1.0f) = 0;

    // Check if Skia rendering is available
    virtual bool is_available() const = 0;
};

} // namespace pulp::render
