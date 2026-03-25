#include <pulp/render/gpu_surface.hpp>

#ifdef PULP_HAS_WEBGPU

#include <webgpu/webgpu.h>
#include <iostream>
#include <cstring>

namespace pulp::render {

class WgpuGpuSurface : public GpuSurface {
public:
    WgpuGpuSurface() = default;

    ~WgpuGpuSurface() override {
        if (device_) wgpuDeviceRelease(device_);
        if (adapter_) wgpuAdapterRelease(adapter_);
        if (instance_) wgpuInstanceRelease(instance_);
    }

    bool initialize(const Config& config) override {
        width_ = config.width;
        height_ = config.height;

        // Create WebGPU instance
        WGPUInstanceDescriptor instance_desc{};
        instance_ = wgpuCreateInstance(&instance_desc);
        if (!instance_) {
            std::cerr << "[pulp-render] Failed to create WebGPU instance\n";
            return false;
        }

        // Request adapter
        WGPURequestAdapterOptions adapter_opts{};
        adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

        WGPURequestAdapterCallbackInfo adapter_cb{};
        adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
        adapter_cb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                 WGPUStringView message, void* userdata1, void*) {
            auto* self = static_cast<WgpuGpuSurface*>(userdata1);
            if (status == WGPURequestAdapterStatus_Success) {
                self->adapter_ = adapter;
            } else {
                std::cerr << "[pulp-render] Adapter request failed";
                if (message.data && message.length > 0)
                    std::cerr << ": " << std::string(message.data, message.length);
                std::cerr << "\n";
            }
        };
        adapter_cb.userdata1 = this;

        wgpuInstanceRequestAdapter(instance_, &adapter_opts, adapter_cb);
        wgpuInstanceProcessEvents(instance_);

        if (!adapter_) {
            std::cerr << "[pulp-render] No suitable GPU adapter found\n";
            return false;
        }

        // Request device
        WGPUDeviceDescriptor device_desc{};
        WGPUStringView device_label{};
        device_label.data = "Pulp GPU Device";
        device_label.length = strlen(device_label.data);
        device_desc.label = device_label;

        // Error callback
        device_desc.uncapturedErrorCallbackInfo.callback =
            [](WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void*) {
                std::cerr << "[pulp-render] WebGPU error (" << type << ")";
                if (message.data && message.length > 0)
                    std::cerr << ": " << std::string(message.data, message.length);
                std::cerr << "\n";
            };

        WGPURequestDeviceCallbackInfo device_cb{};
        device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
        device_cb.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                WGPUStringView message, void* userdata1, void*) {
            auto* self = static_cast<WgpuGpuSurface*>(userdata1);
            if (status == WGPURequestDeviceStatus_Success) {
                self->device_ = device;
            } else {
                std::cerr << "[pulp-render] Device request failed";
                if (message.data && message.length > 0)
                    std::cerr << ": " << std::string(message.data, message.length);
                std::cerr << "\n";
            }
        };
        device_cb.userdata1 = this;

        wgpuAdapterRequestDevice(adapter_, &device_desc, device_cb);
        wgpuInstanceProcessEvents(instance_);

        if (!device_) {
            std::cerr << "[pulp-render] Failed to create GPU device\n";
            return false;
        }

        initialized_ = true;
        std::cout << "[pulp-render] WebGPU initialized successfully\n";
        return true;
    }

    void resize(uint32_t width, uint32_t height) override {
        width_ = width;
        height_ = height;
    }

    bool begin_frame() override {
        return initialized_;
    }

    void end_frame() override {
        if (instance_) wgpuInstanceProcessEvents(instance_);
    }

    bool is_initialized() const override { return initialized_; }
    uint32_t width() const override { return width_; }
    uint32_t height() const override { return height_; }

    // Expose internals for Skia integration
    WGPUDevice device() const { return device_; }
    WGPUInstance instance() const { return instance_; }

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

#else // !PULP_HAS_WEBGPU

namespace pulp::render {

std::unique_ptr<GpuSurface> GpuSurface::create_dawn() {
    return nullptr;
}

} // namespace pulp::render

#endif
