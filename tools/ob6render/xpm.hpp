// Writer for Akai MPC Keygroup programs (.xpm, the MPCVObject XML format).
//
// We emit the 2.x-era layout that MPC firmware 3.x still loads; the MPC
// fills any element we omit with its defaults. Two details are not
// documented by Akai and must be confirmed on hardware with the
// `calibrate` command (see docs/MPC_KEY37.md):
//   * RootNote is written as MIDI note + rootOffset (default 1);
//   * envelope times are normalised 0..1 on a curve we approximate.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ob6tools {

struct XpmLayer {
    int velStart = 0, velEnd = 127;
    std::string sampleName;  // WAV file name without extension, same folder
    int rootNote = 60;       // MIDI note of the recorded pitch
    uint32_t frames = 0;
    bool loop = false;
    uint32_t loopStart = 0, loopEnd = 0;
};

struct XpmZone {
    int lowNote = 0, highNote = 127;
    std::vector<XpmLayer> layers;  // up to 4 velocity layers
};

struct XpmProgram {
    std::string name;
    std::vector<XpmZone> zones;
    int rootOffset = 1;
    float releaseSeconds = 0.3f;  // MPC amp-envelope release
    bool mono = false;
};

std::string buildKeygroupXpm(const XpmProgram& program);

// Seconds -> MPC normalised envelope time (approximation, see header note).
float mpcEnvelopeTime(float seconds);

}  // namespace ob6tools
