#include "ob6/synth.hpp"

#include <algorithm>

namespace ob6 {

Synth::Synth(float sampleRate, uint32_t seed) : fs_(sampleRate) {
    dcCoef_ = 1.0f - kTwoPi * 5.0f / sampleRate;  // ~5 Hz high-pass
    for (int i = 0; i < kMaxVoices; ++i) voices_[i].init(sampleRate, seed * 7919u + static_cast<uint32_t>(i) + 1u);
    lfo_.reset(seed);
    chorus_.prepare(sampleRate);
    delay_.prepare(sampleRate);
}

void Synth::setPatch(const Patch& patch) {
    patch_ = patch;
    for (auto& v : voices_) v.updateEnvelopes(patch_);
}

void Synth::setParam(P p, float value) {
    patch_.set(p, value);
    for (auto& v : voices_) v.updateEnvelopes(patch_);
}

int Synth::polyphony() const { return std::clamp(patch_.steps(P::Voices), 1, kMaxVoices); }

int Synth::activeVoices() const {
    return static_cast<int>(std::count_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.active(); }));
}

Voice& Synth::allocate(int note) {
    const int n = polyphony();
    // 1. Same note already sounding: reuse it (no doubled voices).
    for (int i = 0; i < n; ++i)
        if (voices_[i].active() && voices_[i].note() == note) return voices_[i];
    // 2. A free voice, least recently started first (round robin feel).
    // 3. Otherwise the oldest released voice, then the oldest held one.
    Voice* best = nullptr;
    auto rank = [](const Voice& v) { return !v.active() ? 0 : (!v.gated() ? 1 : 2); };
    for (int i = 0; i < n; ++i) {
        Voice& v = voices_[i];
        if (!best || rank(v) < rank(*best) || (rank(v) == rank(*best) && v.startedAt < best->startedAt)) best = &v;
    }
    return *best;
}

void Synth::startUnison(int note, float velocity, bool retrigger) {
    const int n = std::clamp(patch_.steps(P::UnisonVoices), 1, kMaxVoices);
    const float spread = 0.25f * patch_[P::UnisonDetune];
    for (int i = 0; i < n; ++i) {
        const float pos = n == 1 ? 0.0f : 2.0f * static_cast<float>(i) / static_cast<float>(n - 1) - 1.0f;
        voices_[i].noteOn(patch_, note, velocity, lastNote_, pos * spread, retrigger);
        voices_[i].startedAt = ++allocCounter_;
    }
    for (int i = n; i < kMaxVoices; ++i)
        if (voices_[i].gated()) voices_[i].noteOff();
}

void Synth::noteOn(int note, int velocity) {
    if (note < 0 || note > 127) return;
    if (velocity <= 0) return noteOff(note);
    const float vel = static_cast<float>(velocity) / 127.0f;
    keyDown_[note] = true;

    if (patch_.on(P::Unison)) {
        // Push to the top of the note stack.
        auto end = stack_.begin() + stackSize_;
        auto it = std::remove(stack_.begin(), end, note);
        stackSize_ = static_cast<int>(it - stack_.begin());
        stack_[stackSize_++] = note;
        stackVel_[note] = vel;
        startUnison(note, vel, true);
    } else {
        Voice& v = allocate(note);
        v.noteOn(patch_, note, vel, lastNote_, 0.0f, true);
        v.startedAt = ++allocCounter_;
    }
    lastNote_ = static_cast<float>(note);
}

void Synth::noteOff(int note) {
    if (note < 0 || note > 127) return;
    keyDown_[note] = false;

    if (patch_.on(P::Unison)) {
        auto end = stack_.begin() + stackSize_;
        const bool wasTop = stackSize_ > 0 && stack_[stackSize_ - 1] == note;
        stackSize_ = static_cast<int>(std::remove(stack_.begin(), end, note) - stack_.begin());
        if (!wasTop || sustain_) return;
        if (stackSize_ > 0) {
            const int prev = stack_[stackSize_ - 1];
            startUnison(prev, stackVel_[prev], false);  // legato back to the held note
            lastNote_ = static_cast<float>(prev);
        } else {
            for (auto& v : voices_)
                if (v.gated()) v.noteOff();
        }
        return;
    }
    if (sustain_) return;
    for (auto& v : voices_)
        if (v.gated() && v.note() == note) v.noteOff();
}

void Synth::releaseUnheld() {
    if (patch_.on(P::Unison)) {
        if (stackSize_ == 0)
            for (auto& v : voices_)
                if (v.gated()) v.noteOff();
        return;
    }
    for (auto& v : voices_)
        if (v.gated() && (v.note() < 0 || !keyDown_[v.note()])) v.noteOff();
}

void Synth::setSustain(bool down) {
    sustain_ = down;
    if (!down) releaseUnheld();
}

void Synth::setPitchBend(float amount) { bend_ = clampf(amount, -1.0f, 1.0f); }
void Synth::setModWheel(float amount) { wheel_ = clampf(amount, 0.0f, 1.0f); }
void Synth::setAftertouch(float amount) { aftertouch_ = clampf(amount, 0.0f, 1.0f); }

void Synth::allNotesOff() {
    keyDown_.fill(false);
    stackSize_ = 0;
    sustain_ = false;
    for (auto& v : voices_)
        if (v.gated()) v.noteOff();
}

void Synth::panic() {
    allNotesOff();
    for (auto& v : voices_) v.kill();
    chorus_.clear();
    delay_.clear();
}

void Synth::handleMidi(const uint8_t* d, size_t len) {
    if (len < 1) return;
    const uint8_t status = d[0] & 0xF0;
    const int a = len > 1 ? d[1] & 0x7F : 0;
    const int b = len > 2 ? d[2] & 0x7F : 0;
    switch (status) {
        case 0x80: noteOff(a); break;
        case 0x90: b == 0 ? noteOff(a) : noteOn(a, b); break;
        case 0xD0: setAftertouch(static_cast<float>(a) / 127.0f); break;
        case 0xE0: setPitchBend(static_cast<float>((b << 7 | a) - 8192) / 8192.0f); break;
        case 0xB0:
            if (a == 1) {
                setModWheel(static_cast<float>(b) / 127.0f);
            } else if (a == 64) {
                setSustain(b >= 64);
            } else if (a == 120) {
                panic();
            } else if (a == 123) {
                allNotesOff();
            } else if (const ParamInfo* info = findParamByCC(a)) {
                setParam(info->id, info->min + (info->max - info->min) * static_cast<float>(b) / 127.0f);
            }
            break;
        default: break;
    }
}

void Synth::process(float* outL, float* outR, size_t frames) {
    VoiceContext ctx;
    ctx.sampleRate = fs_;
    const float lfoHz = Lfo::rateToHz(patch_[P::LfoRate]);
    const int lfoShape = patch_.steps(P::LfoShape);
    const float amount = clampf(
        patch_[P::LfoAmount] + wheel_ * patch_[P::WheelLfo] + aftertouch_ * patch_[P::AtLfo], 0.0f, 1.0f);
    ctx.lfoDepth = amount * amount;
    ctx.bend = bend_ * patch_[P::BendRange];
    ctx.aftertouch = aftertouch_;

    const float chorusMix = patch_[P::ChorusMix];
    const float delayMix = patch_[P::DelayMix];
    const float volume = patch_[P::Volume];

    for (size_t i = 0; i < frames; ++i) {
        ctx.lfoRaw = lfo_.process(lfoHz, lfoShape, fs_);
        float dry = 0.0f;
        for (auto& v : voices_) dry += v.process(patch_, ctx);
        dry *= 0.4f;
        // AC coupling: sync and asymmetric X-Mod waveforms carry DC.
        dcY_ = dry - dcX_ + dcCoef_ * dcY_;
        dcX_ = dry;
        dry = dcY_;

        float l = dry, r = dry;
        if (chorusMix > 0.0f) {
            float cl, cr;
            chorus_.process(dry, cl, cr, patch_[P::ChorusRate], patch_[P::ChorusDepth]);
            l = dry + chorusMix * (cl - dry);
            r = dry + chorusMix * (cr - dry);
        }
        if (delayMix > 0.0f) {
            float dl, dr;
            delay_.process(0.5f * (l + r), dl, dr, patch_[P::DelayTime], patch_[P::DelayFeedback]);
            l += delayMix * dl;
            r += delayMix * dr;
        }
        outL[i] = softClip(l * volume);
        outR[i] = softClip(r * volume);
    }
}

}  // namespace ob6
