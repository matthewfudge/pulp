---
name: android
description: Android platform development for Pulp — NDK cross-compilation, Oboe audio, Dawn/Skia GPU rendering, JNI bridge, touch interaction, emulator workflows. Covers build, deploy, debug, and the gotchas discovered during bringup.
requires:
  scripts:
    - tools/cmake/PulpAndroid.cmake
  tools:
    - adb
    - emulator
---

# Android Skill

Build, deploy, and debug Pulp on Android. This skill captures the architecture decisions and hard-won gotchas from the Android platform bringup.

## Architecture Overview

```
Kotlin (Android UI)          C++ (Pulp core)
├── PulpApplication          ├── libpulp.so (arm64)
│   └── System.loadLibrary   │   ├── core/* subsystems
├── PulpSurfaceView          │   ├── Dawn (Vulkan backend)
│   ├── SurfaceHolder.Cb     │   ├── Skia Graphite
│   └── onTouchEvent ──JNI──→│   └── Oboe audio
├── PulpActivity             │
├── PulpAudioService         └── gpu_surface_android.cpp
└── PulpMidiService              ├── Surface lifecycle
                                 ├── Touch → View routing
                                 └── Widget hierarchy
```

### Key Files

| File | Purpose |
|------|---------|
| `tools/cmake/PulpAndroid.cmake` | NDK toolchain integration, Oboe, platform wiring |
| `core/render/platform/android/gpu_surface_android.cpp` | Vulkan surface, touch routing, widget hierarchy |
| `android/app/src/main/kotlin/com/pulp/render/PulpSurfaceView.kt` | SurfaceView + touch dispatch |
| `android/app/src/main/kotlin/com/pulp/PulpApplication.kt` | JNI load, lifecycle |
| `core/platform/src/android/jni_bridge.cpp` | JNI_OnLoad, class caching, exception guards |
| `android/build-skia-android.sh` | Skia m144 Graphite + Dawn arm64 build script |

### Branch & Worktree

Android work lives on `feature/android-targeting`, typically in a worktree at `../pulp-android`:

```bash
# Create worktree (if not already present)
git worktree add ../pulp-android feature/android-targeting

# Always work in the worktree, not the main repo
cd ../pulp-android
```

## Build

### Cross-compile for Android

```bash
# NDK must be installed (Android Studio SDK Manager or standalone)
# Set NDK path — typically:
export ANDROID_NDK=$HOME/Library/Android/sdk/ndk/30.0.8

# Configure for Android arm64
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_NATIVE_API_LEVEL=26 \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-android -j$(sysctl -n hw.ncpu)
```

### Build the APK

```bash
cd android
./gradlew assembleDebug
# Output: android/app/build/outputs/apk/debug/app-debug.apk
```

### Host build (for tests)

```bash
# Regular host build still works in the android worktree
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

## Deploy & Test on Emulator

### Start emulator

```bash
# List available AVDs
emulator -list-avds

# Start (use -wipe-data if StorageManager is corrupted)
emulator -avd Medium_Phone_API_36 -gpu swiftshader_indirect &

# Wait for boot
adb wait-for-device
adb shell getprop sys.boot_completed  # returns "1" when ready
```

### Install and launch

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.pulp/.PulpActivity

# If killed for slow start (Dawn init takes 3+ seconds), just retry:
adb shell am start -n com.pulp/.PulpActivity
```

### Screenshot verification

```bash
adb exec-out screencap -p > /tmp/pulp-android.png
# View with: open /tmp/pulp-android.png
```

### Touch testing via adb

```bash
# Tap at coordinates (dp)
adb shell input tap 200 400

# Drag (swipe) from point A to point B over 300ms
adb shell input swipe 200 400 200 300 300

# Take screenshot after to verify state changed
sleep 0.5 && adb exec-out screencap -p > /tmp/after-tap.png
```

### Logcat

```bash
# Filter to Pulp logs
adb logcat -s PulpAudio PulpRender Pulp

# Clear and watch
adb logcat -c && adb logcat -s Pulp
```

## Critical Gotchas

These are hard-won lessons from the bringup. Violating any of these will cause crashes or subtle bugs.

### Platform Detection Order

```cpp
// WRONG — __linux__ matches Android too!
#if defined(__linux__)
    // Linux code...
#elif defined(__ANDROID__)
    // Never reached!

// RIGHT — check Android first
#if defined(__ANDROID__)
    // Android code...
#elif defined(__linux__)
    // Linux code...
```

In CMake, `ANDROID` must be checked before `UNIX` for the same reason.

### No posix_spawn on Android

Android Bionic doesn't have `posix_spawn`. Use `fork`/`exec` instead. The `ChildProcess` subsystem already handles this with `#ifdef __ANDROID__` guards.

### Vulkan Swapchain Format

Android Vulkan uses **RGBA8**, not BGRA8 (which macOS/desktop Vulkan uses). The Dawn backend texture info must match exactly, or Skia canvas operations (especially text rendering) will crash or produce garbage.

### SkiaSurface Scaling — Don't Double-Scale

`SkiaSurface` applies `scale_factor` internally via `canvas->scale()`. If you also call `canvas->scale(density, density)` in your render function, everything renders 2x too large.

```cpp
// WRONG
canvas->scale(density, density);  // SkiaSurface already did this!
root->paint(*canvas);

// RIGHT — just paint, SkiaSurface handles the scale
root->paint(*canvas);
```

### Touch Coordinate Conversion

Android touch events arrive in physical pixels. Convert to dp before hit-testing:

```cpp
float dp_x = pixel_x / display_density;
float dp_y = pixel_y / display_density;
auto* hit = root->hit_test({dp_x, dp_y});
```

Then convert dp to view-local coordinates by walking up the parent chain:

```cpp
static view::Point to_local(view::View* target, float dp_x, float dp_y) {
    float abs_x = 0, abs_y = 0;
    for (auto* v = target; v != nullptr; v = v->parent()) {
        abs_x += v->bounds().x;
        abs_y += v->bounds().y;
    }
    return {dp_x - abs_x, dp_y - abs_y};
}
```

### MouseEvent Dispatch — Both Rich and Legacy

Some widgets (like Fader) use `on_mouse_event(MouseEvent)` to track drag state. Others use the simple `on_mouse_down(Point)`. Always dispatch both:

```cpp
view::MouseEvent ev;
ev.position = local;
ev.window_position = dp;
ev.button = view::MouseButton::left;
ev.pointer_type = view::PointerType::touch;
ev.pressure = pressure;
ev.is_down = true;
ev.click_count = click_count;

hit->on_mouse_event(ev);   // Rich event (Fader needs this)
hit->on_mouse_down(local);  // Legacy event (Toggle, Knob need this)
```

### Fader Orientation Default

`Fader::orientation_` defaults to `Orientation::vertical`. If you want horizontal faders, set explicitly:

```cpp
fader->set_orientation(Fader::Orientation::horizontal);
```

### View Hierarchy Depth and Flex Layout

Deeply nested Panel containers (Panel > Panel > Widget) break flex layout propagation — child bounds remain {0,0,0,0} and hit_test returns the parent Panel instead of the widget. Keep hierarchies flat:

```cpp
// WRONG — nested wrappers break layout
auto wrapper = std::make_shared<Panel>();
auto inner = std::make_shared<Panel>();
inner->add_child(knob);
wrapper->add_child(inner);
root->add_child(wrapper);

// RIGHT — add directly or one level deep
auto row = std::make_shared<Panel>();
row->add_child(knob1);
row->add_child(knob2);
root->add_child(row);
```

### Don't Call layout_children() on Sub-Panels Early

Calling `layout_children()` on a sub-panel before it has been assigned bounds by its parent's layout pass will hang or produce zero-size children. Only call `layout_children()` on the root after setting root bounds.

### JNI Symbol Export

Use `--whole-archive` when linking `libpulp.so` to ensure JNI symbols are exported:

```cmake
target_link_options(pulp-jni PRIVATE
    -Wl,--whole-archive $<TARGET_FILE:pulp-platform> -Wl,--no-whole-archive
)
```

### Skia Build Dependencies

Skia headers reference `src/` and `modules/` from the source tree. Dawn headers need both source and generated includes. The build script `build-skia-android.sh` handles this, but if you're debugging include paths:

```cmake
target_include_directories(pulp-render PRIVATE
    ${SKIA_SOURCE_DIR}          # for src/ headers
    ${SKIA_SOURCE_DIR}/modules  # for skparagraph etc.
    ${DAWN_SOURCE_DIR}/include
    ${DAWN_BUILD_DIR}/gen/include
)
```

### Oboe Compiler Warnings

NDK 30 Clang is stricter than desktop Clang. Suppress warnings in Oboe with `-w`:

```cmake
if(TARGET oboe)
    target_compile_options(oboe PRIVATE -w)
endif()
```

### Emulator StorageManager Corruption

If you see `NullPointerException` on `StorageManager.getVolumes()`, the emulator's data partition is corrupted. Fix:

```bash
# Kill emulator
adb emu kill
# Restart with data wipe
emulator -avd <avd_name> -wipe-data -no-snapshot
```

### Guard PulpAndroid.cmake with if(ANDROID)

The `include(PulpAndroid.cmake)` in root CMakeLists.txt must be wrapped in `if(ANDROID)` to prevent any side effects on non-Android builds:

```cmake
if(ANDROID)
    include(${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/PulpAndroid.cmake)
endif()
```

## Widget Painting Convention

When mapping a 0..1 value to a pixel position with a circular/rect indicator, **always inset by the indicator's radius** so it stays fully within widget bounds:

```cpp
// Convention for slider-like widgets:
float usable = length - 2.0f * radius;
float pos = radius + value * usable;
```

This applies to: Fader thumb, XYPad dot, CorrelationMeter bar, ColorPicker cursors. Knob and Toggle already self-inset by design.

## Widget Animations Without a Render Loop

Many widgets use `ValueAnimation` for visual state changes (Toggle thumb position, Knob hover scale, Fader hover). Without a continuous render loop (AChoreographer), `advance_animations(dt)` is never called, so animations are started but never progress — the widget's visual state appears stuck even though the logical state changed.

**Fix**: Before each paint, advance animations with a large dt to snap to completion:

```cpp
// Recursively advance all widget animations
static void advance_view_animations(view::View* v, float dt) {
    if (auto* k = dynamic_cast<view::Knob*>(v))   k->advance_animations(dt);
    if (auto* f = dynamic_cast<view::Fader*>(v))   f->advance_animations(dt);
    if (auto* t = dynamic_cast<view::Toggle*>(v))  t->advance_animations(dt);
    for (size_t i = 0; i < v->child_count(); ++i)
        advance_view_animations(v->child_at(i), dt);
}

// In render function, before paint:
advance_view_animations(g_root_view.get(), 1.0f);  // 1s snaps to target
```

This is a workaround until AChoreographer provides a proper frame loop with real dt values.

## Known Blockers

1. **Text rendering crashes** — `fill_text()` / Labels crash on Android. Root cause: SkCanvas internal state mismatch from swapchain texture format. Fix requires ensuring Graphite backend texture info matches the Vulkan swapchain format exactly.

2. **No continuous render loop** — Currently repaints only on touch. Need AChoreographer callback for smooth animation.

3. **x86_64 Skia build** — Only arm64 Skia is built. Emulator runs arm64 via translation but an x86_64 build would be faster.
