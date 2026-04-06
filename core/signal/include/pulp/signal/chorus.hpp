#pragma once

#include <pulp/signal/delay_line.hpp>
#include <cmath>

namespace pulp::signal {

// Stereo chorus effect using modulated delay lines
class Chorus {
public:
    void prepare(float sample_rate) {
        sample_rate_ = sample_rate;
        int max_delay = static_cast<int>(sample_rate * 0.05f); // 50ms max
        delay_l_.prepare(max_delay);
        delay_r_.prepare(max_delay);
    }

    void set_rate(float hz) { rate_ = hz; }      // LFO rate (0.1-5 Hz typical)
    void set_depth(float d) { depth_ = d; }       // Modulation depth (0-1)
    void set_mix(float m) { mix_ = m; }           // Dry/wet mix (0-1)
    void set_delay_ms(float ms) { delay_ms_ = ms; } // Center delay (5-30ms typical)

    struct StereoSample { float left, right; };

    StereoSample process(float input) {
        float lfo_l = std::sin(2.0f * pi * phase_);
        float lfo_r = std::sin(2.0f * pi * phase_ + pi * 0.5f); // 90° offset

        float delay_samples = delay_ms_ * sample_rate_ * 0.001f;
        float mod_l = delay_samples + lfo_l * depth_ * delay_samples * 0.5f;
        float mod_r = delay_samples + lfo_r * depth_ * delay_samples * 0.5f;

        delay_l_.push(input);
        delay_r_.push(input);

        float wet_l = delay_l_.read(mod_l);
        float wet_r = delay_r_.read(mod_r);

        phase_ += rate_ / sample_rate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        float dry = 1.0f - mix_;
        return {input * dry + wet_l * mix_,
                input * dry + wet_r * mix_};
    }

    void reset() {
        delay_l_.reset();
        delay_r_.reset();
        phase_ = 0;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;
    DelayLine delay_l_, delay_r_;
    float sample_rate_ = 44100.0f;
    float rate_ = 1.0f;
    float depth_ = 0.5f;
    float mix_ = 0.5f;
    float delay_ms_ = 15.0f;
    float phase_ = 0;
};

} // namespace pulp::signal
