#include <pulp/midi/device.hpp>
#include <pulp/runtime/log.hpp>

#ifndef _WIN32
#error "winmidi_device.cpp is Windows-only"
#endif

#include <pulp/platform/win32_sane.hpp>
#include <mmeapi.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace pulp::midi::win {

// ── Helpers ──────────────────────────────────────────────────────────────

static std::string wide_to_utf8(const wchar_t* wide) {
    if (!wide || !wide[0]) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len - 1, '\0');  // len includes null terminator
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), len, nullptr, nullptr);
    return result;
}

// ── WinMidiInput ─────────────────────────────────────────────────────────

class WinMidiInput : public MidiInput {
public:
    ~WinMidiInput() override { close(); }

    bool open(const std::string& port_id, MidiInputCallback callback) override {
        callback_ = std::move(callback);

        UINT device_id = 0;
        if (!port_id.empty()) {
            device_id = static_cast<UINT>(std::stoul(port_id));
        }

        UINT num_devs = midiInGetNumDevs();
        if (num_devs == 0 || device_id >= num_devs) {
            runtime::log_error("WinMIDI: no input devices available (requested {})", device_id);
            return false;
        }

        MMRESULT result = midiInOpen(&handle_, device_id,
            reinterpret_cast<DWORD_PTR>(midi_in_callback),
            reinterpret_cast<DWORD_PTR>(this),
            CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR) {
            runtime::log_error("WinMIDI: could not open input device {} (error {})", device_id, result);
            return false;
        }

        result = midiInStart(handle_);
        if (result != MMSYSERR_NOERROR) {
            runtime::log_error("WinMIDI: could not start input (error {})", result);
            midiInClose(handle_);
            handle_ = nullptr;
            return false;
        }

        is_open_ = true;
        return true;
    }

    void close() override {
        if (handle_) {
            midiInStop(handle_);
            midiInReset(handle_);
            midiInClose(handle_);
            handle_ = nullptr;
        }
        is_open_ = false;
    }

    bool is_open() const override { return is_open_; }

private:
    static void CALLBACK midi_in_callback(
        HMIDIIN, UINT msg, DWORD_PTR instance,
        DWORD_PTR param1, DWORD_PTR param2)
    {
        if (msg != MIM_DATA) return;

        auto* self = reinterpret_cast<WinMidiInput*>(instance);
        if (!self->callback_) return;

        // param1 contains the MIDI message packed into a DWORD:
        // low byte = status, next byte = data1, next byte = data2
        auto status = static_cast<uint8_t>(param1 & 0xFF);
        auto data1 = static_cast<uint8_t>((param1 >> 8) & 0xFF);
        auto data2 = static_cast<uint8_t>((param1 >> 16) & 0xFF);

        // param2 is the timestamp in milliseconds
        MidiEvent evt;
        evt.message = choc::midi::ShortMessage(status, data1, data2);
        evt.timestamp = static_cast<double>(param2) / 1000.0;

        self->callback_(evt);
    }

    HMIDIIN handle_ = nullptr;
    MidiInputCallback callback_;
    bool is_open_ = false;
};

// ── WinMidiOutput ────────────────────────────────────────────────────────

class WinMidiOutput : public MidiOutput {
public:
    ~WinMidiOutput() override { close(); }

    bool open(const std::string& port_id) override {
        UINT device_id = 0;
        if (!port_id.empty()) {
            device_id = static_cast<UINT>(std::stoul(port_id));
        }

        UINT num_devs = midiOutGetNumDevs();
        if (num_devs == 0 || device_id >= num_devs) {
            runtime::log_error("WinMIDI: no output devices available (requested {})", device_id);
            return false;
        }

        MMRESULT result = midiOutOpen(&handle_, device_id,
            0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            runtime::log_error("WinMIDI: could not open output device {} (error {})", device_id, result);
            return false;
        }

        is_open_ = true;
        return true;
    }

    void close() override {
        if (handle_) {
            midiOutReset(handle_);
            midiOutClose(handle_);
            handle_ = nullptr;
        }
        is_open_ = false;
    }

    bool is_open() const override { return is_open_; }

    void send(const MidiEvent& event) override {
        if (!handle_) return;

        const auto* d = event.data();
        // Pack the MIDI message into a DWORD (little-endian: status | data1<<8 | data2<<16)
        DWORD msg = static_cast<DWORD>(d[0])
                  | (static_cast<DWORD>(d[1]) << 8)
                  | (static_cast<DWORD>(d[2]) << 16);

        midiOutShortMsg(handle_, msg);
    }

private:
    HMIDIOUT handle_ = nullptr;
    bool is_open_ = false;
};

// ── WinMidiSystem ────────────────────────────────────────────────────────

class WinMidiSystem : public MidiSystem {
public:
    std::vector<MidiPortInfo> enumerate_inputs() override {
        std::vector<MidiPortInfo> ports;
        UINT count = midiInGetNumDevs();
        for (UINT i = 0; i < count; ++i) {
            MIDIINCAPSW caps{};
            if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                MidiPortInfo info;
                info.id = std::to_string(i);
                info.name = wide_to_utf8(caps.szPname);
                info.is_input = true;
                ports.push_back(std::move(info));
            }
        }
        return ports;
    }

    std::vector<MidiPortInfo> enumerate_outputs() override {
        std::vector<MidiPortInfo> ports;
        UINT count = midiOutGetNumDevs();
        for (UINT i = 0; i < count; ++i) {
            MIDIOUTCAPSW caps{};
            if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                MidiPortInfo info;
                info.id = std::to_string(i);
                info.name = wide_to_utf8(caps.szPname);
                info.is_output = true;
                ports.push_back(std::move(info));
            }
        }
        return ports;
    }

    std::unique_ptr<MidiInput> create_input() override {
        return std::make_unique<WinMidiInput>();
    }

    std::unique_ptr<MidiOutput> create_output() override {
        return std::make_unique<WinMidiOutput>();
    }
};

} // namespace pulp::midi::win

// Factory function
namespace pulp::midi {

std::unique_ptr<MidiSystem> create_midi_system() {
    return std::make_unique<win::WinMidiSystem>();
}

} // namespace pulp::midi
