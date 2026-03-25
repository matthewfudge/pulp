#pragma once

// Headless processing adapter
// Wraps a Pulp Processor for programmatic use without UI or audio device.
// Use cases: CI validation, batch processing, golden-file tests, benchmarks.

#include <pulp/format/processor.hpp>

namespace pulp::format {

// Headless plugin host — load a plugin and process audio programmatically
class HeadlessHost {
public:
    explicit HeadlessHost(ProcessorFactory factory);

    // Prepare for processing (call before process())
    void prepare(double sample_rate, int max_buffer_size,
                 int input_channels = 2, int output_channels = 2);

    // Process a block of audio
    // Input and output can be the same buffer for in-place processing
    void process(audio::BufferView<float>& output,
                 const audio::BufferView<const float>& input);

    // Process with MIDI
    void process(audio::BufferView<float>& output,
                 const audio::BufferView<const float>& input,
                 midi::MidiBuffer& midi_in,
                 midi::MidiBuffer& midi_out);

    // Release resources
    void release();

    // Parameter access
    state::StateStore& state() { return store_; }
    const state::StateStore& state() const { return store_; }

    // Plugin info
    const PluginDescriptor& descriptor() const { return descriptor_; }

    // State serialization
    std::vector<uint8_t> save_state() const { return store_.serialize(); }
    bool load_state(std::span<const uint8_t> data) { return store_.deserialize(data); }

private:
    std::unique_ptr<Processor> processor_;
    state::StateStore store_;
    PluginDescriptor descriptor_;
    double sample_rate_ = 48000.0;
};

} // namespace pulp::format
