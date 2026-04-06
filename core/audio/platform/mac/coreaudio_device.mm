#include "coreaudio_device.hpp"
#include <pulp/runtime/log.hpp>
#include <CoreAudio/CoreAudio.h>

namespace pulp::audio::mac {

// ── CoreAudioDevice ────────────────────────────────────────────────────────

CoreAudioDevice::CoreAudioDevice(AudioDeviceID device_id)
    : device_id_(device_id)
{
}

CoreAudioDevice::~CoreAudioDevice() {
    if (is_running_) stop();
    if (is_open_) close();
}

bool CoreAudioDevice::open(const DeviceConfig& config) {
    config_ = config;

    // Create output audio unit (AUHAL)
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    auto component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        runtime::log_error("CoreAudio: could not find HAL output component");
        return false;
    }

    auto status = AudioComponentInstanceNew(component, &audio_unit_);
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not create audio unit ({})", static_cast<int>(status));
        return false;
    }

    // Set the device
    status = AudioUnitSetProperty(audio_unit_,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global, 0,
        &device_id_, sizeof(device_id_));
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not set device ({})", static_cast<int>(status));
        close();
        return false;
    }

    // Enable input on bus 1 if input channels are requested
    input_enabled_ = false;
    if (config_.input_channels > 0) {
        UInt32 enable_input = 1;
        status = AudioUnitSetProperty(audio_unit_,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input, 1,
            &enable_input, sizeof(enable_input));
        if (status != noErr) {
            runtime::log_warn("CoreAudio: could not enable input ({})", static_cast<int>(status));
            // Continue without input — effects will receive silence
        } else {
            input_enabled_ = true;
        }
    }

    // Set output stream format (bus 0, input scope = what we provide to the device)
    AudioStreamBasicDescription stream_desc{};
    stream_desc.mSampleRate = config_.sample_rate;
    stream_desc.mFormatID = kAudioFormatLinearPCM;
    stream_desc.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved;
    stream_desc.mBitsPerChannel = 32;
    stream_desc.mChannelsPerFrame = static_cast<UInt32>(config_.output_channels);
    stream_desc.mFramesPerPacket = 1;
    stream_desc.mBytesPerFrame = sizeof(float);
    stream_desc.mBytesPerPacket = sizeof(float);

    status = AudioUnitSetProperty(audio_unit_,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input, 0,
        &stream_desc, sizeof(stream_desc));
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not set output stream format ({})", static_cast<int>(status));
        close();
        return false;
    }

    // Set input stream format (bus 1, output scope = what we receive from the device)
    if (input_enabled_) {
        AudioStreamBasicDescription input_desc = stream_desc;
        input_desc.mChannelsPerFrame = static_cast<UInt32>(config_.input_channels);

        status = AudioUnitSetProperty(audio_unit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output, 1,
            &input_desc, sizeof(input_desc));
        if (status != noErr) {
            runtime::log_warn("CoreAudio: could not set input stream format ({})", static_cast<int>(status));
            input_enabled_ = false;
        }
    }

    // Pre-allocate input capture buffers (no allocation in audio callback)
    if (input_enabled_) {
        auto in_ch = static_cast<UInt32>(config_.input_channels);
        auto buf_frames = static_cast<UInt32>(config_.buffer_size);

        input_buffer_storage_.resize(in_ch * buf_frames, 0.0f);
        input_ptrs_.resize(in_ch);

        // Build an AudioBufferList for AudioUnitRender
        input_buffer_list_size_ = offsetof(AudioBufferList, mBuffers) + in_ch * sizeof(AudioBuffer);
        input_buffer_list_ = static_cast<AudioBufferList*>(std::malloc(input_buffer_list_size_));
        input_buffer_list_->mNumberBuffers = in_ch;
        for (UInt32 c = 0; c < in_ch; ++c) {
            input_buffer_list_->mBuffers[c].mNumberChannels = 1;
            input_buffer_list_->mBuffers[c].mDataByteSize = buf_frames * sizeof(float);
            input_buffer_list_->mBuffers[c].mData = input_buffer_storage_.data() + c * buf_frames;
            input_ptrs_[c] = static_cast<float*>(input_buffer_list_->mBuffers[c].mData);
        }
    }

    // Set buffer size
    UInt32 buffer_size = static_cast<UInt32>(config_.buffer_size);
    status = AudioUnitSetProperty(audio_unit_,
        kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Global, 0,
        &buffer_size, sizeof(buffer_size));
    if (status != noErr) {
        runtime::log_warn("CoreAudio: could not set buffer size to {} ({})",
            config_.buffer_size, static_cast<int>(status));
    }

    // Set render callback
    AURenderCallbackStruct callback_struct{};
    callback_struct.inputProc = render_callback;
    callback_struct.inputProcRefCon = this;

    status = AudioUnitSetProperty(audio_unit_,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input, 0,
        &callback_struct, sizeof(callback_struct));
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not set render callback ({})", static_cast<int>(status));
        close();
        return false;
    }

    status = AudioUnitInitialize(audio_unit_);
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not initialize audio unit ({})", static_cast<int>(status));
        close();
        return false;
    }

    is_open_ = true;
    runtime::log_info("CoreAudio: opened device '{}' at {} Hz, buffer {}, input {}ch",
        info().name, config_.sample_rate, config_.buffer_size,
        input_enabled_ ? config_.input_channels : 0);
    return true;
}

void CoreAudioDevice::close() {
    if (audio_unit_) {
        AudioUnitUninitialize(audio_unit_);
        AudioComponentInstanceDispose(audio_unit_);
        audio_unit_ = nullptr;
    }
    if (input_buffer_list_) {
        std::free(input_buffer_list_);
        input_buffer_list_ = nullptr;
    }
    input_buffer_storage_.clear();
    input_ptrs_.clear();
    input_enabled_ = false;
    is_open_ = false;
}

bool CoreAudioDevice::start(AudioCallback callback) {
    if (!is_open_) return false;
    callback_ = std::move(callback);
    sample_position_ = 0;

    auto status = AudioOutputUnitStart(audio_unit_);
    if (status != noErr) {
        runtime::log_error("CoreAudio: could not start ({})", static_cast<int>(status));
        return false;
    }
    is_running_ = true;
    return true;
}

void CoreAudioDevice::stop() {
    if (audio_unit_ && is_running_) {
        AudioOutputUnitStop(audio_unit_);
    }
    is_running_ = false;
    callback_ = nullptr;
}

DeviceInfo CoreAudioDevice::info() const {
    return CoreAudioSystem::query_device_info(device_id_);
}

OSStatus CoreAudioDevice::render_callback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    auto* self = static_cast<CoreAudioDevice*>(inRefCon);
    if (!self->callback_) {
        // Silence
        for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
            std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
        }
        return noErr;
    }

    // Build output buffer view from CoreAudio's buffer list
    self->output_ptrs_.resize(ioData->mNumberBuffers);
    for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
        self->output_ptrs_[i] = static_cast<float*>(ioData->mBuffers[i].mData);
    }

    // Capture input from bus 1 if enabled
    BufferView<const float> input_view;
    if (self->input_enabled_ && self->input_buffer_list_) {
        // Reset buffer sizes in case CoreAudio changed them
        auto in_ch = static_cast<UInt32>(self->config_.input_channels);
        for (UInt32 c = 0; c < in_ch; ++c) {
            self->input_buffer_list_->mBuffers[c].mDataByteSize = inNumberFrames * sizeof(float);
        }

        OSStatus input_status = AudioUnitRender(
            self->audio_unit_, ioActionFlags, inTimeStamp,
            1,  // Bus 1 = input
            inNumberFrames,
            self->input_buffer_list_);

        if (input_status == noErr) {
            input_view = BufferView<const float>(
                const_cast<const float**>(self->input_ptrs_.data()),
                self->input_ptrs_.size(), inNumberFrames);
        }
        // On error, input_view remains empty (silence) — don't crash
    }

    BufferView<float> output_view(self->output_ptrs_.data(),
        self->output_ptrs_.size(), inNumberFrames);

    CallbackContext ctx;
    ctx.sample_rate = self->config_.sample_rate;
    ctx.buffer_size = static_cast<int>(inNumberFrames);
    ctx.sample_position = self->sample_position_;

    self->callback_(input_view, output_view, ctx);
    self->sample_position_ += inNumberFrames;

    return noErr;
}

// ── CoreAudioSystem ────────────────────────────────────────────────────────

AudioDeviceID CoreAudioSystem::get_default_device(bool input) {
    AudioObjectPropertyAddress prop{};
    prop.mSelector = input ? kAudioHardwarePropertyDefaultInputDevice
                           : kAudioHardwarePropertyDefaultOutputDevice;
    prop.mScope = kAudioObjectPropertyScopeGlobal;
    prop.mElement = kAudioObjectPropertyElementMain;

    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, 0, nullptr, &size, &device_id);
    return device_id;
}

DeviceInfo CoreAudioSystem::query_device_info(AudioDeviceID device_id) {
    DeviceInfo info;
    info.id = std::to_string(device_id);

    // Get name
    AudioObjectPropertyAddress prop{};
    prop.mSelector = kAudioObjectPropertyName;
    prop.mScope = kAudioObjectPropertyScopeGlobal;
    prop.mElement = kAudioObjectPropertyElementMain;

    CFStringRef name_ref = nullptr;
    UInt32 size = sizeof(name_ref);
    if (AudioObjectGetPropertyData(device_id, &prop, 0, nullptr, &size, &name_ref) == noErr && name_ref) {
        char buf[256];
        CFStringGetCString(name_ref, buf, sizeof(buf), kCFStringEncodingUTF8);
        info.name = buf;
        CFRelease(name_ref);
    }

    // Get output channel count
    prop.mSelector = kAudioDevicePropertyStreamConfiguration;
    prop.mScope = kAudioObjectPropertyScopeOutput;
    size = 0;
    AudioObjectGetPropertyDataSize(device_id, &prop, 0, nullptr, &size);
    if (size > 0) {
        std::vector<uint8_t> buf(size);
        auto* list = reinterpret_cast<AudioBufferList*>(buf.data());
        if (AudioObjectGetPropertyData(device_id, &prop, 0, nullptr, &size, list) == noErr) {
            for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
                info.max_output_channels += static_cast<int>(list->mBuffers[i].mNumberChannels);
            }
        }
    }

    // Get input channel count
    prop.mScope = kAudioObjectPropertyScopeInput;
    size = 0;
    AudioObjectGetPropertyDataSize(device_id, &prop, 0, nullptr, &size);
    if (size > 0) {
        std::vector<uint8_t> buf(size);
        auto* list = reinterpret_cast<AudioBufferList*>(buf.data());
        if (AudioObjectGetPropertyData(device_id, &prop, 0, nullptr, &size, list) == noErr) {
            for (UInt32 i = 0; i < list->mNumberBuffers; ++i) {
                info.max_input_channels += static_cast<int>(list->mBuffers[i].mNumberChannels);
            }
        }
    }

    // Check if default
    info.is_default_output = (device_id == get_default_device(false));
    info.is_default_input = (device_id == get_default_device(true));

    // Get supported sample rates
    prop.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
    prop.mScope = kAudioObjectPropertyScopeGlobal;
    size = 0;
    AudioObjectGetPropertyDataSize(device_id, &prop, 0, nullptr, &size);
    if (size > 0) {
        auto count = size / sizeof(AudioValueRange);
        std::vector<AudioValueRange> ranges(count);
        if (AudioObjectGetPropertyData(device_id, &prop, 0, nullptr, &size, ranges.data()) == noErr) {
            for (const auto& r : ranges) {
                if (r.mMinimum == r.mMaximum) {
                    info.sample_rates.push_back(r.mMinimum);
                }
            }
        }
    }

    return info;
}

std::vector<DeviceInfo> CoreAudioSystem::enumerate_devices() {
    AudioObjectPropertyAddress prop{};
    prop.mSelector = kAudioHardwarePropertyDevices;
    prop.mScope = kAudioObjectPropertyScopeGlobal;
    prop.mElement = kAudioObjectPropertyElementMain;

    UInt32 size = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &prop, 0, nullptr, &size);

    auto count = size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> device_ids(count);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, 0, nullptr, &size, device_ids.data());

    std::vector<DeviceInfo> devices;
    devices.reserve(count);
    for (auto id : device_ids) {
        devices.push_back(query_device_info(id));
    }
    return devices;
}

std::unique_ptr<AudioDevice> CoreAudioSystem::create_device(const std::string& device_id) {
    AudioDeviceID id;
    if (device_id.empty()) {
        id = get_default_device(false);
    } else {
        id = static_cast<AudioDeviceID>(std::stoul(device_id));
    }
    return std::make_unique<CoreAudioDevice>(id);
}

DeviceInfo CoreAudioSystem::default_output_device() {
    return query_device_info(get_default_device(false));
}

DeviceInfo CoreAudioSystem::default_input_device() {
    return query_device_info(get_default_device(true));
}

} // namespace pulp::audio::mac

// Factory function
namespace pulp::audio {

std::unique_ptr<AudioSystem> create_audio_system() {
    return std::make_unique<mac::CoreAudioSystem>();
}

} // namespace pulp::audio
