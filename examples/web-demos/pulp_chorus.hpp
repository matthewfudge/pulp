#pragma once

// PulpChorus — Simple stereo chorus effect for web demo
// Uses two modulated delay lines with LFO for stereo width.
// Original implementation — not derived from any existing chorus code.

#include <pulp/format/processor.hpp>
#include <pulp/state/store.hpp>
#include <pulp/audio/buffer.hpp>
#include <pulp/midi/buffer.hpp>
#include <cmath>
#include <vector>

namespace pulp::examples {

enum ChorusParams { kChorusRate = 0, kChorusDepth = 1, kChorusMix = 2 };

class PulpChorus : public format::Processor {
public:
    format::PluginDescriptor descriptor() const override {
        return {
            .name = "PulpChorus",
            .manufacturer = "Pulp",
            .bundle_id = "com.pulp.chorus",
            .version = "1.0.0",
            .category = format::PluginCategory::Effect,
        };
    }

    void define_parameters(state::StateStore& store) override {
        store.add_parameter({kChorusRate, "Rate", "Hz", {0.1f, 5.0f, 0.8f, 0.01f}});
        store.add_parameter({kChorusDepth, "Depth", "ms", {0.5f, 10.0f, 3.0f, 0.1f}});
        store.add_parameter({kChorusMix, "Mix", "%", {0.0f, 100.0f, 50.0f, 1.0f}});
    }

    void prepare(const format::PrepareContext& ctx) override {
        sample_rate_ = ctx.sample_rate;
        // Max delay: 20ms at max sample rate
        int max_delay = static_cast<int>(sample_rate_ * 0.02) + 1;
        delay_l_.resize(max_delay, 0.0f);
        delay_r_.resize(max_delay, 0.0f);
        write_pos_ = 0;
        lfo_phase_ = 0.0;
    }

    void process(audio::BufferView<float>& output,
                 const audio::BufferView<const float>& input,
                 midi::MidiBuffer&, midi::MidiBuffer&,
                 const format::ProcessContext&) override {
        float rate = state().get_value(kChorusRate);
        float depth_ms = state().get_value(kChorusDepth);
        float mix = state().get_value(kChorusMix) / 100.0f;

        float depth_samples = depth_ms * 0.001f * static_cast<float>(sample_rate_);
        float center_delay = depth_samples + 1.0f; // offset so we never go negative
        double phase_inc = rate / sample_rate_;
        int buf_size = static_cast<int>(delay_l_.size());

        auto in_l = input.channel(0);
        auto in_r = (input.num_channels() > 1) ? input.channel(1) : in_l;
        auto out_l = output.channel(0);
        auto out_r = output.channel(output.num_channels() > 1 ? 1 : 0);

        for (size_t i = 0; i < out_l.size(); ++i) {
            float dry_l = in_l[i];
            float dry_r = (i < in_r.size()) ? in_r[i] : dry_l;

            // Write to delay buffer
            delay_l_[write_pos_] = dry_l;
            delay_r_[write_pos_] = dry_r;

            // LFO modulates delay time (L and R have opposite phase for stereo width)
            float lfo_l = static_cast<float>(std::sin(lfo_phase_ * 2.0 * M_PI));
            float lfo_r = static_cast<float>(std::sin((lfo_phase_ + 0.5) * 2.0 * M_PI));

            float delay_l = center_delay + lfo_l * depth_samples;
            float delay_r = center_delay + lfo_r * depth_samples;

            // Read from delay with linear interpolation
            float wet_l = read_delay(delay_l_, delay_l, buf_size);
            float wet_r = read_delay(delay_r_, delay_r, buf_size);

            // Mix
            out_l[i] = dry_l * (1.0f - mix) + wet_l * mix;
            out_r[i] = dry_r * (1.0f - mix) + wet_r * mix;

            write_pos_ = (write_pos_ + 1) % buf_size;
            lfo_phase_ += phase_inc;
            if (lfo_phase_ >= 1.0) lfo_phase_ -= 1.0;
        }
    }

private:
    double sample_rate_ = 48000.0;
    std::vector<float> delay_l_, delay_r_;
    int write_pos_ = 0;
    double lfo_phase_ = 0.0;

    float read_delay(const std::vector<float>& buf, float delay, int size) const {
        float read_pos = static_cast<float>(write_pos_) - delay;
        if (read_pos < 0) read_pos += size;
        int idx0 = static_cast<int>(read_pos) % size;
        int idx1 = (idx0 + 1) % size;
        float frac = read_pos - std::floor(read_pos);
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }
};

inline std::unique_ptr<format::Processor> create_pulp_chorus() {
    return std::make_unique<PulpChorus>();
}

} // namespace pulp::examples
