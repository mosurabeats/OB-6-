// MPC OS plugin adapter: exposes the engine through mpc_engine() (wrapper/engine.h
// in sd88me/mpc-vst-plugins), which vst2_wrap.c turns into a native VST2 that
// MPC OS loads directly.
//
// Contract: 44.1 kHz, interleaved int16 stereo, 1..128 frames per render()
// (SAMPLE_ACCURATE). Parameter keys are the engine's keys with '.' -> '_'
// (osc1.freq -> osc1_freq), values are strings in the units of
// mpc/params.json: an option index for toggles and lfo_shape, otherwise the
// number itself. "state" is the whole patch as .ob6 text.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "engine.h"  // C header without extern "C" guards
}
#include "ob6/synth.hpp"

namespace {

using ob6::P;

constexpr float kSampleRate = 44100.0f;
constexpr int kMaxFrames = 128;

struct Instance {
    ob6::Synth synth{kSampleRate, 1};
};

// Wrapper key -> engine param, or nullptr.
const ob6::ParamInfo* lookup(const char* key) {
    for (const auto& info : ob6::allParams()) {
        const char* a = info.key;
        const char* b = key;
        while (*a && *b && (*a == *b || (*a == '.' && *b == '_'))) {
            ++a;
            ++b;
        }
        if (!*a && !*b) return &info;
    }
    return nullptr;
}

void* create(const char* /*data_dir*/) {
    auto* inst = new Instance;
    inst->synth.setCcParamsEnabled(false);  // the wrapper maps CCs/NRPN to parameters
    return inst;
}

void destroy(void* p) { delete static_cast<Instance*>(p); }

void midi(void* p, const uint8_t* msg, int len) {
    if (len > 0) static_cast<Instance*>(p)->synth.handleMidi(msg, static_cast<size_t>(len));
}

void setParam(void* p, const char* key, const char* val) {
    ob6::Synth& synth = static_cast<Instance*>(p)->synth;
    if (!std::strcmp(key, "state")) {
        ob6::Patch patch;
        if (ob6::parsePatch(val, patch, nullptr)) synth.setPatch(patch);
        return;
    }
    if (const ob6::ParamInfo* info = lookup(key)) synth.setParam(info->id, static_cast<float>(std::atof(val)));
}

int getParam(void* p, const char* key, char* buf, int len) {
    const ob6::Synth& synth = static_cast<Instance*>(p)->synth;
    if (len <= 0) return 0;
    if (!std::strcmp(key, "state")) {
        const std::string s = ob6::serializePatch(synth.patch());
        if (static_cast<int>(s.size()) >= len) return 0;
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return static_cast<int>(s.size());
    }
    const ob6::ParamInfo* info = lookup(key);
    if (!info) return 0;
    const float v = synth.patch()[info->id];
    const int n = info->kind == ob6::Kind::Continuous ? std::snprintf(buf, static_cast<size_t>(len), "%g", v)
                                                       : std::snprintf(buf, static_cast<size_t>(len), "%d",
                                                                       static_cast<int>(v + (v < 0 ? -0.5f : 0.5f)));
    return n > 0 && n < len ? n : 0;
}

void render(void* p, int16_t* out, int frames) {
    ob6::Synth& synth = static_cast<Instance*>(p)->synth;
    float left[kMaxFrames], right[kMaxFrames];
    while (frames > 0) {
        const int n = frames < kMaxFrames ? frames : kMaxFrames;
        synth.process(left, right, static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            out[2 * i] = static_cast<int16_t>(ob6::clampf(left[i], -1.0f, 1.0f) * 32767.0f);
            out[2 * i + 1] = static_cast<int16_t>(ob6::clampf(right[i], -1.0f, 1.0f) * 32767.0f);
        }
        out += 2 * n;
        frames -= n;
    }
}

const mpc_engine_t kEngine = {create, destroy, midi, setParam, getParam, render, nullptr, nullptr};

}  // namespace

extern "C" const mpc_engine_t* mpc_engine(void) { return &kEngine; }
