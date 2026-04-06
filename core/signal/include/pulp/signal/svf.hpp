#pragma once

#include <cmath>

namespace pulp::signal {

// State Variable Filter — Topology Preserving Transform (TPT) design
// Provides simultaneous lowpass, highpass, bandpass, and notch outputs
// Numerically stable at all frequencies, no cramping at Nyquist
class Svf {
public:
    enum class Mode { lowpass, highpass, bandpass, notch };

    void set_sample_rate(float sr) { sample_rate_ = sr; update(); }
    void set_frequency(float hz) { freq_ = hz; update(); }
    void set_resonance(float q) { q_ = q; update(); }
    void set_mode(Mode m) { mode_ = m; }

    float process(float input) {
        float v3 = input - ic2_;
        float v1 = a1_ * ic1_ + a2_ * v3;
        float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;

        switch (mode_) {
            case Mode::lowpass:  return v2;
            case Mode::highpass: return input - k_ * v1 - v2;
            case Mode::bandpass: return v1;
            case Mode::notch:    return input - k_ * v1;
        }
        return v2;
    }

    void process(float* buffer, int num_samples) {
        for (int i = 0; i < num_samples; ++i)
            buffer[i] = process(buffer[i]);
    }

    void reset() { ic1_ = 0; ic2_ = 0; }

private:
    float sample_rate_ = 44100.0f;
    float freq_ = 1000.0f;
    float q_ = 0.707f;
    Mode mode_ = Mode::lowpass;

    float g_ = 0, k_ = 0;
    float a1_ = 0, a2_ = 0, a3_ = 0;
    float ic1_ = 0, ic2_ = 0;

    void update() {
        g_ = std::tan(3.14159265f * freq_ / sample_rate_);
        k_ = 1.0f / q_;
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
        a2_ = g_ * a1_;
        a3_ = g_ * a2_;
    }
};

} // namespace pulp::signal
