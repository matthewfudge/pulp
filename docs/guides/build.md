# Build Guide

## Requirements

- CMake 3.24+
- C++20 compiler:
  - macOS: Apple Clang (Xcode 15+) or Clang 15+
  - Linux: GCC 13+ or Clang 15+
  - Windows: MSVC 2022+

## Basic Build

```bash
cmake -B build
cmake --build build
```

Or use the Pulp CLI:

```bash
pulp build
```

The CLI auto-detects when CMake reconfiguration is needed (if `CMakeLists.txt` is newer than `CMakeCache.txt`).

## Build Options

Pass extra arguments through the CLI:

```bash
pulp build --target PulpGain_VST3    # Build a specific target
pulp build -j8                        # Parallel jobs
```

Or directly with CMake:

```bash
cmake --build build --target PulpGain_VST3
cmake --build build --parallel 8
```

## Build Outputs

Plugin bundles are placed under the build directory:

| Format     | Location                              | Extension    |
|------------|---------------------------------------|-------------|
| VST3       | `build/VST3/<name>.vst3`              | `.vst3`     |
| CLAP       | `build/CLAP/<name>.clap`              | `.clap`     |
| AU v2      | `build/AU/<name>.component`           | `.component`|
| Standalone | `build/<name>`                        | (executable)|

## External Dependencies

The build system fetches some dependencies automatically via CMake FetchContent:

- **CLAP SDK** -- fetched automatically
- **Catch2** -- fetched automatically for tests

Other dependencies are provided or verified by the source tree bootstrap:

- **VST3 SDK** -- provided under `external/vst3sdk/` by `./setup.sh` from a pinned shared SDK cache
- **AudioUnit SDK** -- provided under `external/AudioUnitSDK/` by `./setup.sh` on macOS from a pinned shared SDK cache
- **Skia** -- must already be present under `external/skia-build/` by default, or provided via `SKIA_DIR`. `./tools/build-skia.sh` pins the helper repo to a known `skia-builder` commit and defaults Skia itself to the `chrome/m144` branch.

The GPU build fetches WebGPU/Dawn through CMake `FetchContent`; you do not need a
separate `external/dawn/` checkout.

Pulp now prefers machine-local shared source caches for FetchContent dependencies when
they are present. `./setup.sh` populates those caches under:

- macOS: `~/Library/Caches/Pulp/fetchcontent-src/`
- Linux: `${XDG_CACHE_HOME:-~/.cache}/pulp/fetchcontent-src/`

This shares pinned dependency source trees like `webgpu`, `sdl3`, `clap`, `yoga`,
`catch2`, `vst3sdk`, and `AudioUnitSDK` across worktrees without sharing any
`_deps/*-build` directories. On platforms that use prebuilt WebGPU runtimes, Pulp
also caches the extracted `wgpu-native` payload under the same root so new build
trees do not each download it. Cache entries are ref-specific, so branches pinned to
different dependency revisions do not fight over one mutable checkout.

You can override the shared cache root with:

```bash
cmake -S . -B build -DPULP_SHARED_FETCHCONTENT_SOURCE_DIR=/path/to/cache
```

Or override a single dependency source tree directly with standard CMake
FetchContent source overrides:

```bash
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_WEBGPU=/path/to/WebGPU-distribution
cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL
```

## Clean Build

```bash
pulp clean        # Removes the build directory
cmake -B build    # Reconfigure from scratch
```

## Platform Notes

### macOS

macOS is the primary development platform. All formats (VST3, AU v2, CLAP, Standalone) build and validate here. The AU format requires the AudioUnit SDK and is macOS-only.

### Windows / Linux

Build stubs exist but are not yet validated end-to-end. VST3 and CLAP format targets should configure. AU is unavailable.

## Adding a Plugin to the Build

Use `pulp_add_plugin()` in your `CMakeLists.txt`. See `docs/reference/cmake.md` for the full function signature and options.

Each plugin target creates format-specific sub-targets (`_VST3`, `_AU`, `_CLAP`, `_Standalone`) that can be built independently.
