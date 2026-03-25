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

Other dependencies must be present:

- **VST3 SDK** -- expected at `external/vst3sdk/`
- **AudioUnit SDK** -- expected at `external/AudioUnitSDK/` (macOS only)
- **Dawn** -- expected at `external/dawn/` (for GPU rendering)
- **Skia** -- expected at `external/skia-build/` (for GPU rendering)

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
