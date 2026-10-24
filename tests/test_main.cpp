// Engine and renderer tests. Run from the repository root (ctest does this).

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "keygroup.hpp"
#include "ob6/envelope.hpp"
#include "ob6/filter.hpp"
#include "ob6/oscillator.hpp"
#include "ob6/synth.hpp"
#include "wav.hpp"
#include "xpm.hpp"

using ob6::P;

namespace {

int g_failures = 0;
std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> r;
    return r;
}
struct Register {
    Register(const char* name, std::function<void()> fn) { registry().emplace_back(name, std::move(fn)); }
};

#define TEST(name)                               \
    void name();                                 \
    const Register reg_##name(#name, name);      \
    void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

float sineGain(const std::function<float(float)>& process, float hz, float fs) {
    // Skip the transient, then compare output RMS with input RMS.
    double in2 = 0, out2 = 0;
    for (int i = 0; i < static_cast<int>(fs); ++i) {
        const float x = std::sin(ob6::kTwoPi * hz * static_cast<float>(i) / fs);
        const float y = process(x);
        if (i > fs / 4) {
            in2 += x * x;
            out2 += y * y;
        }
    }
    return static_cast<float>(std::sqrt(out2 / in2));
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

size_t countOf(const std::string& hay, const std::string& needle) {
    size_t n = 0;
    for (size_t pos = hay.find(needle); pos != std::string::npos; pos = hay.find(needle, pos + 1)) ++n;
    return n;
}

}  // namespace

TEST(oscillator_tracks_pitch) {
    const float fs = 48000.0f;
    ob6::BlepOscillator osc;
    // The saw ramp crosses zero upward exactly once per cycle (the BLEP-
    // smoothed reset is a downward step, so it never adds a crossing).
    int crossings = 0;
    float prev = 0.0f;
    for (int i = 0; i < 48000; ++i) {
        const float y = osc.process(440.0f / fs, 0.5f, 0.5f);
        if (prev < 0.0f && y >= 0.0f) ++crossings;
        prev = y;
    }
    CHECK(std::abs(crossings - 440) <= 1);
}

// Fraction of spectral energy (dB) outside the harmonics of `bin`, for a
// signal whose fundamental sits exactly on FFT bin `bin`.
float inharmonicDb(std::vector<float> x, int bin) {
    const size_t n = x.size();  // power of two
    std::vector<double> re(x.begin(), x.end()), im(n, 0.0);
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(re[i], re[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979323846 / static_cast<double>(len);
        for (size_t i = 0; i < n; i += len)
            for (size_t k = 0; k < len / 2; ++k) {
                const double wr = std::cos(ang * k), wi = std::sin(ang * k);
                const size_t a = i + k, b = a + len / 2;
                const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
    }
    double harmonic = 0, other = 0;
    for (size_t k = 1; k < n / 2; ++k) {
        const double e = re[k] * re[k] + im[k] * im[k];
        (k % static_cast<size_t>(bin) == 0 ? harmonic : other) += e;
    }
    return static_cast<float>(10.0 * std::log10(other / harmonic));
}

TEST(blep_saw_suppresses_aliasing) {
    // fs = N so bins are 1 Hz; 2560 Hz has 12 harmonics below Nyquist and
    // aliases that land between them.
    const size_t n = 65536;
    const int bin = 2560;
    ob6::BlepOscillator osc;
    std::vector<float> blep(n), naive(n);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i) {
        blep[i] = osc.process(static_cast<float>(bin) / n, 0.5f, 0.5f);
        naive[i] = static_cast<float>(2.0 * phase - 1.0);
        phase += static_cast<double>(bin) / n;
        if (phase >= 1.0) phase -= 1.0;
    }
    const float blepDb = inharmonicDb(blep, bin), naiveDb = inharmonicDb(naive, bin);
    std::printf("  aliasing: blep %.1f dB, naive %.1f dB\n", blepDb, naiveDb);
    // 2-point polyBLEP measures -28.2 dB here, identical to the textbook
    // formulation; naive is ~-13 dB.
    CHECK(blepDb < -27.0f);
    CHECK(blepDb < naiveDb - 12.0f);
}

TEST(oscillator_stays_bounded_under_sweeps_and_sync) {
    ob6::BlepOscillator master, slave;
    ob6::Rng rng(7);
    bool finite = true;
    float maxAbs = 0.0f;
    for (int i = 0; i < 200000; ++i) {
        const float t = static_cast<float>(i) / 200000.0f;
        float wrap = -1.0f;
        master.process(0.001f + 0.05f * t, rng.uniform(), rng.uniform(), -1.0f, &wrap);
        const float y = slave.process(0.002f + 0.3f * t, std::fmod(t * 7.0f, 1.0f), 0.05f + 0.9f * t, wrap);
        finite = finite && std::isfinite(y);
        maxAbs = std::max(maxAbs, std::fabs(y));
    }
    CHECK(finite);
    CHECK(maxAbs < 2.5f);
}

TEST(hard_sync_locks_slave_to_master_period) {
    const float fs = 44000.0f;  // master period is exactly 200 samples
    ob6::BlepOscillator master, slave;
    std::vector<float> y;
    for (int i = 0; i < 4000; ++i) {
        float wrap = -1.0f;
        master.process(220.0f / fs, 0.5f, 0.5f, -1.0f, &wrap);
        y.push_back(slave.process(733.0f / fs, 0.5f, 0.5f, wrap));
    }
    float maxDiff = 0.0f;
    for (size_t i = 1000; i + 200 < y.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(y[i] - y[i + 200]));
    CHECK(maxDiff < 0.02f);
}

TEST(filter_modes_shape_the_spectrum) {
    const float fs = 48000.0f;
    const float g = ob6::SvfFilter::gFromHz(500.0f, fs);
    const float k = ob6::SvfFilter::kFromResonance(0.0f);
    auto run = [&](float mode, bool bp, float hz) {
        ob6::SvfFilter f;
        return sineGain([&](float x) { return f.process(x * 0.25f, g, k, mode, bp) * 4.0f; }, hz, fs);
    };
    CHECK(run(0.0f, false, 50.0f) > 0.9f);     // LP passes lows
    CHECK(run(0.0f, false, 10000.0f) < 0.01f); // ... and cuts highs (12 dB/oct)
    CHECK(run(1.0f, false, 10000.0f) > 0.9f);  // HP passes highs
    CHECK(run(1.0f, false, 50.0f) < 0.02f);
    CHECK(run(0.5f, false, 500.0f) < 0.05f);   // notch at cutoff
    CHECK(run(0.0f, true, 500.0f) > 0.9f);     // BP peak
    CHECK(run(0.0f, true, 10000.0f) < 0.1f);
}

TEST(envelope_runs_adsr_and_goes_idle) {
    const float fs = 48000.0f;
    ob6::Envelope env;
    env.setSampleRate(fs);
    env.set(0.01f, 0.1f, 0.5f, 0.05f);
    env.gateOn();
    int toPeak = 0;
    while (env.process() < 1.0f && toPeak < 48000) ++toPeak;
    CHECK(toPeak > 0.005f * fs && toPeak < 0.015f * fs);
    for (int i = 0; i < 24000; ++i) env.process();
    CHECK(env.stage() == ob6::Envelope::Stage::Sustain);
    CHECK(std::fabs(env.value() - 0.5f) < 1e-4f);
    env.gateOff();
    for (int i = 0; i < 48000 && !env.idle(); ++i) env.process();
    CHECK(env.idle());
}

TEST(polyphony_is_six_with_voice_stealing) {
    ob6::Synth synth(48000.0f);
    std::vector<float> l(256), r(256);
    for (int n = 60; n < 67; ++n) synth.noteOn(n, 100);
    synth.process(l.data(), r.data(), l.size());
    CHECK(synth.activeVoices() == 6);
    for (int n = 60; n < 67; ++n) synth.noteOff(n);
    for (int i = 0; i < 2000; ++i) synth.process(l.data(), r.data(), l.size());
    CHECK(synth.activeVoices() == 0);
}

TEST(voices_param_limits_polyphony) {
    ob6::Synth synth(48000.0f);
    ob6::Patch p;
    p.set(P::Voices, 2);
    synth.setPatch(p);
    for (int n = 60; n < 65; ++n) synth.noteOn(n, 100);
    CHECK(synth.activeVoices() == 2);
}

TEST(unison_stacks_voices_and_returns_to_held_note) {
    ob6::Synth synth(48000.0f);
    ob6::Patch p;
    p.set(P::Unison, 1);
    p.set(P::UnisonVoices, 4);
    synth.setPatch(p);
    synth.noteOn(60, 100);
    CHECK(synth.activeVoices() == 4);
    synth.noteOn(64, 100);
    synth.noteOff(64);
    CHECK(synth.activeVoices() == 4);  // legato back to 60, still sounding
}

TEST(sustain_pedal_holds_notes) {
    ob6::Synth synth(48000.0f);
    std::vector<float> l(256), r(256);
    synth.setSustain(true);
    synth.noteOn(60, 100);
    synth.noteOff(60);
    for (int i = 0; i < 500; ++i) synth.process(l.data(), r.data(), l.size());
    CHECK(synth.activeVoices() == 1);
    synth.setSustain(false);
    for (int i = 0; i < 2000; ++i) synth.process(l.data(), r.data(), l.size());
    CHECK(synth.activeVoices() == 0);
}

TEST(midi_messages_drive_the_synth) {
    ob6::Synth synth(48000.0f);
    const uint8_t on[3] = {0x90, 60, 100};
    const uint8_t cc[3] = {0xB0, 74, 0};
    synth.handleMidi(on, 3);
    synth.handleMidi(cc, 3);
    CHECK(synth.activeVoices() == 1);
    CHECK(synth.patch()[P::FilterCutoff] == 0.0f);
}

TEST(random_patches_never_produce_nan) {
    ob6::Rng rng(99);
    std::vector<float> l(512), r(512);
    for (int trial = 0; trial < 40; ++trial) {
        ob6::Patch p;
        for (const auto& info : ob6::allParams()) p.set(info.id, info.min + (info.max - info.min) * rng.uniform());
        ob6::Synth synth(44100.0f, static_cast<uint32_t>(trial + 1));
        synth.setPatch(p);
        synth.setModWheel(rng.uniform());
        synth.setAftertouch(rng.uniform());
        for (int n = 0; n < 6; ++n) synth.noteOn(24 + static_cast<int>(rng.uniform() * 80), 127);
        bool ok = true;
        for (int b = 0; b < 40; ++b) {
            synth.process(l.data(), r.data(), l.size());
            for (size_t i = 0; i < l.size(); ++i)
                ok = ok && std::isfinite(l[i]) && std::isfinite(r[i]) && std::fabs(l[i]) <= 1.0f;
        }
        CHECK(ok);
    }
}

TEST(patch_text_round_trips) {
    ob6::Patch p;
    std::vector<std::string> errors;
    CHECK(ob6::parsePatch("name = Test Brass\nosc1.sync = on  # comment\nlfo.shape = square\n"
                          "filter.cutoff = 0.25\n",
                          p, &errors));
    CHECK(errors.empty());
    CHECK(p.name == "Test Brass");
    CHECK(p.on(P::Osc1Sync));
    CHECK(p.steps(P::LfoShape) == 4);
    ob6::Patch q;
    CHECK(ob6::parsePatch(ob6::serializePatch(p), q, nullptr));
    CHECK(q.name == p.name);
    for (int i = 0; i < ob6::kNumParams; ++i) CHECK(std::fabs(q.values[i] - p.values[i]) < 1e-3f);
}

TEST(patch_parser_rejects_bad_input) {
    ob6::Patch p;
    std::vector<std::string> errors;
    CHECK(!ob6::parsePatch("filter.cutof = 0.5\n", p, &errors));
    CHECK(!ob6::parsePatch("filter.cutoff = 2\n", p, &errors));
    CHECK(!ob6::parsePatch("filter.cutoff = loud\n", p, &errors));
    CHECK(!ob6::parsePatch("just words\n", p, &errors));
    CHECK(errors.size() == 4);
}

TEST(param_table_is_consistent) {
    std::set<int> ccs;
    std::set<std::string> keys;
    for (const auto& info : ob6::allParams()) {
        CHECK(info.min < info.max);
        CHECK(info.def >= info.min && info.def <= info.max);
        CHECK(keys.insert(info.key).second);
        if (info.cc >= 0) {
            CHECK(ccs.insert(info.cc).second);
            CHECK(info.cc != 1 && info.cc != 64 && info.cc < 120);  // reserved
        }
    }
}

TEST(shipped_patches_parse) {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator("patches")) {
        if (entry.path().extension() != ".ob6") continue;
        ob6::Patch p;
        std::vector<std::string> errors;
        const bool ok = ob6::loadPatchFile(entry.path().string(), p, &errors);
        if (!ok) std::printf("  %s: %s\n", entry.path().c_str(), errors.empty() ? "?" : errors[0].c_str());
        CHECK(ok);
        ++count;
    }
    CHECK(count >= 6);
}

TEST(loop_crossfade_is_seamless) {
    ob6::Rng rng(3);
    std::vector<float> x(20000);
    for (float& s : x) s = rng.bipolar();
    const size_t loopStart = 5000, loopEnd = 15000, xf = 2000;
    std::vector<float> y = x;
    ob6tools::applyLoopCrossfade(y, loopStart, loopEnd, xf);
    CHECK(y.size() == loopEnd);
    // The wrap from loopEnd-1 to loopStart behaves like x[loopStart-1] -> x[loopStart].
    CHECK(std::fabs(y[loopEnd - 1] - x[loopStart - 1]) < 0.01f);
    for (size_t i = 0; i < loopEnd - xf; ++i) CHECK(y[i] == x[i]);
}

TEST(xpm_maps_zones_layers_and_root_offset) {
    ob6tools::XpmProgram prog;
    prog.name = "A&B";
    prog.rootOffset = 1;
    ob6tools::XpmZone z;
    z.lowNote = 0;
    z.highNote = 61;
    z.layers.push_back({0, 127, "A&B_60_C3", 60, 1000, true, 100, 899});
    prog.zones = {z, z};
    const std::string xml = ob6tools::buildKeygroupXpm(prog);
    CHECK(countOf(xml, "<Instrument number=") == 2);
    CHECK(countOf(xml, "<Layer number=") == 8);
    CHECK(countOf(xml, "<RootNote>61</RootNote>") == 2);
    CHECK(xml.find("<KeygroupNumKeygroups>2</KeygroupNumKeygroups>") != std::string::npos);
    CHECK(xml.find("A&amp;B") != std::string::npos);
    CHECK(xml.find("A&B") == std::string::npos);
}

TEST(keygroup_render_writes_usb_folder) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "ob6tests_keygroup";
    fs::remove_all(dir);
    ob6::Patch p;
    p.name = "Test Pad";
    ob6tools::KeygroupOptions opts;
    opts.lowNote = 48;
    opts.highNote = 60;
    opts.step = 6;
    opts.loopSeconds = 0.5f;
    std::string log, err;
    CHECK(ob6tools::buildKeygroup(p, opts, dir.string(), &log, &err));
    const fs::path prog = dir / "Test Pad";
    const std::string xml = readFile(prog / "Test Pad.xpm");
    CHECK(countOf(xml, "<Instrument number=") == 3);
    CHECK(countOf(xml, "<Loop>True</Loop>") == 3);
    CHECK(xml.find("<LowNote>52</LowNote>") != std::string::npos);  // 48 covers 0-51, 54 covers 52-57
    const std::string wav = readFile(prog / "Test Pad_54_Gb2.wav");
    CHECK(wav.size() > 44);
    CHECK(wav.compare(0, 4, "RIFF") == 0 && wav.compare(8, 4, "WAVE") == 0);
    CHECK(wav.find("smpl") != std::string::npos);
    fs::remove_all(dir);
}

TEST(plucks_render_one_shot_and_trim_tail) {
    ob6::Patch p;
    p.set(P::AEnvSustain, 0.0f);
    p.set(P::AEnvDecay, 0.35f);  // ~32 ms .. a short pluck
    ob6tools::KeygroupOptions opts;
    const ob6tools::RenderedNote n = ob6tools::renderNote(p, 60, 127, opts);
    CHECK(!n.loop);
    CHECK(n.left.size() < static_cast<size_t>(2.0f * 44100.0f));
    CHECK(n.left.size() > static_cast<size_t>(0.02f * 44100.0f));
}

int main() {
    for (auto& [name, fn] : registry()) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
