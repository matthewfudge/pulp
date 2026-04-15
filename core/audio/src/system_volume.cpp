#include <pulp/audio/system_volume.hpp>
#include <pulp/platform/child_process.hpp>

#ifdef __APPLE__
#include <TargetConditionals.h>
#if !(TARGET_OS_IPHONE || TARGET_OS_TV || TARGET_OS_WATCH)
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#define PULP_HAS_COREAUDIO_DEVICE 1
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <combaseapi.h>
#endif

#ifdef __linux__
#include <cstdlib>
#include <cstdio>
#include <cstring>
#endif

namespace pulp::audio {

#ifdef PULP_HAS_COREAUDIO_DEVICE

static AudioDeviceID get_default_output_device() {
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device);
    return device;
}

std::optional<float> get_system_volume() {
    AudioDeviceID device = get_default_output_device();
    if (device == kAudioObjectUnknown) return std::nullopt;

    Float32 volume = 0;
    UInt32 size = sizeof(volume);
    AudioObjectPropertyAddress addr = {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &volume);
    if (status != noErr) return std::nullopt;
    return volume;
}

bool set_system_volume(float volume) {
    AudioDeviceID device = get_default_output_device();
    if (device == kAudioObjectUnknown) return false;

    Float32 vol = volume;
    AudioObjectPropertyAddress addr = {
        kAudioHardwareServiceDeviceProperty_VirtualMainVolume,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    return AudioObjectSetPropertyData(device, &addr, 0, nullptr, sizeof(vol), &vol) == noErr;
}

std::optional<bool> is_system_muted() {
    AudioDeviceID device = get_default_output_device();
    if (device == kAudioObjectUnknown) return std::nullopt;

    UInt32 muted = 0;
    UInt32 size = sizeof(muted);
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &muted);
    if (status != noErr) return std::nullopt;
    return muted != 0;
}

bool set_system_muted(bool muted) {
    AudioDeviceID device = get_default_output_device();
    if (device == kAudioObjectUnknown) return false;

    UInt32 val = muted ? 1 : 0;
    AudioObjectPropertyAddress addr = {
        kAudioDevicePropertyMute,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    return AudioObjectSetPropertyData(device, &addr, 0, nullptr, sizeof(val), &val) == noErr;
}

#elif defined(_WIN32)

std::optional<float> get_system_volume() {
    CoInitialize(nullptr);

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* volume = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return std::nullopt;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { enumerator->Release(); return std::nullopt; }

    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&volume);
    if (FAILED(hr)) { device->Release(); enumerator->Release(); return std::nullopt; }

    float level = 0;
    volume->GetMasterVolumeLevelScalar(&level);

    volume->Release();
    device->Release();
    enumerator->Release();
    return level;
}

bool set_system_volume(float vol) {
    CoInitialize(nullptr);

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* volume = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { enumerator->Release(); return false; }

    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&volume);
    if (FAILED(hr)) { device->Release(); enumerator->Release(); return false; }

    hr = volume->SetMasterVolumeLevelScalar(vol, nullptr);

    volume->Release();
    device->Release();
    enumerator->Release();
    return SUCCEEDED(hr);
}

std::optional<bool> is_system_muted() {
    // Simplified — full implementation would use IAudioEndpointVolume::GetMute
    return std::nullopt;
}

bool set_system_muted(bool) { return false; }

#elif defined(__linux__)

// Linux: use amixer (ALSA) command-line tool
std::optional<float> get_system_volume() {
    auto r = pulp::platform::exec("/bin/sh", {"-c",
        "amixer sget Master 2>/dev/null | grep -oP '\\[\\d+%\\]' | head -1 | tr -d '[]%'"}, 5000);
    if (r.exit_code != 0 || r.stdout_output.empty()) return std::nullopt;
    int pct = std::atoi(r.stdout_output.c_str());
    return static_cast<float>(pct) / 100.0f;
}

bool set_system_volume(float volume) {
    int pct = static_cast<int>(volume * 100.0f);
    std::string cmd = "amixer sset Master " + std::to_string(pct) + "% 2>/dev/null";
    auto r = pulp::platform::exec("/bin/sh", {"-c", cmd}, 5000);
    return r.exit_code == 0;
}

std::optional<bool> is_system_muted() { return std::nullopt; }
bool set_system_muted(bool) { return false; }

#else

std::optional<float> get_system_volume() { return std::nullopt; }
bool set_system_volume(float) { return false; }
std::optional<bool> is_system_muted() { return std::nullopt; }
bool set_system_muted(bool) { return false; }

#endif

}  // namespace pulp::audio
