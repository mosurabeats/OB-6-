// Analog-style ADSR: exponential segments that aim past their target, the
// way an RC envelope generator does. Retriggers from the current level.
#pragma once

#include "dsp.hpp"

namespace ob6 {

class Envelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(float fs) { fs_ = fs; }

    // Times in seconds, sustain 0..1.
    void set(float attack, float decay, float sustain, float release) {
        sustain_ = clampf(sustain, 0.0f, 1.0f);
        attackCoef_ = coef(attack * fs_, kAttackRatio);
        attackBase_ = (1.0f + kAttackRatio) * (1.0f - attackCoef_);
        decayCoef_ = coef(decay * fs_, kDecayRatio);
        decayBase_ = (sustain_ - kDecayRatio) * (1.0f - decayCoef_);
        releaseCoef_ = coef(release * fs_, kDecayRatio);
        releaseBase_ = -kDecayRatio * (1.0f - releaseCoef_);
    }

    void gateOn() { stage_ = Stage::Attack; }
    void gateOff() {
        if (stage_ != Stage::Idle) stage_ = Stage::Release;
    }
    void reset() {
        stage_ = Stage::Idle;
        out_ = 0.0f;
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                break;
            case Stage::Attack:
                out_ = attackBase_ + out_ * attackCoef_;
                if (out_ >= 1.0f) {
                    out_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;
            case Stage::Decay:
                out_ = decayBase_ + out_ * decayCoef_;
                if (out_ <= sustain_) {
                    out_ = sustain_;
                    stage_ = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                out_ = sustain_;
                break;
            case Stage::Release:
                out_ = releaseBase_ + out_ * releaseCoef_;
                if (out_ <= 0.0f) {
                    out_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }
        return out_;
    }

    Stage stage() const { return stage_; }
    bool idle() const { return stage_ == Stage::Idle; }
    float value() const { return out_; }

private:
    static constexpr float kAttackRatio = 0.3f;
    static constexpr float kDecayRatio = 0.0001f;

    static float coef(float samples, float ratio) {
        if (samples <= 1.0f) return 0.0f;
        return std::exp(-std::log((1.0f + ratio) / ratio) / samples);
    }

    float fs_ = 48000.0f;
    Stage stage_ = Stage::Idle;
    float out_ = 0.0f;
    float sustain_ = 1.0f;
    float attackCoef_ = 0.0f, attackBase_ = 1.0f;
    float decayCoef_ = 0.0f, decayBase_ = 0.0f;
    float releaseCoef_ = 0.0f, releaseBase_ = 0.0f;
};

}  // namespace ob6
