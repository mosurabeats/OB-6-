// 2-pole state-variable filter in the spirit of the Oberheim SEM.
//
// Topology-preserving (trapezoidal) SVF after Andrew Simper, with gentle
// saturation of the band-pass integrator for analog-style resonance
// compression. Like the SEM it does not self-oscillate (max Q ~ 10).
//
// Mode morphs continuously: 0 = low-pass, 0.5 = notch, 1 = high-pass.
// The separate band-pass switch overrides the morph, as on the OB-6.
#pragma once

#include "dsp.hpp"

namespace ob6 {

class SvfFilter {
public:
    void reset() { ic1_ = ic2_ = 0.0f; }

    static float gFromHz(float hz, float sampleRate) {
        hz = clampf(hz, 5.0f, 0.49f * sampleRate);
        return std::tan(kPi * hz / sampleRate);
    }
    static float kFromResonance(float res) { return 2.0f - 1.9f * clampf(res, 0.0f, 1.0f); }

    float process(float x, float g, float k, float mode, bool bandpass) {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - ic2_;
        const float v1 = a1 * ic1_ + a2 * v3;
        const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        ic1_ = 4.0f * softClip(0.25f * ic1_);

        if (bandpass) return v1 * std::sqrt(2.0f * k);  // unity peak at zero resonance
        const float lp = v2;
        const float hp = x - k * v1 - v2;
        const float m = clampf(mode, 0.0f, 1.0f);
        const float lpGain = std::min(1.0f, 2.0f * (1.0f - m));
        const float hpGain = std::min(1.0f, 2.0f * m);
        return lpGain * lp + hpGain * hp;  // lp + hp at the centre is the notch
    }

private:
    float ic1_ = 0.0f, ic2_ = 0.0f;
};

}  // namespace ob6
