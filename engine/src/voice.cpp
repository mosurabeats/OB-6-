#include "ob6/voice.hpp"

namespace ob6 {

namespace {

float dutyFromPw(float pw) { return 0.5f - 0.46f * clampf(pw, 0.0f, 1.0f); }

}  // namespace

void Voice::init(float sampleRate, uint32_t seed) {
    fs_ = sampleRate;
    fenv_.setSampleRate(sampleRate);
    aenv_.setSampleRate(sampleRate);
    rng_.seedWith(seed * 2654435761u + 1u);
    slopPitch1_ = rng_.bipolar();
    slopPitch2_ = rng_.bipolar();
    slopCutoff_ = rng_.bipolar();
    slopEnv_ = rng_.bipolar();
    osc1_.reset(rng_.uniform());
    osc2_.reset(rng_.uniform());
    sub_.reset();
    filter_.reset();
    fenv_.reset();
    aenv_.reset();
    active_ = gated_ = false;
    note_ = -1;
}

void Voice::updateEnvelopes(const Patch& p) {
    const float envScale = 1.0f + 0.1f * p[P::Vintage] * slopEnv_;
    fenv_.set(timeParamToSeconds(p[P::FEnvAttack]) * envScale, timeParamToSeconds(p[P::FEnvDecay]) * envScale,
              p[P::FEnvSustain], timeParamToSeconds(p[P::FEnvRelease]) * envScale);
    aenv_.set(timeParamToSeconds(p[P::AEnvAttack]) * envScale, timeParamToSeconds(p[P::AEnvDecay]) * envScale,
              p[P::AEnvSustain], timeParamToSeconds(p[P::AEnvRelease]) * envScale);
    const float g = p[P::Glide];
    glideCoef_ = g <= 0.0f ? 1.0f : onePoleCoef(2.0f * g * g, fs_);
}

void Voice::noteOn(const Patch& p, int note, float velocity, float startPitch, float detune, bool retrigger) {
    updateEnvelopes(p);
    // A sleeping voice glides from the last played note; a busy one from
    // wherever it currently is.
    if (!active_) {
        filter_.reset();
        pitch_ = startPitch;
    }
    if (glideCoef_ >= 1.0f) pitch_ = static_cast<float>(note);
    note_ = note;
    targetPitch_ = static_cast<float>(note);
    velocity_ = clampf(velocity, 0.0f, 1.0f);
    detune_ = detune;
    if (retrigger || !gated_) {
        fenv_.gateOn();
        aenv_.gateOn();
    }
    active_ = gated_ = true;
}

void Voice::noteOff() {
    gated_ = false;
    fenv_.gateOff();
    aenv_.gateOff();
}

void Voice::kill() {
    active_ = gated_ = false;
    fenv_.reset();
    aenv_.reset();
    filter_.reset();
    note_ = -1;
}

float Voice::process(const Patch& p, const VoiceContext& c) {
    if (!active_) return 0.0f;

    const float fe = fenv_.process();
    const float ae = aenv_.process();
    if (aenv_.idle()) {
        active_ = false;
        note_ = -1;
        return 0.0f;
    }

    const float vintage = p[P::Vintage];
    if (--driftCountdown_ <= 0) {
        driftCountdown_ = 2048;
        driftTarget_ = rng_.bipolar();
    }
    drift_ += (driftTarget_ - drift_) * 0.0002f;
    pitch_ += (targetPitch_ - pitch_) * glideCoef_;

    // --- pitch -------------------------------------------------------------
    const float common = c.bend + detune_ + vintage * 0.06f * drift_;
    float p1 = pitch_ + common + p[P::Osc1Freq] + vintage * 0.08f * slopPitch1_;
    float p2 = (p.on(P::Osc2KeyTrack) ? pitch_ : 60.0f) + common + p[P::Osc2Freq] + p[P::Osc2Fine] +
               vintage * 0.08f * slopPitch2_;
    if (p.on(P::Osc2LowFreq)) p2 -= 84.0f;

    float shape1 = p[P::Osc1Shape];
    float pw1 = p[P::Osc1PW];
    float pw2 = p[P::Osc2PW];
    float mode = p[P::FilterMode];
    float cutoffOct = p[P::FilterCutoff] * 10.0f;  // octaves above 20 Hz
    float ampMod = 1.0f;

    // --- LFO ---------------------------------------------------------------
    const float lfo = c.lfoRaw * c.lfoDepth;
    if (p.on(P::LfoDestFreq1)) p1 += 12.0f * lfo;
    if (p.on(P::LfoDestFreq2)) p2 += 12.0f * lfo;
    if (p.on(P::LfoDestPW)) {
        pw1 += 0.5f * lfo;
        pw2 += 0.5f * lfo;
    }
    if (p.on(P::LfoDestCutoff)) cutoffOct += 5.0f * lfo;
    if (p.on(P::LfoDestMode)) mode += 0.5f * lfo;
    if (p.on(P::LfoDestAmp)) ampMod = 1.0f - c.lfoDepth * 0.5f * (1.0f + c.lfoRaw);

    // --- X-Mod (filter env and osc 2 modulate osc 1 / filter) ----------------
    const float xo = p[P::XmodOsc2];
    const float xm = p[P::XmodFenv] * fe + xo * xo * osc2Last_;
    if (p.on(P::XmodDestFreq1)) p1 += 24.0f * xm;
    if (p.on(P::XmodDestShape1)) shape1 += 0.5f * xm;
    if (p.on(P::XmodDestPW1)) pw1 += 0.5f * xm;
    if (p.on(P::XmodDestCutoff)) cutoffOct += 6.0f * xm;
    if (p.on(P::XmodDestMode)) mode += 0.5f * xm;

    // --- filter cutoff -------------------------------------------------------
    const float fv = p[P::FilterVel];
    cutoffOct += 8.0f * p[P::FilterEnvAmt] * fe * (1.0f - fv + fv * velocity_);
    cutoffOct += p[P::FilterKeyTrack] * (pitch_ - 60.0f) / 12.0f;
    cutoffOct += 3.0f * p[P::AtCutoff] * c.aftertouch;
    cutoffOct += 0.3f * vintage * slopCutoff_;
    const float g = SvfFilter::gFromHz(20.0f * std::exp2(cutoffOct), c.sampleRate);
    const float k = SvfFilter::kFromResonance(p[P::FilterRes]);

    // --- oscillators ---------------------------------------------------------
    const float inv = 1.0f / c.sampleRate;
    float wrap2 = -1.0f;
    const float o2 = osc2_.process(noteToHz(p2) * inv, p[P::Osc2Shape], dutyFromPw(pw2), -1.0f, &wrap2);
    const float inc1 = noteToHz(p1) * inv;
    const float o1 = osc1_.process(inc1, shape1, dutyFromPw(pw1), p.on(P::Osc1Sync) ? wrap2 : -1.0f);
    const float sub = sub_.process(inc1 * 0.5f, 1.0f, 0.5f);
    osc2Last_ = o2;

    const float mix = 0.5f * (o1 * p[P::MixOsc1] + o2 * p[P::MixOsc2] + sub * p[P::MixSub] +
                              rng_.bipolar() * p[P::MixNoise]);
    const float y = filter_.process(softClip(1.2f * mix), g, k, mode, p.on(P::FilterBandpass));

    const float av = p[P::AmpVel];
    return y * ae * (1.0f - av + av * velocity_) * ampMod;
}

}  // namespace ob6
