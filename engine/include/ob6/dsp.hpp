// Small DSP helpers shared by the engine. Header-only, no allocation.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ob6 {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf(float x, float lo, float hi) { return std::min(std::max(x, lo), hi); }

inline float noteToHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

// Rational tanh approximation, exact 1.0 at |x| >= 3 and continuous there.
inline float softClip(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Normalised 0..1 knob -> 1 ms .. 20 s, exponential like an analog time pot.
inline float timeParamToSeconds(float x) { return 0.001f * std::pow(20000.0f, clampf(x, 0.0f, 1.0f)); }

// Coefficient for y += (target - y) * c reaching ~63% in `seconds`.
inline float onePoleCoef(float seconds, float sampleRate) {
    if (seconds <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-1.0f / (seconds * sampleRate));
}

// xorshift32: deterministic, cheap, good enough for noise and slop.
class Rng {
public:
    explicit Rng(uint32_t seed = 0x9E3779B9u) { seedWith(seed); }
    void seedWith(uint32_t seed) { state_ = seed ? seed : 0x9E3779B9u; }
    uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float bipolar() { return uniform() * 2.0f - 1.0f; }

private:
    uint32_t state_;
};

}  // namespace ob6
