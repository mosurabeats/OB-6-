#include "xpm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ob6tools {

namespace {

std::string escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default: r += c;
        }
    }
    return r;
}

class Xml {
public:
    void open(const std::string& tag, const std::string& attrs = "") {
        indent();
        out_ += "<" + tag + (attrs.empty() ? "" : " " + attrs) + ">\n";
        ++depth_;
    }
    void close(const std::string& tag) {
        --depth_;
        indent();
        out_ += "</" + tag + ">\n";
    }
    void text(const std::string& tag, const std::string& value) {
        indent();
        out_ += "<" + tag + ">" + escape(value) + "</" + tag + ">\n";
    }
    void num(const std::string& tag, double v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.6f", v);
        text(tag, buf);
    }
    void integer(const std::string& tag, long long v) { text(tag, std::to_string(v)); }
    void boolean(const std::string& tag, bool v) { text(tag, v ? "True" : "False"); }
    std::string str() const { return out_; }

private:
    void indent() { out_.append(static_cast<size_t>(depth_) * 2, ' '); }
    std::string out_ = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n";
    int depth_ = 0;
};

void writeLayer(Xml& x, int number, const XpmLayer* layer, int rootOffset) {
    x.open("Layer", "number=\"" + std::to_string(number) + "\"");
    x.boolean("Active", layer != nullptr);
    x.num("Volume", 1.0);
    x.num("Pan", 0.5);
    x.num("Pitch", 0.0);
    x.integer("TuneCoarse", 0);
    x.integer("TuneFine", 0);
    x.integer("VelStart", layer ? layer->velStart : 0);
    x.integer("VelEnd", layer ? layer->velEnd : 127);
    x.integer("SampleStart", 0);
    x.integer("SampleEnd", layer ? layer->frames : 0);
    x.boolean("Loop", layer && layer->loop);
    x.integer("LoopStart", layer && layer->loop ? layer->loopStart : 0);
    x.integer("LoopEnd", layer && layer->loop ? layer->loopEnd : 0);
    x.integer("LoopCrossfadeLength", 0);
    x.integer("LoopTune", 0);
    x.boolean("Mute", false);
    x.integer("RootNote", layer ? layer->rootNote + rootOffset : 0);
    x.boolean("KeyTrack", true);
    x.text("SampleName", layer ? layer->sampleName : "");
    x.text("SampleFile", "");
    x.integer("SliceIndex", 128);
    x.integer("Direction", 0);
    x.integer("Offset", 0);
    x.integer("SliceStart", 0);
    x.integer("SliceEnd", layer ? layer->frames : 0);
    x.integer("SliceLoopStart", layer && layer->loop ? layer->loopStart : 0);
    x.integer("SliceLoop", layer && layer->loop ? 1 : 0);
    x.integer("SliceLoopCrossFadeLength", 0);
    x.close("Layer");
}

}  // namespace

float mpcEnvelopeTime(float seconds) {
    // Log curve spanning 0 .. ~10 s. Approximate; see xpm.hpp.
    const float s = std::max(0.0f, seconds);
    return std::min(1.0f, std::log1p(s / 0.005f) / std::log1p(10.0f / 0.005f));
}

std::string buildKeygroupXpm(const XpmProgram& p) {
    Xml x;
    x.open("MPCVObject");
    x.open("Version");
    x.text("File_Version", "2.1");
    x.text("Application", "MPC-V");
    x.text("Application_Version", "2.10.0.0");
    x.text("Platform", "Linux");
    x.close("Version");

    x.open("Program", "type=\"Keygroup\"");
    x.text("ProgramName", p.name);
    x.num("Pan", 0.5);
    x.num("Volume", 1.0);
    x.boolean("Mute", false);
    x.boolean("Solo", false);
    x.num("Tune", 0.0);
    x.boolean("Mono", p.mono);
    x.integer("KeygroupNumKeygroups", static_cast<long long>(p.zones.size()));

    x.open("Instruments");
    int number = 1;
    for (const XpmZone& z : p.zones) {
        x.open("Instrument", "number=\"" + std::to_string(number++) + "\"");
        x.num("Volume", 1.0);
        x.boolean("Mute", false);
        x.num("Pan", 0.5);
        x.integer("TuneCoarse", 0);
        x.integer("TuneFine", 0);
        x.boolean("Mono", p.mono);
        x.integer("LowNote", z.lowNote);
        x.integer("HighNote", z.highNote);
        x.boolean("IgnoreBaseNote", false);
        x.integer("ZonePlay", 1);  // velocity-switched layers
        x.integer("MuteGroup", 0);
        // The OB-6 filter and envelopes are rendered into the samples, so
        // the MPC's own filter is bypassed and its envelope only handles
        // note release.
        x.integer("FilterType", 0);
        x.num("Cutoff", 1.0);
        x.num("Resonance", 0.0);
        x.num("FilterEnvAmt", 0.0);
        x.num("VolumeHold", 0.0);
        x.num("VolumeAttack", 0.0);
        x.num("VolumeDecay", 0.0);
        x.num("VolumeSustain", 1.0);
        x.num("VolumeRelease", mpcEnvelopeTime(p.releaseSeconds));
        x.num("VelocitySensitivity", 1.0);
        x.open("Layers");
        for (int i = 0; i < 4; ++i)
            writeLayer(x, i + 1, i < static_cast<int>(z.layers.size()) ? &z.layers[i] : nullptr, p.rootOffset);
        x.close("Layers");
        x.close("Instrument");
    }
    x.close("Instruments");
    x.close("Program");
    x.close("MPCVObject");
    return x.str();
}

}  // namespace ob6tools
