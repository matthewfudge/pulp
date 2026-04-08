// audio_inspector.cpp — Audio domain monitoring

#include <pulp/inspect/audio_inspector.hpp>

namespace pulp::inspect {

void AudioInspector::set_config(const AudioConfig& config) {
    std::lock_guard lock(mutex_);
    config_ = config;
}

AudioConfig AudioInspector::config() const {
    std::lock_guard lock(mutex_);
    return config_;
}

void AudioInspector::begin_callback() {
    callback_start_ = std::chrono::steady_clock::now();
}

void AudioInspector::end_callback(uint64_t frame_number) {
    auto elapsed = std::chrono::steady_clock::now() - callback_start_;
    float elapsed_ms = std::chrono::duration<float, std::milli>(elapsed).count();

    float budget_ms = 0;
    {
        std::lock_guard lock(mutex_);
        if (config_.sample_rate > 0 && config_.buffer_size > 0)
            budget_ms = static_cast<float>(config_.buffer_size) / static_cast<float>(config_.sample_rate) * 1000.0f;
    }

    if (budget_ms > 0 && elapsed_ms > budget_ms) {
        BufferUnderrun underrun;
        underrun.frame = frame_number;
        underrun.callback_time_ms = elapsed_ms;
        underrun.budget_ms = budget_ms;
        underrun.time = std::chrono::steady_clock::now();

        std::lock_guard lock(mutex_);
        underruns_.push_back(underrun);
        if (underruns_.size() > kMaxUnderruns)
            underruns_.erase(underruns_.begin());
    }
}

void AudioInspector::report_levels(const std::vector<ChannelLevels>& levels) {
    if (!metering_enabled_) return;
    std::lock_guard lock(mutex_);
    levels_ = levels;
}

void AudioInspector::log_midi(uint8_t status, uint8_t data1, uint8_t data2,
                               const std::string& description) {
    MidiLogEntry entry;
    entry.status = status;
    entry.data1 = data1;
    entry.data2 = data2;
    entry.time = std::chrono::steady_clock::now();
    entry.description = description;

    std::lock_guard lock(mutex_);
    midi_log_.push_back(std::move(entry));
    if (midi_log_.size() > kMaxMidi)
        midi_log_.erase(midi_log_.begin());
}

std::vector<BufferUnderrun> AudioInspector::recent_underruns() const {
    std::lock_guard lock(mutex_);
    return underruns_;
}

std::vector<MidiLogEntry> AudioInspector::recent_midi() const {
    std::lock_guard lock(mutex_);
    return midi_log_;
}

std::vector<ChannelLevels> AudioInspector::latest_levels() const {
    std::lock_guard lock(mutex_);
    return levels_;
}

} // namespace pulp::inspect
