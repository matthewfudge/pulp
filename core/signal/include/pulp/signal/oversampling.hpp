#pragma once

#include <pulp/signal/biquad.hpp>
#include <vector>
#include <functional>

namespace pulp::signal {

// Oversampling processor — runs a callback at 2x or 4x sample rate
// with anti-aliasing filters on input and output
class Oversampler {
public:
    enum class Factor { x2 = 2, x4 = 4 };

    void set_factor(Factor f) { factor_ = f; }
    void set_sample_rate(float sr) { sample_rate_ = sr; configure_filters(); }

    // Process a single sample with oversampled callback
    // The callback receives an upsampled sample and returns the processed output
    float process(float input, std::function<float(float)> callback) {
        int n = static_cast<int>(factor_);

        // Upsample: insert zeros
        std::vector<float> upsampled(n, 0.0f);
        upsampled[0] = input * n; // Scale up to preserve energy

        // Anti-alias filter on upsampled signal
        for (int i = 0; i < n; ++i)
            upsampled[i] = aa_up_.process(upsampled[i]);

        // Process at higher rate
        for (int i = 0; i < n; ++i)
            upsampled[i] = callback(upsampled[i]);

        // Anti-alias filter on output
        for (int i = 0; i < n; ++i)
            upsampled[i] = aa_down_.process(upsampled[i]);

        // Downsample: take every nth sample
        return upsampled[0];
    }

    void reset() {
        aa_up_.reset();
        aa_down_.reset();
    }

private:
    Factor factor_ = Factor::x2;
    float sample_rate_ = 44100.0f;
    Biquad aa_up_, aa_down_;

    void configure_filters() {
        float os_rate = sample_rate_ * static_cast<float>(factor_);
        float cutoff = sample_rate_ * 0.4f; // Below Nyquist of original rate
        aa_up_.set_coefficients(Biquad::Type::lowpass, cutoff, 0.707f, os_rate);
        aa_down_.set_coefficients(Biquad::Type::lowpass, cutoff, 0.707f, os_rate);
    }
};

} // namespace pulp::signal
