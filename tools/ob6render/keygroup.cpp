#include "keygroup.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "ob6/synth.hpp"
#include "wav.hpp"
#include "xpm.hpp"

namespace ob6tools {

using ob6::P;

namespace {

float seconds(const ob6::Patch& p, P param) { return ob6::timeParamToSeconds(p[param]); }

uint32_t seedFor(int note, int velocity) { return static_cast<uint32_t>(note * 131 + velocity * 7 + 17); }

void renderInto(ob6::Synth& synth, RenderedNote& out, size_t frames) {
    const size_t start = out.left.size();
    out.left.resize(start + frames);
    out.right.resize(start + frames);
    synth.process(out.left.data() + start, out.right.data() + start, frames);
}

}  // namespace

std::string noteName(int n) {
    static const char* names[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    return std::string(names[((n % 12) + 12) % 12]) + std::to_string(n / 12 - 2);
}

std::string safeFileName(const std::string& name) {
    std::string r;
    for (char c : name) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_';
        r += ok ? c : '_';
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r.empty() ? "OB6 Patch" : r;
}

void applyLoopCrossfade(std::vector<float>& ch, size_t loopStart, size_t loopEnd, size_t xf) {
    xf = std::min({xf, loopStart, loopEnd - loopStart});
    for (size_t i = 0; i < xf; ++i) {
        // Equal power: the two regions are mostly uncorrelated (detuned
        // oscillators, drift), so this keeps the level steady.
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(xf);
        const float fadeOut = std::cos(0.5f * ob6::kPi * t);
        const float fadeIn = std::sin(0.5f * ob6::kPi * t);
        const size_t dst = loopEnd - xf + i;
        ch[dst] = ch[dst] * fadeOut + ch[loopStart - xf + i] * fadeIn;
    }
    ch.resize(loopEnd);
}

RenderedNote renderNote(const ob6::Patch& patch, int note, int velocity, const KeygroupOptions& opts) {
    const float fs = static_cast<float>(opts.sampleRate);
    ob6::Synth synth(fs, seedFor(note, velocity));
    synth.setPatch(patch);
    synth.noteOn(note, velocity);

    RenderedNote out;
    out.root = note;
    out.velocity = velocity;

    const float settle =
        std::max(seconds(patch, P::AEnvAttack) + seconds(patch, P::AEnvDecay),
                 seconds(patch, P::FEnvAttack) + seconds(patch, P::FEnvDecay)) + 0.3f;
    const bool sustains = patch[P::AEnvSustain] >= 0.02f;
    const bool loop = opts.loop == KeygroupOptions::Loop::On ||
                      (opts.loop == KeygroupOptions::Loop::Auto && sustains);

    if (loop) {
        const float maxSettle = std::max(0.0f, opts.maxSeconds - opts.loopSeconds - opts.crossfadeSeconds);
        const size_t xf = static_cast<size_t>(opts.crossfadeSeconds * fs);
        const size_t loopStart = static_cast<size_t>(std::min(settle, maxSettle) * fs) + xf;
        // Whole number of fundamental periods keeps the static part of the
        // waveform phase-aligned across the loop point.
        const double period = fs / ob6::noteToHz(static_cast<float>(note));
        const double periods = std::max(1.0, std::round(opts.loopSeconds * fs / period));
        const size_t loopLen = static_cast<size_t>(std::lround(periods * period));
        renderInto(synth, out, loopStart + loopLen);
        applyLoopCrossfade(out.left, loopStart, loopStart + loopLen, xf);
        applyLoopCrossfade(out.right, loopStart, loopStart + loopLen, xf);
        out.loop = true;
        out.loopStart = static_cast<uint32_t>(loopStart);
        out.loopEnd = static_cast<uint32_t>(loopStart + loopLen - 1);
        return out;
    }

    // One-shot: hold through the decay (or `settle + 1 s` if the patch
    // sustains), release, and stop once the tail is ~72 dB down.
    const size_t holdFrames = static_cast<size_t>((sustains ? settle + 1.0f : opts.maxSeconds) * fs);
    const size_t maxFrames = static_cast<size_t>(opts.maxSeconds * fs);
    const size_t minFrames = static_cast<size_t>((seconds(patch, P::AEnvAttack) + 0.05f) * fs);
    constexpr size_t kBlock = 512;
    float peak = 0.0f;
    size_t quiet = 0;
    bool released = false;
    while (out.left.size() < maxFrames) {
        if (!released && out.left.size() >= holdFrames) {
            synth.noteOff(note);
            released = true;
        }
        const size_t start = out.left.size();
        renderInto(synth, out, kBlock);
        float blockPeak = 0.0f;
        for (size_t i = start; i < out.left.size(); ++i)
            blockPeak = std::max({blockPeak, std::fabs(out.left[i]), std::fabs(out.right[i])});
        peak = std::max(peak, blockPeak);
        quiet = (peak > 0.0f && blockPeak < peak * 2.5e-4f) ? quiet + kBlock : 0;
        if (out.left.size() > minFrames && quiet >= 8192) {
            out.left.resize(out.left.size() - quiet + kBlock);
            out.right.resize(out.left.size());
            break;
        }
    }
    const size_t fade = std::min<size_t>(out.left.size(), static_cast<size_t>(0.005f * fs));
    for (size_t i = 0; i < fade; ++i) {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        out.left[out.left.size() - 1 - i] *= g;
        out.right[out.right.size() - 1 - i] *= g;
    }
    return out;
}

bool buildKeygroup(const ob6::Patch& patch, const KeygroupOptions& opts, const std::string& outDir,
                   std::string* log, std::string* error) {
    namespace fs = std::filesystem;
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    if (opts.lowNote < 0 || opts.highNote > 127 || opts.lowNote > opts.highNote || opts.step < 1)
        return fail("bad note range");
    const int layers = std::clamp(opts.velocityLayers, 1, 4);

    const std::string program = safeFileName(patch.name);
    const fs::path dir = fs::path(outDir) / program;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return fail("cannot create " + dir.string() + ": " + ec.message());

    std::vector<int> roots;
    for (int n = opts.lowNote; n <= opts.highNote; n += opts.step) roots.push_back(n);

    struct Job {
        int zone, velStart, velEnd;
        RenderedNote audio;
    };
    std::vector<Job> jobs;
    float peak = 0.0f;
    for (size_t z = 0; z < roots.size(); ++z) {
        for (int l = 0; l < layers; ++l) {
            const int velStart = l == 0 ? 0 : (127 * l) / layers + 1;
            const int velEnd = (127 * (l + 1)) / layers;
            Job job{static_cast<int>(z), velStart, velEnd, renderNote(patch, roots[z], std::max(1, velEnd), opts)};
            for (size_t i = 0; i < job.audio.left.size(); ++i)
                peak = std::max({peak, std::fabs(job.audio.left[i]), std::fabs(job.audio.right[i])});
            jobs.push_back(std::move(job));
        }
    }
    if (peak <= 0.0f) return fail("patch rendered silence");
    // One gain for the whole program keeps the keyboard's level balance.
    const float gain = std::min(0.891f / peak, 16.0f);

    XpmProgram xpm;
    xpm.name = program;
    xpm.rootOffset = opts.rootOffset;
    xpm.releaseSeconds = seconds(patch, P::AEnvRelease);
    xpm.mono = patch.on(P::Unison);
    for (size_t z = 0; z < roots.size(); ++z) {
        XpmZone zone;
        zone.lowNote = z == 0 ? 0 : roots[z - 1] + (roots[z] - roots[z - 1]) / 2 + 1;
        zone.highNote = z + 1 == roots.size() ? 127 : roots[z] + (roots[z + 1] - roots[z]) / 2;
        xpm.zones.push_back(zone);
    }

    size_t bytes = 0;
    for (Job& job : jobs) {
        RenderedNote& a = job.audio;
        for (float& s : a.left) s *= gain;
        for (float& s : a.right) s *= gain;
        const std::string sample = program + "_" + std::to_string(a.root) + "_" + noteName(a.root) +
                                   (layers > 1 ? "_v" + std::to_string(job.velEnd) : "");
        WavInfo info;
        info.sampleRate = opts.sampleRate;
        info.bits = opts.bits;
        info.rootNote = a.root;
        info.hasLoop = a.loop;
        info.loop = {a.loopStart, a.loopEnd};
        std::vector<std::vector<float>> channels;
        if (opts.mono) {
            std::vector<float> m(a.left.size());
            for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (a.left[i] + a.right[i]);
            channels.push_back(std::move(m));
        } else {
            channels = {std::move(a.left), std::move(a.right)};
        }
        const fs::path wav = dir / (sample + ".wav");
        std::string werr;
        if (!writeWav(wav.string(), channels, info, &werr)) return fail(werr);
        bytes += fs::file_size(wav, ec);

        XpmLayer layer;
        layer.velStart = job.velStart;
        layer.velEnd = job.velEnd;
        layer.sampleName = sample;
        layer.rootNote = a.root;
        layer.frames = static_cast<uint32_t>(channels[0].size());
        layer.loop = a.loop;
        layer.loopStart = a.loopStart;
        layer.loopEnd = a.loopEnd;
        xpm.zones[static_cast<size_t>(job.zone)].layers.push_back(layer);
    }

    const fs::path xpmPath = dir / (program + ".xpm");
    std::ofstream(xpmPath, std::ios::binary) << buildKeygroupXpm(xpm);
    std::ofstream(dir / (program + ".ob6"), std::ios::binary) << ob6::serializePatch(patch);
    if (!fs::exists(xpmPath)) return fail("cannot write " + xpmPath.string());

    if (log) {
        char gainDb[16];
        std::snprintf(gainDb, sizeof gainDb, "%+.1f", 20.0 * std::log10(gain));
        *log += program + ": " + std::to_string(roots.size()) + " keygroups x " + std::to_string(layers) +
                " layer(s), " + (jobs.front().audio.loop ? "looped" : "one-shot") + ", " +
                std::to_string(bytes / 1024) + " KiB, gain " + gainDb + " dB -> " + xpmPath.string() + "\n";
    }
    return true;
}

}  // namespace ob6tools
