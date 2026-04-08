// audio_inspector.hpp — Audio domain monitoring for the inspector
// Buffer underrun detection, per-channel metering, MIDI event logging.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace pulp::inspect {

/// Audio configuration snapshot
struct AudioConfig {
    double sample_rate = 0;
    int buffer_size = 0;
    int input_channels = 0;
    int output_channels = 0;
    int latency_samples = 0;
};

/// A single MIDI event for logging
struct MidiLogEntry {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    std::chrono::steady_clock::time_point time;
    std::string description;  // e.g., "Note On C4 vel=100"
};

/// Buffer underrun event
struct BufferUnderrun {
    uint64_t frame;
    float callback_time_ms;
    float budget_ms;
    std::chrono::steady_clock::time_point time;
};

/// Per-channel audio levels
struct ChannelLevels {
    float peak = 0;
    float rms = 0;
};

/// Monitors audio thread performance and state for the inspector.
/// All methods are thread-safe (called from audio thread, read from UI thread).
class AudioInspector {
public:
    AudioInspector() = default;

    // ── Configuration ───────────────────────────────────────────────
    void set_config(const AudioConfig& config);
    AudioConfig config() const;

    // ── Callback timing (call from audio thread) ────────────────────
    /// Call at the start/end of each audio callback to measure duration.
    void begin_callback();
    void end_callback(uint64_t frame_number);

    // ── Metering (call from audio thread) ───────────────────────────
    /// Report output levels for the current buffer.
    void report_levels(const std::vector<ChannelLevels>& levels);

    // ── MIDI logging (call from audio thread) ───────────────────────
    void log_midi(uint8_t status, uint8_t data1, uint8_t data2,
                  const std::string& description = {});

    // ── Queries (call from any thread) ──────────────────────────────
    std::vector<BufferUnderrun> recent_underruns() const;
    std::vector<MidiLogEntry> recent_midi() const;
    std::vector<ChannelLevels> latest_levels() const;
    bool metering_enabled() const { return metering_enabled_; }
    void set_metering_enabled(bool enabled) { metering_enabled_ = enabled; }

private:
    mutable std::mutex mutex_;
    AudioConfig config_;
    bool metering_enabled_ = false;

    // Callback timing
    std::chrono::steady_clock::time_point callback_start_;

    // Underrun ring buffer
    std::vector<BufferUnderrun> underruns_;
    static constexpr size_t kMaxUnderruns = 50;

    // MIDI ring buffer
    std::vector<MidiLogEntry> midi_log_;
    static constexpr size_t kMaxMidi = 200;

    // Latest levels
    std::vector<ChannelLevels> levels_;
};

} // namespace pulp::inspect
