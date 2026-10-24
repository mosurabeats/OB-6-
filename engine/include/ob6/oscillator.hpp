// Band-limited morphing oscillator (triangle -> saw -> pulse) with hard sync.
//
// Discontinuities are corrected with a 2-sample polyBLEP applied around the
// exact sub-sample event time. To correct the sample *before* an event the
// oscillator runs with one sample of latency; every oscillator in a voice has
// the same latency, so sync and cross-modulation stay phase-consistent.
#pragma once

#include "dsp.hpp"

namespace ob6 {

class BlepOscillator {
public:
    void reset(float phase = 0.0f) {
        phase_ = phase;
        prev_ = 0.0f;
    }

    // inc:      cycles per sample, clamped to [0, 0.45].
    // shape:    0 = triangle, 0.5 = sawtooth, 1 = pulse (continuous morph).
    // duty:     pulse duty cycle (0.02 .. 0.98).
    // syncFrac: < 0 for no sync, otherwise the master wrapped this many
    //           samples ago (0 <= syncFrac < 1) and this oscillator resets.
    // wrapFrac: optional out; time since this oscillator wrapped during this
    //           sample, or -1 if it did not. Feed it to a slave's syncFrac.
    float process(float inc, float shape, float duty, float syncFrac = -1.0f, float* wrapFrac = nullptr);

private:
    struct Weights {
        float tri, saw, pulse, duty;
    };
    static Weights weightsFor(float shape, float duty);
    static float naive(const Weights& w, double p);
    void emit(float height, double d);
    void scan(const Weights& w, double pa, double pb, double inc, double dEnd);

    double phase_ = 0.0;
    float prev_ = 0.0f;  // output owed for the previous sample (latency of one)
    float corr_ = 0.0f;  // BLEP correction accumulated for the current sample
};

inline BlepOscillator::Weights BlepOscillator::weightsFor(float shape, float duty) {
    shape = clampf(shape, 0.0f, 1.0f);
    Weights w{};
    if (shape < 0.5f) {
        w.tri = 1.0f - 2.0f * shape;
        w.saw = 2.0f * shape;
        w.pulse = 0.0f;
    } else {
        w.tri = 0.0f;
        w.saw = 2.0f - 2.0f * shape;
        w.pulse = 2.0f * shape - 1.0f;
    }
    w.duty = clampf(duty, 0.02f, 0.98f);
    return w;
}

inline float BlepOscillator::naive(const Weights& w, double p) {
    const float pf = static_cast<float>(p);
    const float tri = 1.0f - 4.0f * std::fabs(pf - 0.5f);
    const float saw = 2.0f * pf - 1.0f;
    // Pulse with its DC offset removed so PWM does not shift the filter input.
    const float pulse = (pf < w.duty ? 1.0f : -1.0f) - (2.0f * w.duty - 1.0f);
    return w.tri * tri + w.saw * saw + w.pulse * pulse;
}

// A step of `height` that happened `d` samples ago (0 <= d < 1).
inline void BlepOscillator::emit(float height, double d) {
    const float df = static_cast<float>(d);
    prev_ += height * 0.5f * df * df;
    corr_ -= height * 0.5f * (1.0f - df) * (1.0f - df);
}

// Emit steps for waveform edges crossed in phase interval (pa, pb]; pb may
// exceed 1 by less than one cycle. dEnd is the time from pb to "now".
inline void BlepOscillator::scan(const Weights& w, double pa, double pb, double inc, double dEnd) {
    if (inc <= 0.0) return;
    const double edges[3] = {w.duty, 1.0, 1.0 + w.duty};
    for (double e : edges) {
        if (e <= pa || e > pb) continue;
        const double d = (pb - e) / inc + dEnd;
        if (e == 1.0) {
            emit(-2.0f * w.saw + 2.0f * w.pulse, d);  // saw falls, pulse rises
        } else {
            emit(-2.0f * w.pulse, d);  // pulse falls at the duty point
        }
    }
}

inline float BlepOscillator::process(float incf, float shape, float duty, float syncFrac, float* wrapFrac) {
    const double inc = clampf(incf, 0.0f, 0.45f);
    const Weights w = weightsFor(shape, duty);
    corr_ = 0.0f;
    if (wrapFrac) *wrapFrac = -1.0f;

    if (syncFrac >= 0.0f) {
        const double ds = std::min<double>(syncFrac, 0.999999);
        double pe = phase_ + inc * (1.0 - ds);  // phase at the reset instant
        scan(w, phase_, pe, inc, ds);
        if (pe >= 1.0) pe -= 1.0;
        emit(naive(w, 0.0) - naive(w, pe), ds);
        const double pn = inc * ds;
        scan(w, 0.0, pn, inc, 0.0);
        phase_ = pn;
        if (wrapFrac) *wrapFrac = static_cast<float>(ds);
    } else {
        double pb = phase_ + inc;
        scan(w, phase_, pb, inc, 0.0);
        if (pb >= 1.0) {
            pb -= 1.0;
            if (wrapFrac) *wrapFrac = static_cast<float>(pb / inc);
        }
        phase_ = pb;
    }

    const float current = naive(w, phase_) + corr_;
    const float out = prev_;
    prev_ = current;
    return out;
}

}  // namespace ob6
