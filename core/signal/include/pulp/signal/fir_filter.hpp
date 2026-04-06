#pragma once

/// @file fir_filter.hpp
/// Finite Impulse Response filter with configurable order and coefficients.

#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace pulp::signal {

/// FIR filter with arbitrary coefficients and efficient circular buffer.
///
/// @code
/// FirFilter fir;
/// fir.set_coefficients({0.25f, 0.5f, 0.25f}); // 3-tap averaging
/// float out = fir.process(in);
/// @endcode
class FirFilter {
public:
    FirFilter() = default;

    /// Set filter coefficients. The number of taps equals coefficients.size().
    void set_coefficients(std::vector<float> coeffs) {
        coefficients_ = std::move(coeffs);
        buffer_.resize(coefficients_.size(), 0.0f);
        write_pos_ = 0;
    }

    /// Get current coefficient count (filter order + 1).
    int order() const { return static_cast<int>(coefficients_.size()); }

    /// Process a single sample.
    float process(float input) {
        if (coefficients_.empty()) return input;

        buffer_[write_pos_] = input;

        float output = 0.0f;
        int n = static_cast<int>(coefficients_.size());
        int pos = write_pos_;

        for (int i = 0; i < n; ++i) {
            output += coefficients_[static_cast<size_t>(i)] * buffer_[static_cast<size_t>(pos)];
            if (--pos < 0) pos = n - 1;
        }

        write_pos_ = (write_pos_ + 1) % n;
        return output;
    }

    /// Process a buffer of samples in-place.
    void process(float* data, int num_samples) {
        for (int i = 0; i < num_samples; ++i) {
            data[i] = process(data[i]);
        }
    }

    /// Reset the internal delay buffer to zero.
    void reset() {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        write_pos_ = 0;
    }

    // ── Coefficient generators ──────────────────────────────────────────

    /// Generate lowpass FIR coefficients using windowed-sinc method.
    /// @param num_taps Number of filter taps (odd recommended).
    /// @param cutoff_hz Cutoff frequency in Hz.
    /// @param sample_rate Sample rate in Hz.
    static std::vector<float> lowpass(int num_taps, float cutoff_hz, float sample_rate) {
        std::vector<float> h(static_cast<size_t>(num_taps));
        float fc = cutoff_hz / sample_rate;
        int m = num_taps - 1;
        constexpr float pi = 3.14159265358979323846f;

        for (int i = 0; i <= m; ++i) {
            float n = static_cast<float>(i) - static_cast<float>(m) / 2.0f;
            if (std::abs(n) < 1e-6f) {
                h[static_cast<size_t>(i)] = 2.0f * fc;
            } else {
                h[static_cast<size_t>(i)] = std::sin(2.0f * pi * fc * n) / (pi * n);
            }
            // Hamming window
            float w = 0.54f - 0.46f * std::cos(2.0f * pi * static_cast<float>(i) / static_cast<float>(m));
            h[static_cast<size_t>(i)] *= w;
        }

        // Normalize
        float sum = std::accumulate(h.begin(), h.end(), 0.0f);
        if (sum > 0.0f) {
            for (auto& v : h) v /= sum;
        }
        return h;
    }

    /// Generate highpass FIR coefficients using spectral inversion.
    static std::vector<float> highpass(int num_taps, float cutoff_hz, float sample_rate) {
        auto h = lowpass(num_taps, cutoff_hz, sample_rate);
        for (auto& v : h) v = -v;
        h[static_cast<size_t>(num_taps / 2)] += 1.0f;
        return h;
    }

private:
    std::vector<float> coefficients_;
    std::vector<float> buffer_;
    int write_pos_ = 0;
};

} // namespace pulp::signal
