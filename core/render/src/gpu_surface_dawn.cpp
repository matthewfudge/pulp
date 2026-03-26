#include <pulp/render/gpu_surface.hpp>

#ifdef PULP_HAS_SKIA
// When Skia is available, we use Dawn's C++ API (which Skia requires).
// This ensures a single Dawn device can be shared with SkiaSurface.

#include <pulp/runtime/log.hpp>
#include "webgpu/webgpu_cpp.h"
#include "dawn/native/DawnNative.h"
#include "dawn/dawn_proc.h"

namespace pulp::render {

class DawnGpuSurface : public GpuSurface {
public:
    ~DawnGpuSurface() override {
        // Release in reverse order
        current_texture_ = nullptr;
        surface_ = nullptr;
        queue_ = nullptr;
        device_ = nullptr;
        instance_ = nullptr;
    }

    bool initialize(const Config& config) override {
        width_ = config.width;
        height_ = config.height;

        // Install Dawn native procs
        const DawnProcTable& procs = dawn::native::GetProcs();
        dawnProcSetProcs(&procs);

        // Create instance
        wgpu::InstanceDescriptor instance_desc{};
        instance_ = wgpu::CreateInstance(&instance_desc);
        if (!instance_) {
            runtime::log_error("GpuSurface: failed to create Dawn instance");
            return false;
        }

        // Create native surface from platform layer (if provided)
        if (config.native_layer) {
            create_native_surface(config.native_layer);
        }

        // Request adapter (compatible with surface if we have one)
        wgpu::RequestAdapterOptions adapter_opts{};
        adapter_opts.powerPreference = wgpu::PowerPreference::HighPerformance;
        if (surface_) {
            adapter_opts.compatibleSurface = surface_;
        }

        instance_.RequestAdapter(
            &adapter_opts,
            wgpu::CallbackMode::WaitAnyOnly,
            [this](wgpu::RequestAdapterStatus status, wgpu::Adapter result, wgpu::StringView) {
                if (status == wgpu::RequestAdapterStatus::Success) {
                    adapter_ = std::move(result);
                }
            });
        instance_.ProcessEvents();

        if (!adapter_) {
            runtime::log_error("GpuSurface: no suitable GPU adapter found");
            return false;
        }

        // Request device
        wgpu::DeviceDescriptor device_desc{};
        device_desc.label = "Pulp GPU Device";
        device_desc.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
                runtime::log_error("GpuSurface: WebGPU error ({}): {}",
                    static_cast<int>(type),
                    std::string(message.data, message.length));
            });

        adapter_.RequestDevice(
            &device_desc,
            wgpu::CallbackMode::WaitAnyOnly,
            [this](wgpu::RequestDeviceStatus status, wgpu::Device result, wgpu::StringView) {
                if (status == wgpu::RequestDeviceStatus::Success) {
                    device_ = std::move(result);
                }
            });
        instance_.ProcessEvents();

        if (!device_) {
            runtime::log_error("GpuSurface: failed to create GPU device");
            return false;
        }

        queue_ = device_.GetQueue();

        // Configure the surface for presentation
        if (surface_) {
            configure_surface(config);
        }

        initialized_ = true;
        runtime::log_info("GpuSurface: Dawn initialized (surface: {})",
            surface_ ? "presentable" : "offscreen-only");
        return true;
    }

    void resize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
        if (surface_ && device_) {
            // Reconfigure with the format/mode selected during initial configure
            wgpu::SurfaceConfiguration surface_config{};
            surface_config.device = device_;
            surface_config.format = preferred_format_;
            surface_config.width = width_;
            surface_config.height = height_;
            surface_config.presentMode = preferred_mode_;
            surface_config.usage = wgpu::TextureUsage::RenderAttachment;
            surface_.Configure(&surface_config);
        }
    }

    bool begin_frame() override {
        if (!initialized_) return false;

        if (surface_) {
            wgpu::SurfaceTexture surface_texture;
            surface_.GetCurrentTexture(&surface_texture);

            if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
                surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
                current_texture_ = nullptr;
                return false;
            }

            current_texture_ = std::move(surface_texture.texture);
            return current_texture_ != nullptr;
        }

        return true;  // offscreen always ready
    }

    void end_frame() override {
        if (surface_ && current_texture_) {
            surface_.Present();
            current_texture_ = nullptr;  // texture invalid after present
        }
        if (instance_) {
            instance_.ProcessEvents();
        }
    }

    bool is_initialized() const override { return initialized_; }
    bool has_surface() const override { return surface_ != nullptr; }
    uint32_t width() const override { return width_; }
    uint32_t height() const override { return height_; }

    // Dawn handle accessors — SkiaSurface casts these back to Dawn C++ types
    void* dawn_device_handle() const override {
        return const_cast<wgpu::Device*>(&device_);
    }
    void* dawn_queue_handle() const override {
        return const_cast<wgpu::Queue*>(&queue_);
    }
    void* dawn_instance_handle() const override {
        return const_cast<wgpu::Instance*>(&instance_);
    }
    void* current_texture_handle() const override {
        return const_cast<wgpu::Texture*>(&current_texture_);
    }

private:
    void create_native_surface(void* native_layer) {
#ifdef __APPLE__
        wgpu::SurfaceDescriptor surface_desc{};
        wgpu::SurfaceSourceMetalLayer metal_source{};
        metal_source.layer = native_layer;
        surface_desc.nextInChain = &metal_source;

        surface_ = instance_.CreateSurface(&surface_desc);
        if (surface_) {
            runtime::log_info("GpuSurface: created Metal surface from CAMetalLayer");
        } else {
            runtime::log_warn("GpuSurface: failed to create Metal surface");
        }
#else
        (void)native_layer;
#endif
    }

    void configure_surface(const Config& config) {
        // Query surface capabilities — do not assume format or present mode
        wgpu::SurfaceCapabilities caps;
        surface_.GetCapabilities(adapter_, &caps);

        // Select preferred format (BGRA8Unorm if available, else first supported)
        preferred_format_ = wgpu::TextureFormat::BGRA8Unorm;
        if (caps.formatCount > 0) {
            preferred_format_ = caps.formats[0]; // first is preferred
            for (size_t i = 0; i < caps.formatCount; ++i) {
                if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm) {
                    preferred_format_ = wgpu::TextureFormat::BGRA8Unorm;
                    break;
                }
            }
        }

        // Select present mode (Fifo/vsync if available, else first supported)
        wgpu::PresentMode preferred_mode = config.vsync
            ? wgpu::PresentMode::Fifo : wgpu::PresentMode::Immediate;
        bool mode_found = false;
        for (size_t i = 0; i < caps.presentModeCount; ++i) {
            if (caps.presentModes[i] == preferred_mode) {
                mode_found = true;
                break;
            }
        }
        if (!mode_found && caps.presentModeCount > 0) {
            preferred_mode = caps.presentModes[0];
        }
        preferred_mode_ = preferred_mode;

        wgpu::SurfaceConfiguration surface_config{};
        surface_config.device = device_;
        surface_config.format = preferred_format_;
        surface_config.width = config.width;
        surface_config.height = config.height;
        surface_config.presentMode = preferred_mode_;
        surface_config.usage = wgpu::TextureUsage::RenderAttachment;
        surface_.Configure(&surface_config);

        runtime::log_info("GpuSurface: configured surface (format: {}, mode: {})",
            static_cast<int>(preferred_format_), static_cast<int>(preferred_mode));
    }

    wgpu::Instance instance_;
    wgpu::Adapter adapter_;
    wgpu::Device device_;
    wgpu::Queue queue_;
    wgpu::Surface surface_;
    wgpu::Texture current_texture_;

    wgpu::TextureFormat preferred_format_ = wgpu::TextureFormat::BGRA8Unorm;
    wgpu::PresentMode preferred_mode_ = wgpu::PresentMode::Fifo;
    uint32_t width_ = 0, height_ = 0;
    bool initialized_ = false;
};

std::unique_ptr<GpuSurface> GpuSurface::create_dawn() {
    return std::make_unique<DawnGpuSurface>();
}

} // namespace pulp::render

#elif defined(PULP_HAS_WEBGPU)
// Fallback: wgpu-native C API (offscreen only, no Skia integration)

#include <webgpu/webgpu.h>
#include <pulp/runtime/log.hpp>
#include <cstring>

namespace pulp::render {

class WgpuGpuSurface : public GpuSurface {
public:
    ~WgpuGpuSurface() override {
        if (device_) wgpuDeviceRelease(device_);
        if (adapter_) wgpuAdapterRelease(adapter_);
        if (instance_) wgpuInstanceRelease(instance_);
    }

    bool initialize(const Config& config) override {
        width_ = config.width;
        height_ = config.height;

        WGPUInstanceDescriptor instance_desc{};
        instance_ = wgpuCreateInstance(&instance_desc);
        if (!instance_) return false;

        WGPURequestAdapterOptions adapter_opts{};
        adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

        WGPURequestAdapterCallbackInfo adapter_cb{};
        adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
        adapter_cb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                 WGPUStringView, void* ud1, void*) {
            if (status == WGPURequestAdapterStatus_Success)
                static_cast<WgpuGpuSurface*>(ud1)->adapter_ = adapter;
        };
        adapter_cb.userdata1 = this;
        wgpuInstanceRequestAdapter(instance_, &adapter_opts, adapter_cb);
        wgpuInstanceProcessEvents(instance_);
        if (!adapter_) return false;

        WGPUDeviceDescriptor device_desc{};
        WGPURequestDeviceCallbackInfo device_cb{};
        device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
        device_cb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                WGPUStringView, void* ud1, void*) {
            if (status == WGPURequestDeviceStatus_Success)
                static_cast<WgpuGpuSurface*>(ud1)->device_ = device;
        };
        device_cb.userdata1 = this;
        wgpuAdapterRequestDevice(adapter_, &device_desc, device_cb);
        wgpuInstanceProcessEvents(instance_);
        if (!device_) return false;

        initialized_ = true;
        return true;
    }

    void resize(uint32_t w, uint32_t h) override { width_ = w; height_ = h; }
    bool begin_frame() override { return initialized_; }
    void end_frame() override { if (instance_) wgpuInstanceProcessEvents(instance_); }
    bool is_initialized() const override { return initialized_; }
    bool has_surface() const override { return false; }
    uint32_t width() const override { return width_; }
    uint32_t height() const override { return height_; }
    void* dawn_device_handle() const override { return nullptr; }
    void* dawn_queue_handle() const override { return nullptr; }
    void* dawn_instance_handle() const override { return nullptr; }
    void* current_texture_handle() const override { return nullptr; }

private:
    WGPUInstance instance_ = nullptr;
    WGPUAdapter adapter_ = nullptr;
    WGPUDevice device_ = nullptr;
    uint32_t width_ = 0, height_ = 0;
    bool initialized_ = false;
};

std::unique_ptr<GpuSurface> GpuSurface::create_dawn() {
    return std::make_unique<WgpuGpuSurface>();
}

} // namespace pulp::render

#else // No GPU

namespace pulp::render {
std::unique_ptr<GpuSurface> GpuSurface::create_dawn() { return nullptr; }
} // namespace pulp::render

#endif
