#if defined(__ANDROID__)

#include <pulp/render/gpu_surface.hpp>
#include <pulp/render/skia_surface.hpp>
#include <pulp/canvas/canvas.hpp>
#include <pulp/view/view.hpp>
#include <pulp/view/widgets.hpp>
#include <pulp/view/theme.hpp>
#include <pulp/platform/android/jni.hpp>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>

#define PULP_LOG_TAG "Pulp"
#define PULP_LOGI(...) __android_log_print(ANDROID_LOG_INFO, PULP_LOG_TAG, __VA_ARGS__)
#define PULP_LOGW(...) __android_log_print(ANDROID_LOG_WARN, PULP_LOG_TAG, __VA_ARGS__)
#define PULP_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, PULP_LOG_TAG, __VA_ARGS__)

namespace pulp::render {

// ── Android GPU Surface Manager ───────────────────────────────────────────
// Manages the lifecycle of Dawn/Vulkan rendering on Android.
// Bridges ANativeWindow from Kotlin SurfaceView to GpuSurface.

static void android_render_frame();
static std::unique_ptr<GpuSurface> g_gpu_surface;
static std::unique_ptr<SkiaSurface> g_skia_surface;
static std::unique_ptr<view::View> g_root_view;

// Display density from Android (set by Kotlin before surface creation)
static float g_display_density = 2.625f;  // default xxhdpi, overridden by Kotlin

// Touch state: which view is being dragged
static view::View* g_captured_view = nullptr;

void android_set_display_density(float density) {
    g_display_density = density;
    PULP_LOGI("Display density set to %.2f", density);
}

static void create_demo_view_hierarchy(float width, float height) {
    using namespace view;

    // Convert pixel dimensions to density-independent pixels
    float dp_w = width / g_display_density;
    float dp_h = height / g_display_density;

    g_root_view = std::make_unique<Panel>();
    g_root_view->set_bounds({0, 0, dp_w, dp_h});
    g_root_view->set_theme(Theme::dark());
    g_root_view->flex().padding = 16;

    // Skip Labels for now — fill_text crashes on Android Vulkan swapchain
    // Spacer instead of title
    auto spacer = std::make_unique<Panel>();
    spacer->flex().preferred_height = 40;
    spacer->flex().margin_top = 24;
    g_root_view->add_child(std::move(spacer));

    // Knob row
    auto knob_row = std::make_unique<Panel>();
    knob_row->flex().direction = FlexDirection::row;
    knob_row->flex().preferred_height = 64;
    knob_row->flex().margin_top = 16;
    knob_row->flex().justify_content = FlexJustify::space_evenly;

    for (int i = 0; i < 3; ++i) {
        auto knob = std::make_unique<Knob>();
        knob->set_value(0.3f + i * 0.2f);
        knob->flex().preferred_width = 56;
        knob->flex().preferred_height = 56;
        knob_row->add_child(std::move(knob));
    }
    g_root_view->add_child(std::move(knob_row));

    // Fader
    auto fader = std::make_unique<Fader>();
    fader->set_value(0.7f);
    fader->flex().preferred_height = 32;
    fader->flex().margin_top = 16;
    fader->flex().margin_left = 16;
    fader->flex().margin_right = 16;
    g_root_view->add_child(std::move(fader));

    // Meter
    auto meter = std::make_unique<Meter>();
    meter->set_level(-12.0f, -6.0f);
    meter->flex().preferred_height = 24;
    meter->flex().margin_top = 16;
    meter->flex().margin_left = 16;
    meter->flex().margin_right = 16;
    g_root_view->add_child(std::move(meter));

    // Bottom spacer
    auto bottom = std::make_unique<Panel>();
    bottom->flex().preferred_height = 16;
    bottom->flex().margin_top = 16;
    g_root_view->add_child(std::move(bottom));

    g_root_view->layout_children();
    PULP_LOGI("Android GPU surface: View hierarchy created (%d children, %.0fx%.0f dp)",
              static_cast<int>(g_root_view->child_count()), dp_w, dp_h);
}

static ANativeWindow* g_native_window = nullptr;
static std::mutex g_surface_mutex;
static std::condition_variable g_surface_cv;
static bool g_render_stopped = true;
static bool g_surface_valid = false;

void android_surface_created(ANativeWindow* window) {
    PULP_LOGI("Android GPU surface: ANativeWindow received (%dx%d)",
              ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

    g_native_window = window;
    ANativeWindow_acquire(g_native_window);

    // Create and initialize Dawn GPU surface
    g_gpu_surface = GpuSurface::create_dawn();
    if (!g_gpu_surface) {
        PULP_LOGE("Android GPU surface: failed to create Dawn GpuSurface");
        return;
    }

    GpuSurface::Config config;
    config.width = static_cast<uint32_t>(ANativeWindow_getWidth(window));
    config.height = static_cast<uint32_t>(ANativeWindow_getHeight(window));
    config.native_surface_handle = g_native_window;
    config.vsync = true;

    if (g_gpu_surface->initialize(config)) {
        PULP_LOGI("Android GPU surface: Dawn initialized (%ux%u)",
                  config.width, config.height);

        auto info = g_gpu_surface->adapter_info();
        PULP_LOGI("Android GPU surface: adapter=%s backend=%s",
                  info.name.c_str(), info.backend.c_str());

        // Create Skia Graphite surface wrapping the Dawn device
        SkiaSurface::Config skia_config;
        skia_config.width = config.width;
        skia_config.height = config.height;
        skia_config.scale_factor = g_display_density;  // SkiaSurface applies this internally

        g_skia_surface = SkiaSurface::create(*g_gpu_surface, skia_config);
        if (g_skia_surface && g_skia_surface->is_available()) {
            PULP_LOGI("Android GPU surface: Skia Graphite context created");
        } else {
            PULP_LOGW("Android GPU surface: Skia Graphite failed — Dawn-only mode");
        }

        {
            std::lock_guard lock(g_surface_mutex);
            g_surface_valid = true;
            g_render_stopped = false;
        }

        // Render an initial frame
        android_render_frame();
    } else {
        PULP_LOGW("Android GPU surface: Dawn initialization failed — no GPU rendering");
        g_gpu_surface.reset();
    }
}

void android_surface_resized(int width, int height) {
    if (g_gpu_surface && g_gpu_surface->is_initialized()) {
        g_gpu_surface->resize(static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height));
        PULP_LOGI("Android GPU surface: resized to %dx%d", width, height);

        // Recreate view hierarchy at new size
        g_root_view.reset();
        g_captured_view = nullptr;

        android_render_frame();
    }
}

void android_render_frame() {
    if (!g_gpu_surface || !g_gpu_surface->is_initialized()) return;

    if (g_gpu_surface->begin_frame()) {
        if (g_skia_surface && g_skia_surface->is_available()) {
            auto* canvas = g_skia_surface->begin_frame();
            if (canvas) {
                float w = static_cast<float>(g_gpu_surface->width());
                float h = static_cast<float>(g_gpu_surface->height());

                // Create view hierarchy on first render
                if (!g_root_view) {
                    create_demo_view_hierarchy(w, h);
                }

                // SkiaSurface applies display density scaling internally
                g_root_view->paint_all(*canvas);

                g_skia_surface->end_frame();
            }
        }
        g_gpu_surface->end_frame();
    }
}

// ── Touch → View Routing ─────────────────────────────────────────────────

// Convert root dp coordinates to view-local coordinates by walking up the parent chain
static view::Point to_local(view::View* target, float dp_x, float dp_y) {
    // Accumulate absolute offset by walking up parents
    float abs_x = 0, abs_y = 0;
    for (auto* v = target; v != nullptr; v = v->parent()) {
        abs_x += v->bounds().x;
        abs_y += v->bounds().y;
    }
    return {dp_x - abs_x, dp_y - abs_y};
}

void android_touch_down(float px_x, float px_y) {
    if (!g_root_view) return;

    // Convert pixel coordinates to dp
    float dp_x = px_x / g_display_density;
    float dp_y = px_y / g_display_density;

    view::Point pt{dp_x, dp_y};
    auto* target = g_root_view->hit_test(pt);
    if (target) {
        auto local = to_local(target, dp_x, dp_y);
        target->on_mouse_down(local);
        g_captured_view = target;
        PULP_LOGI("Touch down at (%.0f,%.0f) dp, local (%.0f,%.0f)",
                  dp_x, dp_y, local.x, local.y);
    }

    // Repaint after interaction
    android_render_frame();
}

void android_touch_move(float px_x, float px_y) {
    if (!g_captured_view) return;

    float dp_x = px_x / g_display_density;
    float dp_y = px_y / g_display_density;

    auto local = to_local(g_captured_view, dp_x, dp_y);
    g_captured_view->on_mouse_drag(local);

    // Repaint after drag
    android_render_frame();
}

void android_touch_up(float px_x, float px_y) {
    if (!g_captured_view) return;

    float dp_x = px_x / g_display_density;
    float dp_y = px_y / g_display_density;

    auto local = to_local(g_captured_view, dp_x, dp_y);
    g_captured_view->on_mouse_up(local);
    g_captured_view = nullptr;

    // Repaint after release
    android_render_frame();
}

void android_surface_destroyed() {
    PULP_LOGI("Android GPU surface: destroying — waiting for render stop...");

    {
        std::lock_guard lock(g_surface_mutex);
        g_surface_valid = false;
    }

    // Wait for render to confirm stop (condition_variable, not spin)
    {
        std::unique_lock lock(g_surface_mutex);
        g_surface_cv.wait(lock, [] { return g_render_stopped; });
    }

    g_captured_view = nullptr;
    g_root_view.reset();
    g_gpu_surface.reset();

    if (g_native_window) {
        ANativeWindow_release(g_native_window);
        g_native_window = nullptr;
    }

    PULP_LOGI("Android GPU surface: destroyed");
}

// Called by the render loop each frame
bool android_gpu_begin_frame() {
    std::lock_guard lock(g_surface_mutex);
    if (!g_surface_valid || !g_gpu_surface) return false;
    return g_gpu_surface->begin_frame();
}

void android_gpu_end_frame() {
    if (g_gpu_surface) g_gpu_surface->end_frame();
}

void android_gpu_signal_render_stopped() {
    {
        std::lock_guard lock(g_surface_mutex);
        g_render_stopped = true;
    }
    g_surface_cv.notify_one();
}

GpuSurface* android_gpu_surface() {
    return g_gpu_surface.get();
}

} // namespace pulp::render

// ── JNI Exports ───────────────────────────────────────────────────────────

extern "C" {

// Display density — call from Kotlin before surface is created
JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeSetDisplayDensity(
    JNIEnv*, jobject, jfloat density) {
    pulp::render::android_set_display_density(density);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnSurfaceCreated(
    JNIEnv* env, jobject thiz, jobject surface) {
    try {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window) {
            pulp::render::android_surface_created(window);
            ANativeWindow_release(window);  // android_surface_created acquires its own ref
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
        pulp::render::android_surface_resized(width, height);
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
        pulp::render::android_surface_destroyed();
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Unknown C++ exception in nativeOnSurfaceDestroyed");
    }
}

// Touch events → View hierarchy
JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchDown(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y, jfloat pressure) {
    pulp::render::android_touch_down(x, y);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchMove(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y, jfloat pressure) {
    pulp::render::android_touch_move(x, y);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchUp(
    JNIEnv*, jobject, jint pointerId, jfloat x, jfloat y) {
    pulp::render::android_touch_up(x, y);
}

JNIEXPORT void JNICALL
Java_com_pulp_render_PulpSurfaceView_nativeOnTouchCancel(JNIEnv*, jobject) {
    pulp::render::g_captured_view = nullptr;
}

} // extern "C"

#endif // __ANDROID__
