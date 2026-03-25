#pragma once

#include <cmath>
#include <algorithm>

namespace pulp::signal {

// Equal-power stereo panner
// pan = -1 (full left), 0 (center), +1 (full right)
class Panner {
public:
    void set_pan(float pan) { pan_ = std::clamp(pan, -1.0f, 1.0f); }
    float pan() const { return pan_; }

    struct StereoSample {
        float left, right;
    };

    // Pan a mono signal to stereo
    StereoSample process(float input) const {
        // Equal-power panning: constant total energy
        float angle = (pan_ + 1.0f) * 0.25f * pi; // 0 to pi/2
        return {input * std::cos(angle), input * std::sin(angle)};
    }

    // Pan stereo in-place (adjust balance)
    void process(float& left, float& right) const {
        float angle = (pan_ + 1.0f) * 0.25f * pi;
        float l_gain = std::cos(angle);
        float r_gain = std::sin(angle);
        left *= l_gain;
        right *= r_gain;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;
    float pan_ = 0.0f;
};

} // namespace pulp::signal
