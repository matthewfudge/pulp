# Skia Pre-built Binaries

## Source
- **Repository:** https://github.com/olilarkin/skia-builder
- **Release:** chrome/m144
- **Release URL:** https://github.com/olilarkin/skia-builder/releases/tag/chrome%2Fm144
- **Downloaded:** 2026-03-25
- **Skia branch:** chrome/m144 (Skia Graphite + Dawn)

## Build Configuration
- Graphite GPU backend: enabled
- Dawn (WebGPU): enabled
- Metal: enabled (macOS/iOS)
- ICU Unicode: enabled
- SVG module: enabled
- Skottie (Lottie): enabled
- Paragraph/text shaping: enabled
- Build type: Release (optimized)

## Platforms Included

| Directory | Platform | Architectures | Size |
|-----------|----------|--------------|------|
| `mac-gpu/` | macOS | universal (arm64 + x86_64) | ~143 MB |
| `win-gpu/` | Windows | x64 | ~39 MB |
| `linux-gpu/` | Linux | x64 | ~39 MB |
| `ios-gpu/` | iOS device + simulator | arm64, arm64+x86_64 | ~104 MB |
| `wasm-gpu/` | WebAssembly | wasm32 | ~20 MB |

## To Update

1. Check for new releases: https://github.com/olilarkin/skia-builder/releases
2. Download the new platform zips
3. Extract to this directory (replacing `build/`)
4. Update this VERSION.md with the new release tag
5. Commit via Git LFS

Or run: `./tools/build-skia.sh <platform>` to build from source.

## Libraries Per Platform

Each platform includes:
- `libskia.a` — Core Skia + Graphite GPU backend
- `libdawn_combined.a` — Dawn WebGPU implementation
- `libskshaper.a` — Text shaping (HarfBuzz)
- `libskparagraph.a` — Paragraph layout
- `libskottie.a` — Lottie animation
- `libsksg.a` — Scene graph
- `libsvg.a` — SVG rendering
- `libskunicode_icu.a` — Unicode support
- `libskunicode_core.a` — Unicode core

## License
Skia: BSD-3-Clause (Google)
Dawn: BSD-3-Clause (Google)
