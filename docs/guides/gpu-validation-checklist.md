# GPU Rendering Validation Checklist

Status of GPU rendering verification across platforms and configurations.

## Verified (Real Hardware)

| Platform | Backend | Surface | Rendering | Status |
|----------|---------|---------|-----------|--------|
| macOS (Apple Silicon) | Metal | CAMetalLayer (NSView) | Skia Graphite | **Verified** — GPU demo runs at 60fps |
| macOS (Apple Silicon) | Metal | CAMetalLayer (PluginViewHost) | Skia Graphite | **Verified** — DAW-embedded rendering |

## Implemented (Awaiting Hardware Validation)

| Platform | Backend | Surface | Rendering | Status |
|----------|---------|---------|-----------|--------|
| iOS | Metal | CAMetalLayer (UIView) | Skia Graphite | **Implemented** — IOSGpuWindowHost + IOSGpuPluginViewHost |
| Windows | D3D12 | HWND (SDL3) | Skia Graphite | **Implemented** — SDL3 HWND extraction + Dawn D3D12 |
| Linux/X11 | Vulkan | X11 Window (SDL3) | Skia Graphite | **Implemented** — SDL3 X11 extraction + Dawn Vulkan |
| Linux/Wayland | Vulkan | wl_surface (SDL3) | Skia Graphite | **Implemented** — SDL3 Wayland extraction + Dawn Vulkan |

## Architecture

```
Platform Window (NSView/HWND/SDL3)
    │
    ▼
Native Surface Handle (CAMetalLayer*/HWND/X11 Window)
    │
    ▼
GpuSurface (Dawn/WebGPU)
    ├── Metal backend (macOS/iOS)
    ├── D3D12 backend (Windows)
    └── Vulkan backend (Linux)
    │
    ▼
SkiaSurface (Skia Graphite)
    │
    ▼
SkCanvas → View tree painting
```

## Render Loop

| Platform | Mechanism | Target FPS |
|----------|-----------|------------|
| macOS | CVDisplayLink → main queue dispatch | Display refresh (60-120Hz) |
| iOS | CADisplayLink (planned) | Display refresh (60-120Hz) |
| Windows | Timer-based (upgradeable to DwmFlush) | 60Hz |
| Linux | Timer-based (upgradeable to present-paced) | 60Hz |
| WASM | requestAnimationFrame | Display refresh |

## Known Limitations

- **Windows/Linux GPU**: Timer-based render loop at 60Hz fixed rate, not true vsync
- **iOS GPU**: CADisplayLink integration not yet connected (render_frame() not called automatically)
- **Linux Wayland**: Requires compositor support for wl_subsurface
- **WASM**: WebGPU support depends on browser (Chrome 113+, Firefox 120+)

## Test Coverage

- 13 cross-platform render tests (GpuSurface + SkiaSurface)
- GPU demo validates continuous animation, vector drawing, resize
- Headless tests verify surface creation and texture lifecycle
