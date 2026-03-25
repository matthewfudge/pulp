// Headless processing adapter implementation
// Simplest possible host — no threading, no UI, no device I/O

#include <pulp/format/headless.hpp>

namespace pulp::format {

HeadlessHost::HeadlessHost(ProcessorFactory factory) {
    processor_ = factory();
    if (processor_) {
        processor_->set_state_store(&store_);
        processor_->define_parameters(store_);
        descriptor_ = processor_->descriptor();
    }
}

void HeadlessHost::prepare(double sample_rate, int max_buffer_size,
                            int input_channels, int output_channels) {
    if (!processor_) return;

    sample_rate_ = sample_rate;
    PrepareContext ctx;
    ctx.sample_rate = sample_rate;
    ctx.max_buffer_size = max_buffer_size;
    ctx.input_channels = input_channels;
    ctx.output_channels = output_channels;
    processor_->prepare(ctx);
}

void HeadlessHost::process(audio::BufferView<float>& output,
                            const audio::BufferView<const float>& input) {
    midi::MidiBuffer midi_in, midi_out;
    process(output, input, midi_in, midi_out);
}

void HeadlessHost::process(audio::BufferView<float>& output,
                            const audio::BufferView<const float>& input,
                            midi::MidiBuffer& midi_in,
                            midi::MidiBuffer& midi_out) {
    if (!processor_) return;

    ProcessContext ctx;
    ctx.sample_rate = sample_rate_;
    ctx.num_samples = static_cast<int>(output.num_samples());
    processor_->process(output, input, midi_in, midi_out, ctx);
}

void HeadlessHost::release() {
    if (processor_) processor_->release();
}

} // namespace pulp::format
