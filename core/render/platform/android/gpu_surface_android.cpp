#if defined(__ANDROID__)

#include <pulp/platform/android/jni.hpp>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <condition_variable>

#define PULP_LOG_TAG "Pulp"
#define PULP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PULP_LOG_TAG, __VA_ARGS__)
#define PULP_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PULP_LOG_TAG, __VA_ARGS__)
#define PULP_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PULP_LOG_TAG, __VA_ARGS__)

namespace pulp::render {

// ── Android GPU Surface ───────────────────────────────────────────────────
// Manages the native window for Vulkan/Dawn rendering on Android.
// Handles the critical surfaceDestroyed synchronization to prevent SIGSEGV.

class AndroidGpuSurface {
public:
    void on_surface_created(ANativeWindow* window) {
        {
            std::lock_guard lock(surface_mutex_);
            native_window_ = window;
            ANativeWindow_acquire(native_window_);
            surface_valid_ = true;
            render_stopped_ = false;
        }

        width_ = ANativeWindow_getWidth(window);
        height_ = ANativeWindow_getHeight(window);

        PULP_LOGI("GPU surface created: %dx%d", width_, height_);

        // TODO: Create Dawn/WebGPU surface from ANativeWindow
        // wgpu::SurfaceDescriptorFromAndroidNativeWindow desc;
        // desc.window = native_window_;
        // surface_ = instance_.CreateSurface(&desc);

        first_frame_rendered_ = false;
    }

    void on_surface_resized(int width, int height) {
        width_ = width;
        height_ = height;
        PULP_LOGI("GPU surface resized: %dx%d", width, height);
        // TODO: Reconfigure Dawn swap chain
    }

    // CRITICAL: Called from surfaceDestroyed on the main thread.
    // Must block until the render thread has fully stopped.
    void on_surface_destroyed() {
        PULP_LOGI("GPU surface destroying — waiting for render thread...");

        {
            std::lock_guard lock(surface_mutex_);
            surface_valid_ = false;
        }

        // Wait for render thread to confirm stop.
        // Using condition_variable (not spin-sleep) — this is the UI thread,
        // not the audio thread, so blocking is safe and saves battery.
        {
            std::unique_lock lock(surface_mutex_);
            surface_cv_.wait(lock, [this] { return render_stopped_; });
        }

        // Release the native window — safe now, render thread is idle
        if (native_window_) {
            ANativeWindow_release(native_window_);
            native_window_ = nullptr;
        }

        PULP_LOGI("GPU surface destroyed — render thread confirmed stopped");
    }

    // Called by the render thread each frame
    bool begin_frame() {
        std::lock_guard lock(surface_mutex_);
        return surface_valid_;
    }

    void end_frame() {
        if (!first_frame_rendered_) {
            first_frame_rendered_ = true;
            PULP_LOGI("First frame rendered — Vulkan confirmed working");
            // TODO: Call GpuDriverPolicy.markVulkanSuccess() via JNI
        }
    }

    // Called by the render thread when it exits the loop
    void signal_render_stopped() {
        {
            std::lock_guard lock(surface_mutex_);
            render_stopped_ = true;
        }
        surface_cv_.notify_one();
    }

    int width() const { return width_; }
    int height() const { return height_; }
    ANativeWindow* native_window() const { return native_window_; }

private:
    ANativeWindow* native_window_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    std::mutex surface_mutex_;
    std::condition_variable surface_cv_;
    bool surface_valid_ = false;
    bool render_stopped_ = true;
    bool first_frame_rendered_ = false;
};

// Global surface instance
static AndroidGpuSurface g_surface;

} // namespace pulp::render

// ── Touch Input State ─────────────────────────────────────────────────────

namespace pulp::input {

using TouchCallback = void(*)(int pointer_id, float x, float y, float pressure,
                               int action, void* user_data);
static TouchCallback g_touch_callback = nullptr;
static void* g_touch_callback_data = nullptr;

void set_touch_callback(TouchCallback cb, void* user_data) {
    g_touch_callback = cb;
    g_touch_callback_data = user_data;
}

enum TouchAction { Down = 0, Move = 1, Up = 2, Cancel = 3 };

} // namespace pulp::input

// ── JNI Exports ───────────────────────────────────────────────────────────

extern "C" {

// Surface lifecycle
JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnSurfaceCreated(
    JNIEnv* env, jobject thiz, jobject surface) {
    try {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window) {
            pulp::render::g_surface.on_surface_created(window);
            ANativeWindow_release(window);  // on_surface_created acquired its own ref
        } else {
            PULP_LOGE("ANativeWindow_fromSurface returned null");
        }
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnSurfaceCreated");
    }
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnSurfaceResized(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    try {
        pulp::render::g_surface.on_surface_resized(width, height);
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnSurfaceResized");
    }
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnSurfaceDestroyed(
    JNIEnv* env, jobject thiz) {
    try {
        pulp::render::g_surface.on_surface_destroyed();
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnSurfaceDestroyed");
    }
}

// Touch events
JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchDown(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y, jfloat pressure) {
    if (pulp::input::g_touch_callback)
        pulp::input::g_touch_callback(pointerId, x, y, pressure,
                                       pulp::input::Down, pulp::input::g_touch_callback_data);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchMove(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y, jfloat pressure) {
    if (pulp::input::g_touch_callback)
        pulp::input::g_touch_callback(pointerId, x, y, pressure,
                                       pulp::input::Move, pulp::input::g_touch_callback_data);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchUp(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y) {
    if (pulp::input::g_touch_callback)
        pulp::input::g_touch_callback(pointerId, x, y, 0.0f,
                                       pulp::input::Up, pulp::input::g_touch_callback_data);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchCancel(JNIEnv*, jobject) {
    if (pulp::input::g_touch_callback)
        pulp::input::g_touch_callback(-1, 0, 0, 0,
                                       pulp::input::Cancel, pulp::input::g_touch_callback_data);
}

} // extern "C"

#endif // __ANDROID__
