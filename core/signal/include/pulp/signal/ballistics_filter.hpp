#pragma once

/// @file ballistics_filter.hpp
/// Peak/RMS envelope follower with independent attack and release curves.

#include <cmath>
#include <algorithm>

namespace pulp::signal {

/// Envelope follower with configurable attack and release times.
///
/// Tracks the envelope of an input signal using first-order IIR
/// smoothing with separate attack (rising) and release (falling)
/// time constants.
///
/// @code
/// BallisticsFilter env;
/// env.prepare(44100.0f);
/// env.set_attack_ms(1.0f);
/// env.set_release_ms(100.0f);
/// float envelope = env.process(std::abs(sample));
/// @endcode
class BallisticsFilter {
public:
    enum class Mode { peak, rms };

    BallisticsFilter() = default;

    void prepare(float sample_rate) {
        sample_rate_ = sample_rate;
        update_coefficients();
    }

    void set_attack_ms(float ms) {
        attack_ms_ = std::max(0.01f, ms);
        update_coefficients();
    }

    void set_release_ms(float ms) {
        release_ms_ = std::max(0.01f, ms);
        update_coefficients();
    }

    void set_mode(Mode m) { mode_ = m; }

    float process(float input) {
        float value = (mode_ == Mode::rms) ? input * input : std::abs(input);
        float coeff = (value > state_) ? attack_coeff_ : release_coeff_;
        state_ = state_ + coeff * (value - state_);
        return (mode_ == Mode::rms) ? std::sqrt(state_) : state_;
    }

    void process(const float* input, float* output, int num_samples) {
        for (int i = 0; i < num_samples; ++i) {
            output[i] = process(input[i]);
        }
    }

    float current() const {
        return (mode_ == Mode::rms) ? std::sqrt(state_) : state_;
    }

    void reset() { state_ = 0.0f; }

private:
    float sample_rate_ = 44100.0f;
    float attack_ms_ = 1.0f;
    float release_ms_ = 100.0f;
    float attack_coeff_ = 0.0f;
    float release_coeff_ = 0.0f;
    float state_ = 0.0f;
    Mode mode_ = Mode::peak;

    void update_coefficients() {
        if (sample_rate_ <= 0) return;
        attack_coeff_ = time_constant(attack_ms_);
        release_coeff_ = time_constant(release_ms_);
    }

    float time_constant(float ms) const {
        if (ms <= 0.01f) return 1.0f;
        return 1.0f - std::exp(-2.2f / (ms * 0.001f * sample_rate_));
    }
};

} // namespace pulp::signal
