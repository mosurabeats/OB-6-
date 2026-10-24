// Global LFO: sine, triangle, saw up, saw down (ramp), square, sample & hold.
#pragma once

#include "dsp.hpp"

namespace ob6 {

class Lfo {
public:
    enum Shape { Sine = 0, Triangle, Saw, Ramp, Square, Random, NumShapes };

    void reset(uint32_t seed = 1) {
        phase_ = 0.0f;
        rng_.seedWith(seed);
        held_ = rng_.bipolar();
    }

    // Returns -1..1.
    float process(float hz, int shape, float sampleRate) {
        phase_ += hz / sampleRate;
        if (phase_ >= 1.0f) {
            phase_ -= std::floor(phase_);
            held_ = rng_.bipolar();
        }
        switch (shape) {
            case Sine: return std::sin(kTwoPi * phase_);
            case Triangle: return 1.0f - 4.0f * std::fabs(phase_ - 0.5f);
            case Saw: return 2.0f * phase_ - 1.0f;
            case Ramp: return 1.0f - 2.0f * phase_;
            case Square: return phase_ < 0.5f ? 1.0f : -1.0f;
            case Random: return held_;
            default: return 0.0f;
        }
    }

    // Normalised 0..1 knob -> 0.05 .. 51 Hz.
    static float rateToHz(float x) { return 0.05f * std::exp2(clampf(x, 0.0f, 1.0f) * 10.0f); }

private:
    float phase_ = 0.0f;
    float held_ = 0.0f;
    Rng rng_;
};

}  // namespace ob6
