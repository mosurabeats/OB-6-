// One OB-6 style voice: 2 VCOs + sub + noise -> SEM-style SVF -> VCA,
// with filter/amp envelopes, X-Mod, glide and per-voice analog slop.
#pragma once

#include "envelope.hpp"
#include "filter.hpp"
#include "oscillator.hpp"
#include "params.hpp"

namespace ob6 {

// Global modulation state the synth computes once per sample.
struct VoiceContext {
    float sampleRate = 48000.0f;
    float lfoRaw = 0.0f;    // -1..1
    float lfoDepth = 0.0f;  // 0..1, already curved
    float bend = 0.0f;      // semitones
    float aftertouch = 0.0f;
};

class Voice {
public:
    // `seed` gives the voice its fixed personality (static tuning/filter
    // offsets), like component tolerances in a real voice card.
    void init(float sampleRate, uint32_t seed);

    void noteOn(const Patch& p, int note, float velocity, float startPitch, float detune, bool retrigger);
    void noteOff();
    void kill();
    void updateEnvelopes(const Patch& p);  // call after envelope params change

    float process(const Patch& p, const VoiceContext& ctx);

    bool active() const { return active_; }
    bool gated() const { return gated_; }
    int note() const { return note_; }
    float pitch() const { return pitch_; }
    uint64_t startedAt = 0;  // allocation order, maintained by the synth

private:
    float fs_ = 48000.0f;
    BlepOscillator osc1_, osc2_, sub_;
    SvfFilter filter_;
    Envelope fenv_, aenv_;
    Rng rng_;

    bool active_ = false;
    bool gated_ = false;
    int note_ = -1;
    float velocity_ = 1.0f;
    float pitch_ = 60.0f, targetPitch_ = 60.0f;
    float glideCoef_ = 1.0f;
    float detune_ = 0.0f;
    float osc2Last_ = 0.0f;

    // Static slop, each -1..1, scaled by the vintage parameter.
    float slopPitch1_ = 0, slopPitch2_ = 0, slopCutoff_ = 0, slopEnv_ = 0;
    // Slow random pitch drift.
    float drift_ = 0, driftTarget_ = 0;
    int driftCountdown_ = 0;
};

}  // namespace ob6
