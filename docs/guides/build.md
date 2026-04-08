# Building Pulp

## Prerequisites

- CMake 3.24+
- C++20 compiler (Clang 15+, GCC 13+, MSVC 2022+)
- git-lfs (for Skia binaries)
- macOS: Xcode Command Line Tools
- Linux: `libasound2-dev` and desktop dev packages
- Windows: Visual Studio 2022+ or Build Tools

## Quick Build

```bash
./setup.sh                                    # bootstrap deps
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug  # configure
cmake --build build -j$(nproc)                # build
ctest --test-dir build --output-on-failure    # test
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PULP_BUILD_TESTS` | ON | Build test targets |
| `PULP_ENABLE_GPU` | ON | Enable Dawn/Skia GPU rendering |
| `PULP_JS_ENGINE` | auto | JS engine: auto, quickjs, jsc, v8 |
| `PULP_ENABLE_AAX` | OFF | Enable optional AAX format |

## Platform-Specific Notes

See [macOS](platform-macos.html), [Windows](platform-windows.html), [Linux](platform-linux.html).
