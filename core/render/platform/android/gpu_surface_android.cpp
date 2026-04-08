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

static void android_render_test_frame();
static std::unique_ptr<GpuSurface> g_gpu_surface;
static std::unique_ptr<SkiaSurface> g_skia_surface;
static std::unique_ptr<view::View> g_root_view;

static void create_demo_view_hierarchy(float width, float height) {
    using namespace view;

    g_root_view = std::make_unique<Panel>();
    g_root_view->set_bounds({0, 0, width, height});
    g_root_view->flex().padding = 20;

    // Title label
    auto title = std::make_unique<Label>();
    title->set_text("Pulp Audio Engine");
    title->set_font_size(28.0f);
    title->flex().preferred_height = 60;
    title->flex().margin_top = 40;
    g_root_view->add_child(std::move(title));

    // Subtitle
    auto subtitle = std::make_unique<Label>();
    subtitle->set_text("Android · Vulkan · Skia Graphite");
    subtitle->set_font_size(14.0f);
    subtitle->flex().preferred_height = 30;
    g_root_view->add_child(std::move(subtitle));

    // Knob row
    auto knob_row = std::make_unique<Panel>();
    knob_row->flex().direction = FlexDirection::row;
    knob_row->flex().preferred_height = 120;
    knob_row->flex().margin_top = 30;
    knob_row->flex().justify_content = FlexJustify::space_evenly;

    for (int i = 0; i < 3; ++i) {
        auto knob = std::make_unique<Knob>();
        knob->set_value(0.3f + i * 0.2f);
        knob->flex().preferred_width = 80;
        knob->flex().preferred_height = 80;
        knob_row->add_child(std::move(knob));
    }
    g_root_view->add_child(std::move(knob_row));

    // Fader
    auto fader = std::make_unique<Fader>();
    fader->set_value(0.7f);
    fader->flex().preferred_height = 40;
    fader->flex().margin_top = 20;
    fader->flex().margin_left = 20;
    fader->flex().margin_right = 20;
    g_root_view->add_child(std::move(fader));

    // Meter
    auto meter = std::make_unique<Meter>();
    meter->set_level(-12.0f, -6.0f);
    meter->flex().preferred_height = 30;
    meter->flex().margin_top = 20;
    meter->flex().margin_left = 20;
    meter->flex().margin_right = 20;
    g_root_view->add_child(std::move(meter));

    // Bottom label
    auto bottom = std::make_unique<Label>();
    bottom->set_text("Dawn/Vulkan · 48kHz · 0 xruns");
    bottom->set_font_size(12.0f);
    bottom->flex().preferred_height = 30;
    bottom->flex().margin_top = 20;
    g_root_view->add_child(std::move(bottom));

    g_root_view->layout_children();
    PULP_LOGI("Android GPU surface: View hierarchy created (%d children)",
              static_cast<int>(g_root_view->child_count()));
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
        skia_config.scale_factor = 2.0f;  // typical Android density

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

        // Render an initial test frame
        android_render_test_frame();
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

        // Render a test frame — prove the full pipeline works
        android_render_test_frame();
    }
}

static float g_hue = 0.0f;

void android_render_test_frame() {
    if (!g_gpu_surface || !g_gpu_surface->is_initialized()) return;

    if (g_gpu_surface->begin_frame()) {
        if (g_skia_surface && g_skia_surface->is_available()) {
            auto* canvas = g_skia_surface->begin_frame();
            if (canvas) {
                float w = static_cast<float>(g_gpu_surface->width());
                float h = static_cast<float>(g_gpu_surface->height());

                using Color = canvas::Color;

                // Scale for display density (1080px / ~2.5 density ≈ 430dp)
                float scale = w / 430.0f;
                canvas->save();
                canvas->scale(scale, scale);
                float dw = 430.0f;  // draw in density-independent units
                float dh = h / scale;

                // Dark background
                canvas->set_fill_color(Color::rgba(26, 26, 46));
                canvas->fill_rect(0, 0, dw, dh);

                // Title bar
                canvas->set_fill_color(Color::rgba(108, 92, 231));
                canvas->fill_rect(0, 0, dw, 80);

                // Title text area
                canvas->set_fill_color(Color::rgba(255, 255, 255));
                canvas->fill_rect(20, 25, dw * 0.7f, 30);

                // 3 Knobs (circles approximated with rounded rects)
                float knob_y = 120;
                float knob_spacing = dw / 4;
                for (int i = 0; i < 3; ++i) {
                    float cx = knob_spacing * (i + 1);
                    // Outer ring
                    canvas->set_fill_color(Color::rgba(60, 60, 80));
                    canvas->fill_rect(cx - 40, knob_y, 80, 80);
                    // Inner
                    canvas->set_fill_color(Color::rgba(108, 92, 231));
                    canvas->fill_rect(cx - 30, knob_y + 10, 60, 60);
                    // Indicator
                    float angle = 0.3f + i * 0.25f;
                    canvas->set_fill_color(Color::rgba(255, 255, 255));
                    canvas->fill_rect(cx - 3, knob_y + 2, 6, 18);
                }

                // Knob labels
                float label_y = knob_y + 90;
                canvas->set_fill_color(Color::rgba(180, 180, 200));
                for (int i = 0; i < 3; ++i) {
                    float cx = knob_spacing * (i + 1);
                    canvas->fill_rect(cx - 25, label_y, 50, 14);
                }

                // Fader track
                float fader_y = label_y + 40;
                canvas->set_fill_color(Color::rgba(40, 40, 60));
                canvas->fill_rect(40, fader_y, dw - 80, 8);
                // Fader fill
                canvas->set_fill_color(Color::rgba(108, 92, 231));
                canvas->fill_rect(40, fader_y, (dw - 80) * 0.7f, 8);
                // Fader thumb
                float thumb_x = 40 + (dw - 80) * 0.7f;
                canvas->set_fill_color(Color::rgba(255, 255, 255));
                canvas->fill_rect(thumb_x - 8, fader_y - 8, 16, 24);

                // Meter
                float meter_y = fader_y + 40;
                canvas->set_fill_color(Color::rgba(30, 30, 50));
                canvas->fill_rect(40, meter_y, dw - 80, 20);
                // Green portion
                canvas->set_fill_color(Color::rgba(0, 200, 80));
                canvas->fill_rect(40, meter_y, (dw - 80) * 0.6f, 20);
                // Yellow portion
                canvas->set_fill_color(Color::rgba(255, 200, 0));
                canvas->fill_rect(40 + (dw - 80) * 0.6f, meter_y, (dw - 80) * 0.15f, 20);

                // Bottom status bar
                canvas->set_fill_color(Color::rgba(40, 40, 60));
                canvas->fill_rect(0, dh - 50, dw, 50);
                canvas->set_fill_color(Color::rgba(108, 92, 231));
                canvas->fill_rect(20, dh - 40, 12, 30);
                canvas->set_fill_color(Color::rgba(180, 180, 200));
                canvas->fill_rect(45, dh - 35, 200, 14);

                canvas->restore();

                g_skia_surface->end_frame();
                PULP_LOGI("Android GPU surface: Skia frame rendered (%dx%d)",
                          g_gpu_surface->width(), g_gpu_surface->height());
            }
        }
        g_gpu_surface->end_frame();
    }
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

// ── Touch Input ───────────────────────────────────────────────────────────

namespace pulp::input {
using TouchCallback = void(*)(int, float, float, float, int, void*);
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
