// Patch parameters. The OB6_PARAMS table is the single source of truth for
// names, ranges, defaults and MIDI CC numbers; patch files, the CC map and
// docs/PARAMETERS.md are all derived from it.
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ob6 {

enum class Kind { Continuous, Toggle, Stepped };

// X(id, key, min, max, default, kind, cc, description)
#define OB6_PARAMS(X)                                                                                  \
    X(Osc1Freq, "osc1.freq", -24, 24, 0, Stepped, 14, "Osc 1 coarse tune, semitones")                   \
    X(Osc1Shape, "osc1.shape", 0, 1, 0.5, Continuous, 15, "Osc 1 shape: 0 tri, 0.5 saw, 1 pulse")      \
    X(Osc1PW, "osc1.pw", 0, 1, 0, Continuous, 16, "Osc 1 pulse width: 0 square .. 1 narrow")            \
    X(Osc1Sync, "osc1.sync", 0, 1, 0, Toggle, 17, "Hard-sync osc 1 to osc 2")                           \
    X(Osc2Freq, "osc2.freq", -24, 24, 0, Stepped, 18, "Osc 2 coarse tune, semitones")                   \
    X(Osc2Fine, "osc2.fine", -1, 1, 0.08, Continuous, 19, "Osc 2 fine tune, semitones")                 \
    X(Osc2Shape, "osc2.shape", 0, 1, 0.5, Continuous, 20, "Osc 2 shape: 0 tri, 0.5 saw, 1 pulse")      \
    X(Osc2PW, "osc2.pw", 0, 1, 0, Continuous, 21, "Osc 2 pulse width: 0 square .. 1 narrow")            \
    X(Osc2LowFreq, "osc2.lowfreq", 0, 1, 0, Toggle, 22, "Osc 2 drops 7 octaves (modulator use)")        \
    X(Osc2KeyTrack, "osc2.keytrack", 0, 1, 1, Toggle, 23, "Osc 2 follows the keyboard")                 \
    X(MixOsc1, "mix.osc1", 0, 1, 0.8, Continuous, 24, "Mixer: osc 1 level")                             \
    X(MixOsc2, "mix.osc2", 0, 1, 0.8, Continuous, 25, "Mixer: osc 2 level")                             \
    X(MixSub, "mix.sub", 0, 1, 0, Continuous, 26, "Mixer: osc 1 sub-octave level")                      \
    X(MixNoise, "mix.noise", 0, 1, 0, Continuous, 27, "Mixer: white noise level")                       \
    X(FilterCutoff, "filter.cutoff", 0, 1, 0.6, Continuous, 74, "Cutoff, 20 Hz .. 20 kHz")              \
    X(FilterRes, "filter.resonance", 0, 1, 0.15, Continuous, 71, "Resonance (SEM style, no self-osc)")  \
    X(FilterMode, "filter.mode", 0, 1, 0, Continuous, 28, "Mode morph: 0 LP, 0.5 notch, 1 HP")          \
    X(FilterBandpass, "filter.bandpass", 0, 1, 0, Toggle, 29, "Band-pass, overrides the mode morph")    \
    X(FilterEnvAmt, "filter.envamt", -1, 1, 0.35, Continuous, 30, "Filter envelope amount, +/-8 oct")   \
    X(FilterKeyTrack, "filter.keytrack", 0, 1, 0.5, Continuous, 31, "Cutoff key tracking (1 = full)")   \
    X(FilterVel, "filter.velocity", 0, 1, 0.3, Continuous, 102, "Velocity to filter envelope amount")   \
    X(FEnvAttack, "fenv.attack", 0, 1, 0, Continuous, 103, "Filter env attack, 1 ms .. 20 s")           \
    X(FEnvDecay, "fenv.decay", 0, 1, 0.45, Continuous, 104, "Filter env decay")                         \
    X(FEnvSustain, "fenv.sustain", 0, 1, 0.3, Continuous, 105, "Filter env sustain level")              \
    X(FEnvRelease, "fenv.release", 0, 1, 0.4, Continuous, 106, "Filter env release")                    \
    X(AEnvAttack, "aenv.attack", 0, 1, 0, Continuous, 73, "Amp env attack, 1 ms .. 20 s")               \
    X(AEnvDecay, "aenv.decay", 0, 1, 0.5, Continuous, 75, "Amp env decay")                              \
    X(AEnvSustain, "aenv.sustain", 0, 1, 1, Continuous, 107, "Amp env sustain level")                   \
    X(AEnvRelease, "aenv.release", 0, 1, 0.35, Continuous, 72, "Amp env release")                       \
    X(AmpVel, "aenv.velocity", 0, 1, 0.4, Continuous, 108, "Velocity to amplitude")                     \
    X(LfoRate, "lfo.rate", 0, 1, 0.66, Continuous, 109, "LFO rate, 0.05 .. 51 Hz")                      \
    X(LfoShape, "lfo.shape", 0, 5, 0, Stepped, 110, "sine|triangle|saw|ramp|square|random")             \
    X(LfoAmount, "lfo.amount", 0, 1, 0, Continuous, 111, "LFO initial amount")                          \
    X(LfoDestFreq1, "lfo.dest.freq1", 0, 1, 1, Toggle, 112, "LFO -> osc 1 pitch (+/-12 st)")            \
    X(LfoDestFreq2, "lfo.dest.freq2", 0, 1, 1, Toggle, 113, "LFO -> osc 2 pitch (+/-12 st)")            \
    X(LfoDestPW, "lfo.dest.pw", 0, 1, 0, Toggle, 114, "LFO -> both pulse widths")                       \
    X(LfoDestCutoff, "lfo.dest.cutoff", 0, 1, 0, Toggle, 115, "LFO -> cutoff (+/-5 oct)")               \
    X(LfoDestMode, "lfo.dest.mode", 0, 1, 0, Toggle, 116, "LFO -> filter mode")                         \
    X(LfoDestAmp, "lfo.dest.amp", 0, 1, 0, Toggle, 117, "LFO -> amplitude (tremolo)")                   \
    X(XmodFenv, "xmod.fenv", -1, 1, 0, Continuous, 118, "X-Mod source: filter env amount")              \
    X(XmodOsc2, "xmod.osc2", 0, 1, 0, Continuous, 119, "X-Mod source: osc 2 amount (audio rate)")       \
    X(XmodDestFreq1, "xmod.dest.freq1", 0, 1, 0, Toggle, 85, "X-Mod -> osc 1 pitch (+/-24 st)")         \
    X(XmodDestShape1, "xmod.dest.shape1", 0, 1, 0, Toggle, 86, "X-Mod -> osc 1 shape")                  \
    X(XmodDestPW1, "xmod.dest.pw1", 0, 1, 0, Toggle, 87, "X-Mod -> osc 1 pulse width")                  \
    X(XmodDestCutoff, "xmod.dest.cutoff", 0, 1, 0, Toggle, 88, "X-Mod -> cutoff (+/-6 oct)")            \
    X(XmodDestMode, "xmod.dest.mode", 0, 1, 0, Toggle, 89, "X-Mod -> filter mode")                      \
    X(Glide, "glide", 0, 1, 0, Continuous, 5, "Glide time, 0 = off .. ~2 s")                            \
    X(Unison, "unison", 0, 1, 0, Toggle, 90, "Unison: stack voices on one note")                        \
    X(UnisonVoices, "unison.voices", 1, 6, 6, Stepped, -1, "Voices stacked in unison")                  \
    X(UnisonDetune, "unison.detune", 0, 1, 0.25, Continuous, 94, "Unison spread, up to +/-25 cents")    \
    X(Voices, "voices", 1, 6, 6, Stepped, -1, "Polyphony")                                              \
    X(Vintage, "vintage", 0, 1, 0.25, Continuous, 95, "Per-voice tuning/filter/env slop and drift")     \
    X(BendRange, "bend.range", 0, 12, 2, Stepped, -1, "Pitch bend range, semitones")                    \
    X(WheelLfo, "wheel.lfo", 0, 1, 0.35, Continuous, -1, "Mod wheel adds to LFO amount")                \
    X(AtCutoff, "at.cutoff", 0, 1, 0, Continuous, -1, "Aftertouch -> cutoff (+3 oct)")                  \
    X(AtLfo, "at.lfo", 0, 1, 0, Continuous, -1, "Aftertouch adds to LFO amount")                        \
    X(ChorusMix, "fx.chorus.mix", 0, 1, 0, Continuous, 91, "Chorus wet mix")                            \
    X(ChorusRate, "fx.chorus.rate", 0, 1, 0.3, Continuous, 92, "Chorus rate, 0.1 .. 5 Hz")              \
    X(ChorusDepth, "fx.chorus.depth", 0, 1, 0.5, Continuous, 93, "Chorus depth")                        \
    X(DelayMix, "fx.delay.mix", 0, 1, 0, Continuous, 12, "Delay wet mix")                               \
    X(DelayTime, "fx.delay.time", 0, 1, 0.5, Continuous, 13, "Delay time, 10 ms .. 1.5 s")              \
    X(DelayFeedback, "fx.delay.feedback", 0, 0.95, 0.35, Continuous, 3, "Delay feedback (ping-pong)")   \
    X(Volume, "volume", 0, 1, 0.75, Continuous, 7, "Master volume")

enum class P : int {
#define OB6_ENUM(id, ...) id,
    OB6_PARAMS(OB6_ENUM)
#undef OB6_ENUM
        Count
};

constexpr int kNumParams = static_cast<int>(P::Count);

struct ParamInfo {
    P id;
    const char* key;
    float min, max, def;
    Kind kind;
    int cc;  // -1 = not CC-mapped
    const char* description;
};

const ParamInfo& paramInfo(P p);
const std::array<ParamInfo, kNumParams>& allParams();
const ParamInfo* findParam(std::string_view key);
const ParamInfo* findParamByCC(int cc);

struct Patch {
    std::string name = "Init";
    std::array<float, kNumParams> values{};

    Patch();
    float operator[](P p) const { return values[static_cast<int>(p)]; }
    float& operator[](P p) { return values[static_cast<int>(p)]; }
    bool on(P p) const { return (*this)[p] >= 0.5f; }
    int steps(P p) const { return static_cast<int>((*this)[p] + 0.5f); }
    void set(P p, float v);  // clamps (and rounds Stepped/Toggle) to the param range
};

// Patch text format: one `key = value` per line, `#` starts a comment.
// `name = ...` sets the patch name. Toggles accept on/off/true/false/1/0;
// lfo.shape also accepts its shape names. Unknown keys are errors.
bool parsePatch(std::string_view text, Patch& out, std::vector<std::string>* errors);
bool loadPatchFile(const std::string& path, Patch& out, std::vector<std::string>* errors);
std::string serializePatch(const Patch& patch);

}  // namespace ob6
