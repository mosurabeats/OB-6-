// ob6render: turns OB-6 clone patches into MPC Keygroup programs that load
// from a USB drive on stock MPC firmware, plus auditioning helpers.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "keygroup.hpp"
#include "ob6/synth.hpp"
#include "wav.hpp"

using namespace ob6tools;
using ob6::P;

namespace {

int usage() {
    std::fputs(
        "usage:\n"
        "  ob6render keygroup <patch.ob6>... -o <usb-dir> [options]\n"
        "      --low N --high N --step N   sampled root range (MIDI, default 24 96 3)\n"
        "      --layers N                  velocity layers 1-4 (default 1)\n"
        "      --rate HZ --bits 16|24      default 44100 / 24\n"
        "      --loop auto|on|off          sustain looping (default auto)\n"
        "      --loop-seconds S            loop length (default 2)\n"
        "      --root-offset N             XPM RootNote offset (default 1, see calibrate)\n"
        "      --mono                      write mono samples\n"
        "  ob6render calibrate -o <usb-dir> [--root-offset N]\n"
        "  ob6render demo <patch.ob6> -o <file.wav>\n"
        "  ob6render params [--markdown]\n"
        "  ob6render dump <patch.ob6>\n",
        stderr);
    return 2;
}

bool loadPatch(const std::string& path, ob6::Patch& patch) {
    std::vector<std::string> errors;
    if (ob6::loadPatchFile(path, patch, &errors)) return true;
    for (const auto& e : errors) std::fprintf(stderr, "%s: %s\n", path.c_str(), e.c_str());
    return false;
}

struct Args {
    std::vector<std::string> positional;
    std::string out;
    KeygroupOptions kg;
    bool markdown = false;
    bool ok = true;
};

Args parseArgs(int argc, char** argv, int first) {
    Args a;
    for (int i = first; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", arg.c_str());
                a.ok = false;
                return "0";
            }
            return argv[++i];
        };
        if (arg == "-o" || arg == "--out") a.out = next();
        else if (arg == "--low") a.kg.lowNote = std::atoi(next());
        else if (arg == "--high") a.kg.highNote = std::atoi(next());
        else if (arg == "--step") a.kg.step = std::atoi(next());
        else if (arg == "--layers") a.kg.velocityLayers = std::atoi(next());
        else if (arg == "--rate") a.kg.sampleRate = static_cast<uint32_t>(std::atoi(next()));
        else if (arg == "--bits") a.kg.bits = std::atoi(next());
        else if (arg == "--loop-seconds") a.kg.loopSeconds = static_cast<float>(std::atof(next()));
        else if (arg == "--root-offset") a.kg.rootOffset = std::atoi(next());
        else if (arg == "--mono") a.kg.mono = true;
        else if (arg == "--markdown") a.markdown = true;
        else if (arg == "--loop") {
            const std::string m = next();
            if (m == "auto") a.kg.loop = KeygroupOptions::Loop::Auto;
            else if (m == "on") a.kg.loop = KeygroupOptions::Loop::On;
            else if (m == "off") a.kg.loop = KeygroupOptions::Loop::Off;
            else a.ok = false;
        } else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "unknown option %s\n", arg.c_str());
            a.ok = false;
        } else {
            a.positional.push_back(arg);
        }
    }
    if (a.kg.bits != 16 && a.kg.bits != 24) a.ok = false;
    if (a.kg.sampleRate < 8000) a.ok = false;
    return a;
}

int cmdKeygroup(const Args& a) {
    if (a.positional.empty() || a.out.empty()) return usage();
    int failures = 0;
    for (const auto& path : a.positional) {
        ob6::Patch patch;
        if (!loadPatch(path, patch)) {
            ++failures;
            continue;
        }
        std::string log, err;
        if (buildKeygroup(patch, a.kg, a.out, &log, &err)) {
            std::fputs(log.c_str(), stdout);
        } else {
            std::fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str());
            ++failures;
        }
    }
    return failures ? 1 : 0;
}

int cmdCalibrate(Args a) {
    if (a.out.empty()) return usage();
    ob6::Patch p;
    p.name = "OB6 Calibrate";
    p.set(P::Osc1Shape, 0.0f);  // pure triangle: easy for a tuner to read
    p.set(P::MixOsc2, 0.0f);
    p.set(P::FilterCutoff, 1.0f);
    p.set(P::FilterRes, 0.0f);
    p.set(P::FilterEnvAmt, 0.0f);
    p.set(P::FilterKeyTrack, 0.0f);
    p.set(P::AEnvSustain, 1.0f);
    p.set(P::AmpVel, 0.0f);
    p.set(P::Vintage, 0.0f);
    p.set(P::WheelLfo, 0.0f);
    a.kg.lowNote = 36;
    a.kg.highNote = 84;
    a.kg.step = 12;
    a.kg.velocityLayers = 1;
    std::string log, err;
    if (!buildKeygroup(p, a.kg, a.out, &log, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::fputs(log.c_str(), stdout);
    std::ofstream txt(std::filesystem::path(a.out) / safeFileName(p.name) / "CALIBRATE.txt");
    txt << "Load 'OB6 Calibrate.xpm' on the MPC and play these keys with a tuner on the output.\n"
           "Akai names MIDI note 60 'C3'.\n\n";
    for (int n : {36, 48, 60, 72, 84}) {
        char line[96];
        std::snprintf(line, sizeof line, "  MIDI %d (%s): %.2f Hz\n", n, noteName(n).c_str(),
                      ob6::noteToHz(static_cast<float>(n)));
        txt << line;
    }
    txt << "\nIf every key reads one semitone sharp, re-run with --root-offset " << a.kg.rootOffset + 1
        << ";\nif one semitone flat, use --root-offset " << a.kg.rootOffset - 1 << ".\n";
    return 0;
}

int cmdDemo(const Args& a) {
    if (a.positional.size() != 1 || a.out.empty()) return usage();
    ob6::Patch patch;
    if (!loadPatch(a.positional[0], patch)) return 1;
    const float fs = static_cast<float>(a.kg.sampleRate);
    ob6::Synth synth(fs, 1);
    synth.setPatch(patch);

    std::vector<float> left, right;
    auto run = [&](float secs) {
        const size_t n = static_cast<size_t>(secs * fs), start = left.size();
        left.resize(start + n);
        right.resize(start + n);
        synth.process(left.data() + start, right.data() + start, n);
    };
    const int chords[4][4] = {{48, 55, 58, 63}, {44, 51, 55, 60}, {51, 55, 58, 62}, {46, 53, 58, 62}};
    for (const auto& chord : chords) {
        for (int n : chord) synth.noteOn(n, 100);
        run(1.8f);
        for (int n : chord) synth.noteOff(n);
        run(0.2f);
    }
    for (int n : {60, 63, 67, 70, 72, 70, 67, 63}) {
        synth.noteOn(n, 110);
        run(0.2f);
        synth.noteOff(n);
        run(0.05f);
    }
    run(3.0f);

    WavInfo info;
    info.sampleRate = a.kg.sampleRate;
    info.bits = a.kg.bits;
    std::string err;
    if (!writeWav(a.out, {left, right}, info, &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    std::printf("wrote %s (%.1f s)\n", a.out.c_str(), static_cast<double>(left.size()) / fs);
    return 0;
}

int cmdParams(const Args& a) {
    if (a.markdown) {
        std::puts("| Key | Range | Default | CC | Description |");
        std::puts("|---|---|---|---|---|");
    }
    for (const auto& info : ob6::allParams()) {
        const std::string cc = info.cc >= 0 ? std::to_string(info.cc) : "-";
        if (a.markdown) {
            std::string desc = info.description;
            for (size_t pos; (pos = desc.find('|')) != std::string::npos;) desc.replace(pos, 1, ", ");
            std::printf("| `%s` | %g .. %g | %g | %s | %s |\n", info.key, info.min, info.max, info.def, cc.c_str(),
                        desc.c_str());
        } else {
            std::printf("%-20s %6g .. %-6g def %-6g cc %-3s %s\n", info.key, info.min, info.max, info.def,
                        cc.c_str(), info.description);
        }
    }
    return 0;
}

int cmdDump(const Args& a) {
    if (a.positional.size() != 1) return usage();
    ob6::Patch patch;
    if (!loadPatch(a.positional[0], patch)) return 1;
    std::fputs(ob6::serializePatch(patch).c_str(), stdout);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];
    Args a = parseArgs(argc, argv, 2);
    if (!a.ok) return usage();
    if (cmd == "keygroup") return cmdKeygroup(a);
    if (cmd == "calibrate") return cmdCalibrate(a);
    if (cmd == "demo") return cmdDemo(a);
    if (cmd == "params") return cmdParams(a);
    if (cmd == "dump") return cmdDump(a);
    return usage();
}
