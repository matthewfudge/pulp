# Android Development Skill

Build, test, and debug Pulp's Android target. Covers NDK cross-compilation, Gradle integration, Skia GPU build, JNI bridge patterns, and emulator testing.

## Prerequisites

```bash
# Required
pulp doctor --android    # or check manually:
# - Android SDK at ~/Library/Android/sdk (or ANDROID_HOME)
# - NDK 30.x (sdkmanager "ndk;30.0.14904198")
# - Java 17+ (openjdk)
# - ninja (brew install ninja, for Skia build)

# Optional
# - Android emulator + AVD for testing
# - Android Studio for GUI debugging
```

## Build Commands

```bash
# Quick build (NDK + Gradle → APK)
cd android && ANDROID_HOME=~/Library/Android/sdk ./gradlew assembleDebug

# Full clean build
cd android && rm -rf app/.cxx app/build && ./gradlew assembleDebug

# Direct CMake cross-compile (without Gradle)
NDK=$ANDROID_HOME/ndk/30.0.14904198
cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_NATIVE_API_LEVEL=26 \
      -DANDROID_STL=c++_shared -DPULP_ENABLE_GPU=OFF \
      -DPULP_BUILD_TESTS=OFF -DPULP_BUILD_EXAMPLES=OFF \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -S . -B build-android
cmake --build build-android -j$(sysctl -n hw.ncpu)

# Build Skia for Android (first time only, ~5 min on M1 Max)
./tools/build-skia-android.sh

# Build with GPU (requires Skia Android binaries)
# In android/app/build.gradle.kts, set PULP_ENABLE_GPU=ON
```

## Emulator Testing

```bash
# Start emulator
$ANDROID_HOME/emulator/emulator -avd <avd-name> &

# Wait for boot
adb wait-for-device
adb shell getprop sys.boot_completed  # wait for "1"

# Install and launch
adb install android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.pulp.app/com.pulp.PulpActivity

# Check native bridge initialized
adb logcat -s Pulp:V

# Expected output:
# I Pulp: JNI_OnLoad: initializing Pulp native bridge
# I Pulp: JNI_OnLoad: cached class com/pulp/bridge/PulpBridge
# I Pulp: JNI_OnLoad: cached class com/pulp/PulpActivity
# ...
# I Pulp: JNI_OnLoad: Pulp native bridge initialized

# Screenshot
adb shell screencap -p /sdcard/screenshot.png
adb pull /sdcard/screenshot.png /tmp/screenshot.png
```

## Critical Gotchas

### Platform Detection (CMake)
```cmake
# WRONG — Android is also UNIX, so this makes Android unreachable:
if(UNIX)        # matches Android!
elseif(ANDROID) # never reached

# RIGHT — check ANDROID before UNIX:
if(ANDROID)
elseif(UNIX)
```

### Platform Guards (C++)
```cpp
// WRONG — __linux__ matches Android:
#elif defined(__linux__)
    #include "fontconfig.h"  // doesn't exist on Android!

// RIGHT — check __ANDROID__ before __linux__:
#elif defined(__ANDROID__)
    #include "android_font_manager.h"
#elif defined(__linux__)
    #include "fontconfig.h"
```

### posix_spawn Not Available
```cpp
// Android Bionic doesn't support posix_spawn. Use fork/exec:
#ifdef __ANDROID__
    pid = fork();
    if (pid == 0) { execvp(...); _exit(127); }
#else
    posix_spawn(&pid, ...);
#endif
```

### JNI Symbol Export (--whole-archive)
```cmake
# JNI functions in static libs are "unused" from the linker's perspective.
# Without --whole-archive, they get stripped → UnsatisfiedLinkError at runtime.
target_link_options(pulp-jni PRIVATE
    -Wl,--whole-archive
    $<TARGET_FILE:pulp-platform>
    $<TARGET_FILE:pulp-audio>
    -Wl,--no-whole-archive)
```

### JNI FindClass Trap on Native Threads
```cpp
// Native threads use System ClassLoader → FindClass("com/pulp/...") FAILS.
// Cache all app classes in JNI_OnLoad (runs on main thread with app ClassLoader):
extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    g_MyClass = (jclass)env->NewGlobalRef(env->FindClass("com/pulp/MyClass"));
    return JNI_VERSION_1_6;
}
```

### CMake 4.x Compatibility
```cmake
# Oboe and other FetchContent deps have old cmake_minimum_required.
# CMake 4.x rejects them. Add to Gradle cmake arguments:
arguments += "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
```

### Skia Android Build
```bash
# Skia headers reference src/, modules/, third_party/ from source tree.
# The include path must point at the Skia SOURCE ROOT, not just include/.
# FindSkia.cmake adds: skia-src/ as include, plus Dawn source + generated headers.

# Skia for Android uses bundled FreeType, expat, ICU, HarfBuzz — never
# system libs (which don't exist on Android NDK).

# Only arm64-v8a built by default. x86_64 needs a separate Skia build
# or GPU disabled for emulator.
```

### Gradle local.properties
```bash
# Must exist before Gradle build. Auto-generate:
echo "sdk.dir=$HOME/Library/Android/sdk" > android/local.properties
```

## Architecture

```
android/app/build.gradle.kts
  → CMakeLists.txt (root, with NDK toolchain)
    → PulpAndroid.cmake (Oboe FetchContent, Android source wiring)
    → FindSkia.cmake (finds android-gpu/ libs)
    → 12 static libraries → pulp-jni shared lib (libpulp.so)

Kotlin layer (android/app/src/main/kotlin/com/pulp/):
  PulpApplication.kt → System.loadLibrary("pulp") → JNI_OnLoad
  PulpActivity.kt    → Compose UI, lifecycle, onTrimMemory
  PulpAudioService.kt → Foreground service for background audio
  PulpAudioEngine.kt  → Device enumeration, optimal config
  PulpMidiManager.kt  → MIDI device discovery, port open/close
  PulpSurfaceView.kt  → Vulkan surface (Phase 4, not yet connected)
```

## File Locations

| What | Where |
|------|-------|
| Android CMake module | `tools/cmake/PulpAndroid.cmake` |
| Skia build script | `tools/build-skia-android.sh` |
| FindSkia (with Android) | `tools/cmake/FindSkia.cmake` |
| JNI bridge | `core/platform/src/android/jni_bridge.cpp` |
| JNI headers | `core/platform/include/pulp/platform/android/jni.hpp` |
| Oboe audio device | `core/audio/platform/android/oboe_device.cpp` |
| ADPF hints | `core/audio/platform/android/adpf_hints.cpp` |
| Tone generator demo | `core/audio/platform/android/tone_generator.cpp` |
| Android MIDI | `core/midi/platform/android/android_midi.cpp` |
| Kotlin sources | `android/app/src/main/kotlin/com/pulp/` |
| Gradle config | `android/app/build.gradle.kts` |
| Manifest | `android/app/src/main/AndroidManifest.xml` |
| Plan | `planning/android-platform-plan-v5.md` |
