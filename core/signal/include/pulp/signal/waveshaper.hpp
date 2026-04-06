#pragma once

#include <cmath>
#include <algorithm>

namespace pulp::signal {

// Waveshaping distortion with multiple curve types
class WaveShaper {
public:
    enum class Curve { soft_clip, hard_clip, tanh_clip, fold, sine_fold };

    void set_curve(Curve c) { curve_ = c; }
    void set_drive(float drive) { drive_ = drive; }

    float process(float input) const {
        float x = input * drive_;
        switch (curve_) {
            case Curve::soft_clip:
                return x / (1.0f + std::abs(x));
            case Curve::hard_clip:
                return std::clamp(x, -1.0f, 1.0f);
            case Curve::tanh_clip:
                return std::tanh(x);
            case Curve::fold:
                // Wave folding: fold back at +-1
                while (x > 1.0f || x < -1.0f) {
                    if (x > 1.0f) x = 2.0f - x;
                    if (x < -1.0f) x = -2.0f - x;
                }
                return x;
            case Curve::sine_fold:
                return std::sin(x * 1.5707963f); // pi/2
        }
        return x;
    }

    void process(float* buffer, int num_samples) const {
        for (int i = 0; i < num_samples; ++i)
            buffer[i] = process(buffer[i]);
    }

private:
    Curve curve_ = Curve::tanh_clip;
    float drive_ = 1.0f;
};

} // namespace pulp::signal
