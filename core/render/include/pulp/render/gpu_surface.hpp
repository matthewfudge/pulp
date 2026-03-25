#pragma once

#include <cstdint>
#include <memory>
#include <functional>

namespace pulp::render {

// Abstract GPU surface — represents a renderable target
// Concrete implementations use Dawn/WebGPU backend
class GpuSurface {
public:
    struct Config {
        uint32_t width = 800;
        uint32_t height = 600;
        bool vsync = true;
    };

    virtual ~GpuSurface() = default;

    // Initialize the GPU device and surface
    virtual bool initialize(const Config& config) = 0;

    // Resize the surface
    virtual void resize(uint32_t width, uint32_t height) = 0;

    // Begin a frame (acquire next texture)
    virtual bool begin_frame() = 0;

    // End a frame (present)
    virtual void end_frame() = 0;

    // Query capabilities
    virtual bool is_initialized() const = 0;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;

    // Factory: create a Dawn-backed surface
    static std::unique_ptr<GpuSurface> create_dawn();
};

} // namespace pulp::render
