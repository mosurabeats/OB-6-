// Six-voice OB-6 style synth: voice allocation, unison, global LFO,
// performance controls, MIDI input and the effects bus.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "effects.hpp"
#include "lfo.hpp"
#include "params.hpp"
#include "voice.hpp"

namespace ob6 {

class Synth {
public:
    static constexpr int kMaxVoices = 6;

    explicit Synth(float sampleRate = 48000.0f, uint32_t seed = 1);

    void setPatch(const Patch& patch);
    const Patch& patch() const { return patch_; }
    void setParam(P p, float value);

    void noteOn(int note, int velocity);  // velocity 1..127 (0 = note off)
    void noteOff(int note);
    void setSustain(bool down);
    void setPitchBend(float amount);  // -1..1
    void setModWheel(float amount);   // 0..1
    void setAftertouch(float amount); // 0..1
    void allNotesOff();               // release everything
    void panic();                     // silence immediately

    // Raw MIDI bytes of one complete channel message (any channel).
    void handleMidi(const uint8_t* data, size_t len);
    // Whether CCs from the table in params.hpp set parameters. A host that
    // maps CCs to parameters itself (the MPC VST wrapper) turns this off so
    // its screen stays in sync.
    void setCcParamsEnabled(bool enabled) { ccParams_ = enabled; }

    // Renders `frames` stereo samples, overwriting the buffers.
    void process(float* outL, float* outR, size_t frames);

    int activeVoices() const;
    float sampleRate() const { return fs_; }

private:
    int polyphony() const;
    Voice& allocate(int note);
    void startUnison(int note, float velocity, bool retrigger);
    void releaseUnheld();

    float fs_;
    Patch patch_;
    std::array<Voice, kMaxVoices> voices_;
    Lfo lfo_;
    Chorus chorus_;
    PingPongDelay delay_;

    uint64_t allocCounter_ = 0;
    float lastNote_ = 60.0f;
    std::array<bool, 128> keyDown_{};
    bool sustain_ = false;

    // Unison note stack (last-note priority).
    std::array<int, 128> stack_{};
    int stackSize_ = 0;
    std::array<float, 128> stackVel_{};

    float bend_ = 0.0f, wheel_ = 0.0f, aftertouch_ = 0.0f;
    float dcX_ = 0.0f, dcY_ = 0.0f;  // DC blocker state (AC coupling)
    float dcCoef_ = 0.999f;
    bool ccParams_ = true;
};

}  // namespace ob6
