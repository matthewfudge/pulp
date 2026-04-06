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
config.native_surface_handle = (__bridge void*)metalLayer;  // CAMetalLayer*

gpu->initialize(config);

// For offscreen-only mode, leave native_surface_handle as nullptr:
// config.native_surface_handle = nullptr;  // default
```

### Frame Lifecycle

1. `begin_frame()` — acquires the next swapchain texture (returns false if surface is lost/minimized)
2. Draw commands are recorded via SkiaSurface / Skia Graphite Recorder
3. `end_frame()` — submits GPU work and presents to the native surface

When `native_surface_handle` is null, `begin_frame()` always succeeds and `end_frame()` only processes events (no presentation).

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
- Provides the `CAMetalLayer` handle to `GpuSurface::Config::native_surface_handle`

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
| Skia Graphite → on-screen present | Requires GpuSurface with native_surface_handle; not yet default |
| CoreGraphics fallback (macOS) | Works and is the current default render path |
| View host GPU integration | Planned — view hosts currently use CoreGraphics only |

## Cross-Platform GPU Rendering Model

The same core rendering pipeline applies across macOS, iOS, Windows, and Linux. Only native surface creation and frame driving differ per platform.

**Shared architecture (all platforms):**

1. **One Dawn device/queue** — `GpuSurface` creates and owns the Dawn instance, adapter, device, and queue. There is exactly one device per render context. `SkiaSurface` borrows this device; it does not create its own.

2. **Platform-specific surface creation** — The platform host creates a native view or window and passes a handle to `GpuSurface::Config::native_surface_handle`. On macOS/iOS this is a `CAMetalLayer*`, on Windows an `HWND`, on Linux a handle extracted from SDL3 (X11, Wayland, or XCB). `GpuSurface` uses this to create a Dawn surface for on-screen presentation.

3. **Per-frame rendering flow:**
   - `GpuSurface::begin_frame()` — acquires the current presentable texture from the surface
   - `SkiaSurface::begin_frame()` — wraps that texture as a Skia Graphite `BackendTexture` and returns a `Canvas`
   - **Draw** — the view tree paints into the canvas
   - `SkiaSurface::end_frame()` — snaps the Graphite recording and submits GPU work on the shared device/queue
   - `GpuSurface::end_frame()` — presents the rendered frame to the native surface

4. **Resize and DPI** — The platform host detects size and scale changes and calls `GpuSurface::resize()` + `SkiaSurface::resize()` before the next frame acquire. `GpuSurface` reconfigures the Dawn surface with the new dimensions. Format and present mode are selected from `Surface::GetCapabilities()`, not hardcoded.

5. **Frame driving** — Each platform uses its own mechanism to pace frames. macOS uses `CVDisplayLink` (vsync-accurate). Windows and Linux use Dawn's `PresentMode::Fifo` for natural frame pacing. The platform host owns the frame loop; the render module does not assume a specific driver.

**Responsibility boundaries:**

| Layer | Owns | Does not own |
|-------|------|-------------|
| **Platform host** | Native view/window, resize/DPI events, frame loop | GPU device, Metal/D3D12/Vulkan objects |
| **GpuSurface** | Dawn device/queue/surface, acquire/present, reconfigure | Widget trees, paint traversal, layout |
| **SkiaSurface** | Graphite context/recorder, per-frame texture wrapping | Surface lifecycle, presentation policy |
| **View** | Layout, input, paint traversal | Native views, GPU objects |

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
