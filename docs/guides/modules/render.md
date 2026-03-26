# Render Module

The render module manages GPU surfaces and the Skia Graphite rendering context. It connects Dawn (WebGPU) to Skia for hardware-accelerated 2D rendering.

**Status**: experimental — offscreen Graphite rendering works; on-screen presentation path exists on Apple but is not yet the default view host rendering mode
**Dependencies**: runtime, canvas
**Headers**: `pulp/render/gpu_surface.hpp`, `pulp/render/skia_surface.hpp`

## Architecture

The rendering stack has three layers with clear ownership:

```
Platform (view host)
  → owns native view/layer (NSView + CAMetalLayer on macOS, UIView + CAMetalLayer on iOS)
  → provides the native layer handle to the render subsystem
  → handles size, scale, safe-area, and visibility events

Render (GpuSurface + SkiaSurface)
  → GpuSurface owns the Dawn/WebGPU instance, adapter, device, queue, and surface
  → GpuSurface manages the presentable surface (swapchain) when a native layer is provided
  → SkiaSurface creates a Skia Graphite context and records drawing commands
  → Frame lifecycle: begin_frame (acquire) → draw → end_frame (submit + present)

View (paint traversal)
  → performs layout, input, invalidation, and paint traversal
  → draws into a Canvas (SkiaCanvas for GPU, CoreGraphicsCanvas for CPU fallback)
  → does not own native Metal objects or GPU devices
```

## GpuSurface

Abstract GPU surface representing a renderable target. The Dawn implementation handles device creation, native surface attachment, and frame lifecycle.

```cpp
#include <pulp/render/gpu_surface.hpp>

auto gpu = GpuSurface::create_dawn();

// For on-screen rendering, pass a native layer handle:
GpuSurface::Config config{};
config.width = 800;
config.height = 600;
config.vsync = true;
config.native_layer = (__bridge void*)metalLayer;  // CAMetalLayer*

gpu->initialize(config);

// For offscreen-only mode, leave native_layer as nullptr:
// config.native_layer = nullptr;  // default
```

### Frame Lifecycle

1. `begin_frame()` — acquires the next swapchain texture (returns false if surface is lost/minimized)
2. Draw commands are recorded via SkiaSurface / Skia Graphite Recorder
3. `end_frame()` — submits GPU work and presents to the native surface

When `native_layer` is null, `begin_frame()` always succeeds and `end_frame()` only processes events (no presentation).

### has_surface()

Returns true if the GpuSurface has a presentable native surface attached. Use this to distinguish on-screen rendering from offscreen-only mode.

## SkiaSurface

Creates a Skia Graphite rendering context for 2D drawing:

```cpp
#include <pulp/render/skia_surface.hpp>

SkiaSurface::Config skia_config{};
skia_config.width = 800;
skia_config.height = 600;
skia_config.scale_factor = 2.0f;  // Retina

auto skia = SkiaSurface::create(*gpu, skia_config);

// Each frame:
if (gpu->begin_frame()) {
    if (auto* canvas = skia->begin_frame()) {
        root_view.paint_all(*canvas);
    }
    skia->end_frame();   // submits Graphite recording
    gpu->end_frame();     // presents to native surface
}
```

SkiaSurface does not own the presentation path. GpuSurface handles surface acquisition and presentation; SkiaSurface only records and submits drawing commands.

## Apple Render Surfaces

### macOS

The macOS render surface uses an `NSView` backed by `CAMetalLayer`. The view:
- Sets `wantsLayer = YES` and assigns a `CAMetalLayer` as its layer
- Configures the Metal device, BGRA8 pixel format, and `contentsScale` for Retina
- Handles resize via `setFrameSize:` and scale changes via `viewDidChangeBackingProperties`
- Provides the `CAMetalLayer` handle to `GpuSurface::Config::native_layer`

### iOS

The iOS render surface uses a `UIView` with `+layerClass` returning `CAMetalLayer`. It handles:
- Safe area insets for notch/home-indicator regions
- Display scale (`UIScreen.mainScreen.scale` or `traitCollection.displayScale`)
- Orientation changes and geometry updates

### Current State (honest assessment)

| Component | State |
|-----------|-------|
| Dawn device bootstrap | Works on macOS and iOS |
| GpuSurface presentable surface (macOS) | Infrastructure exists, not yet default render path |
| GpuSurface presentable surface (iOS) | Infrastructure exists, not yet wired to view hosts |
| Skia Graphite offscreen rendering | Works (tested) |
| Skia Graphite → on-screen present | Requires GpuSurface with native_layer; not yet default |
| CoreGraphics fallback (macOS) | Works and is the current default render path |
| View host GPU integration | Planned — view hosts currently use CoreGraphics only |

## GPU Backends

Dawn supports multiple backends, selected automatically per platform:

| Platform | Backend | Status |
|----------|---------|--------|
| macOS | Metal | experimental — device and surface infrastructure exist |
| iOS | Metal | experimental — CAMetalLayer helper exists, not yet active default |
| Windows | D3D12 | planned — Dawn supports it, not yet integrated |
| Linux | Vulkan | planned — Dawn supports it, not yet integrated |
| Web | WebGPU | planned |

## When to Use

The render module is optional. Most of the framework works without it:

- **With render**: GPU-accelerated UI via Skia Graphite, WebGPU effects, on-screen presentation
- **Without render**: CoreGraphics canvas (macOS), headless testing, CLI tools

Plugins can be built, tested, and shipped without linking the render module. The format and state subsystems have no dependency on it. The CoreGraphics fallback path is the current default and is production-ready.
