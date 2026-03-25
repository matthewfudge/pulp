#pragma once

#include <cmath>

namespace pulp::signal {

// Band-limited oscillator with polyBLEP anti-aliasing
// Supports sine, saw, square, and triangle waveforms
class Oscillator {
public:
    enum class Waveform { sine, saw, square, triangle };

    void set_sample_rate(float sr) { sample_rate_ = sr; }
    void set_frequency(float hz) { freq_ = hz; }
    void set_waveform(Waveform w) { waveform_ = w; }

    // Reset phase to 0
    void reset() { phase_ = 0; }

    // Generate next sample
    float next() {
        float dt = freq_ / sample_rate_;
        float out = 0;

        switch (waveform_) {
            case Waveform::sine:
                out = std::sin(2.0f * pi * phase_);
                break;

            case Waveform::saw:
                out = 2.0f * phase_ - 1.0f;
                out -= poly_blep(phase_, dt);
                break;

            case Waveform::square: {
                out = phase_ < 0.5f ? 1.0f : -1.0f;
                out += poly_blep(phase_, dt);
                out -= poly_blep(std::fmod(phase_ + 0.5f, 1.0f), dt);
                break;
            }

            case Waveform::triangle:
                // Integrated square wave
                out = phase_ < 0.5f ? 1.0f : -1.0f;
                out += poly_blep(phase_, dt);
                out -= poly_blep(std::fmod(phase_ + 0.5f, 1.0f), dt);
                // Integrate: leaky integrator
                tri_state_ = dt * out + (1.0f - dt) * tri_state_;
                // Scale to -1..1 range
                out = tri_state_ * 4.0f;
                break;
        }

        // Advance phase
        phase_ += dt;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        return out;
    }

    float phase() const { return phase_; }
    float frequency() const { return freq_; }

private:
    static constexpr float pi = 3.14159265358979323846f;

    float sample_rate_ = 44100.0f;
    float freq_ = 440.0f;
    float phase_ = 0;
    float tri_state_ = 0; // For triangle wave integration
    Waveform waveform_ = Waveform::sine;

    // PolyBLEP anti-aliasing residual
    static float poly_blep(float t, float dt) {
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }
};

} // namespace pulp::signal
