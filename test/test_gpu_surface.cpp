#include <catch2/catch_test_macros.hpp>
#include <pulp/render/gpu_surface.hpp>

using namespace pulp::render;

TEST_CASE("GpuSurface creation", "[render][gpu]") {
    auto surface = GpuSurface::create_dawn();

#ifdef PULP_HAS_WEBGPU
    REQUIRE(surface != nullptr);

    SECTION("Initialize") {
        GpuSurface::Config config;
        config.width = 800;
        config.height = 600;

        bool ok = surface->initialize(config);
        // This may fail in CI without GPU, but shouldn't crash
        if (ok) {
            REQUIRE(surface->is_initialized());
            REQUIRE(surface->width() == 800);
            REQUIRE(surface->height() == 600);

            // Frame cycle
            REQUIRE(surface->begin_frame());
            surface->end_frame();

            // Resize
            surface->resize(1024, 768);
            REQUIRE(surface->width() == 1024);
            REQUIRE(surface->height() == 768);
        } else {
            // No GPU available (headless CI, etc.)
            REQUIRE_FALSE(surface->is_initialized());
        }
    }
#else
    REQUIRE(surface == nullptr);
#endif
}
