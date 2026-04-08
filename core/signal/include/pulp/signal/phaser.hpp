#pragma once

#include <algorithm>
#include <cmath>
#include <array>

namespace pulp::signal {

// Phaser effect using cascaded allpass filters with LFO modulation
class Phaser {
public:
    void set_sample_rate(float sr) { sample_rate_ = sr; }
    void set_rate(float hz) { rate_ = hz; }       // LFO rate
    void set_depth(float d) { depth_ = d; }       // Modulation depth (0-1)
    void set_feedback(float fb) { feedback_ = std::clamp(fb, -0.95f, 0.95f); }
    void set_mix(float m) { mix_ = m; }
    void set_stages(int n) { stages_ = std::clamp(n, 2, max_stages); }

    float process(float input) {
        // LFO
        float lfo = std::sin(2.0f * pi * phase_) * 0.5f + 0.5f;
        phase_ += rate_ / sample_rate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        // Map LFO to frequency range (200Hz - 5000Hz)
        float min_freq = 200.0f;
        float max_freq = 5000.0f;
        float freq = min_freq + lfo * depth_ * (max_freq - min_freq);

        // Allpass coefficient
        float w0 = 2.0f * pi * freq / sample_rate_;
        float coeff = (1.0f - std::tan(w0 * 0.5f)) / (1.0f + std::tan(w0 * 0.5f));

        // Process through allpass chain
        float x = input + feedback_state_ * feedback_;
        for (int i = 0; i < stages_; ++i) {
            float y = coeff * (x - ap_state_[i * 2 + 1]) + ap_state_[i * 2];
            ap_state_[i * 2] = x;
            ap_state_[i * 2 + 1] = y;
            x = y;
        }
        feedback_state_ = x;

        return input * (1.0f - mix_) + x * mix_;
    }

    void process(float* buffer, int num_samples) {
        for (int i = 0; i < num_samples; ++i)
            buffer[i] = process(buffer[i]);
    }

    void reset() {
        ap_state_.fill(0);
        feedback_state_ = 0;
        phase_ = 0;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;
    static constexpr int max_stages = 8;

    float sample_rate_ = 44100.0f;
    float rate_ = 0.5f;
    float depth_ = 0.7f;
    float feedback_ = 0.5f;
    float mix_ = 0.5f;
    int stages_ = 4;
    float phase_ = 0;
    float feedback_state_ = 0;
    std::array<float, max_stages * 2> ap_state_{};
};

} // namespace pulp::signal
