// Minimal PCM WAV writer with an optional `smpl` chunk (root note + one
// forward loop), which samplers including the MPC read for loop points.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ob6tools {

struct WavLoop {
    uint32_t start = 0;  // first sample of the loop
    uint32_t end = 0;    // last sample of the loop (inclusive, per the RIFF spec)
};

struct WavInfo {
    uint32_t sampleRate = 44100;
    int bits = 24;        // 16 or 24
    int rootNote = -1;    // MIDI note for the smpl chunk, -1 = no smpl chunk
    bool hasLoop = false;
    WavLoop loop;
};

// Channels are interleaved from `channels` (1 or 2 equal-length buffers).
bool writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, const WavInfo& info,
              std::string* error);

}  // namespace ob6tools
