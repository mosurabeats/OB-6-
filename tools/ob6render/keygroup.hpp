// Renders an OB-6 patch into a multisampled MPC Keygroup program folder.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ob6/params.hpp"

namespace ob6tools {

struct KeygroupOptions {
    int lowNote = 24;   // lowest sampled root (MIDI)
    int highNote = 96;  // highest sampled root
    int step = 3;       // semitones between roots
    int velocityLayers = 1;
    uint32_t sampleRate = 44100;
    int bits = 24;
    enum class Loop { Auto, On, Off } loop = Loop::Auto;
    float loopSeconds = 2.0f;       // length of the sustain loop
    float crossfadeSeconds = 0.25f; // loop crossfade
    float maxSeconds = 12.0f;       // cap for one-shot renders
    int rootOffset = 1;             // see xpm.hpp
    bool mono = false;              // write mono WAVs
};

struct RenderedNote {
    int root = 60;
    int velocity = 127;
    std::vector<float> left, right;
    bool loop = false;
    uint32_t loopStart = 0, loopEnd = 0;  // loopEnd inclusive
};

RenderedNote renderNote(const ob6::Patch& patch, int note, int velocity, const KeygroupOptions& opts);

// Makes ch[loopStart..loopEnd) loop seamlessly by crossfading its tail with
// the audio just before loopStart, then truncates the buffer at loopEnd.
void applyLoopCrossfade(std::vector<float>& ch, size_t loopStart, size_t loopEnd, size_t crossfade);

// Writes <outDir>/<program>/<program>.xpm plus its WAVs. Returns false and
// sets `error` on failure; `log` receives a human readable summary.
bool buildKeygroup(const ob6::Patch& patch, const KeygroupOptions& opts, const std::string& outDir,
                   std::string* log, std::string* error);

std::string safeFileName(const std::string& name);
std::string noteName(int midiNote);  // Akai convention: 60 = C3

}  // namespace ob6tools
