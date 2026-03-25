#pragma once

#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include <pulp/state/store.hpp>
#include <string>
#include <memory>
#include <vector>

namespace pulp::format {

// Plugin category
enum class PluginCategory {
    Effect,      // Audio effect (takes input, produces output)
    Instrument,  // Synth/sampler (receives MIDI, produces audio)
    MidiEffect,  // MIDI processor (receives MIDI, produces MIDI)
};

// Audio bus description — for multi-bus plugins (sidechain, aux, etc.)
struct BusInfo {
    std::string name;
    int default_channels = 2;
    bool optional = false;  // true for sidechain buses that can be deactivated
};

// Plugin metadata — declared once, immutable
struct PluginDescriptor {
    std::string name;
    std::string manufacturer;
    std::string bundle_id;
    std::string version;       // "1.0.0"
    PluginCategory category = PluginCategory::Effect;

    // Bus configuration — defaults to single stereo in/out for compatibility.
    // Override input_buses/output_buses for multi-bus (sidechain, aux).
    std::vector<BusInfo> input_buses = {{"Main In", 2, false}};
    std::vector<BusInfo> output_buses = {{"Main Out", 2, false}};

    bool accepts_midi = false;
    bool produces_midi = false;

    // Tail time in samples (0 = no tail, -1 = infinite)
    int tail_samples = 0;

    // Convenience: channel count of the first (main) bus
    int default_input_channels() const {
        return input_buses.empty() ? 0 : input_buses[0].default_channels;
    }
    int default_output_channels() const {
        return output_buses.empty() ? 0 : output_buses[0].default_channels;
    }
};

// Prepare context — passed once before processing starts
struct PrepareContext {
    double sample_rate = 48000.0;
    int max_buffer_size = 512;
    int input_channels = 2;
    int output_channels = 2;
};

// Process context — passed every audio callback
struct ProcessContext {
    double sample_rate = 0;
    int num_samples = 0;
    bool is_playing = false;
    bool is_recording = false;
    double tempo_bpm = 120.0;
    double position_beats = 0.0; // Position in quarter notes
    int64_t position_samples = 0;
    int time_sig_numerator = 4;
    int time_sig_denominator = 4;
};

// The plugin processor interface
// This is what plugin developers implement.
// Format adapters (VST3, AU, CLAP) wrap this interface.
class Processor {
public:
    virtual ~Processor() = default;

    // Metadata
    virtual PluginDescriptor descriptor() const = 0;

    // Parameter registration (called once during construction)
    // Override to add parameters to the store
    virtual void define_parameters(state::StateStore& store) = 0;

    // Lifecycle
    virtual void prepare(const PrepareContext& context) = 0;
    virtual void release() {}

    // Processing — called on the real-time audio thread
    // Main bus only — covers the vast majority of plugins.
    virtual void process(
        audio::BufferView<float>& audio_output,
        const audio::BufferView<const float>& audio_input,
        midi::MidiBuffer& midi_in,
        midi::MidiBuffer& midi_out,
        const ProcessContext& context) = 0;

    // State store access (set by the framework, not by the developer)
    state::StateStore& state() { return *state_store_; }
    const state::StateStore& state() const { return *state_store_; }

    // Framework sets this during initialization
    void set_state_store(state::StateStore* store) { state_store_ = store; }

private:
    state::StateStore* state_store_ = nullptr;
};

// Factory function type — plugins provide this
using ProcessorFactory = std::unique_ptr<Processor>(*)();

} // namespace pulp::format
