#include "wav.hpp"

#include <cmath>
#include <cstdio>

#include "ob6/dsp.hpp"

namespace ob6tools {

namespace {

void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
void putTag(std::vector<uint8_t>& b, const char* tag) { b.insert(b.end(), tag, tag + 4); }

}  // namespace

bool writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, const WavInfo& info,
              std::string* error) {
    const uint16_t numCh = static_cast<uint16_t>(channels.size());
    if (numCh < 1 || numCh > 2 || (info.bits != 16 && info.bits != 24)) {
        if (error) *error = "unsupported channel count or bit depth";
        return false;
    }
    const size_t frames = channels[0].size();
    const uint16_t bytesPerSample = static_cast<uint16_t>(info.bits / 8);
    const uint32_t dataBytes = static_cast<uint32_t>(frames * numCh * bytesPerSample);

    std::vector<uint8_t> b;
    b.reserve(dataBytes + 128);
    putTag(b, "RIFF");
    put32(b, 0);  // patched below
    putTag(b, "WAVE");

    putTag(b, "fmt ");
    put32(b, 16);
    put16(b, 1);  // PCM
    put16(b, numCh);
    put32(b, info.sampleRate);
    put32(b, info.sampleRate * numCh * bytesPerSample);
    put16(b, numCh * bytesPerSample);
    put16(b, static_cast<uint32_t>(info.bits));

    if (info.rootNote >= 0) {
        putTag(b, "smpl");
        put32(b, 36 + (info.hasLoop ? 24 : 0));
        put32(b, 0);                                              // manufacturer
        put32(b, 0);                                              // product
        put32(b, static_cast<uint32_t>(1e9 / info.sampleRate));  // sample period, ns
        put32(b, static_cast<uint32_t>(info.rootNote));          // MIDI unity note
        put32(b, 0);                                              // pitch fraction
        put32(b, 0);                                              // SMPTE format
        put32(b, 0);                                              // SMPTE offset
        put32(b, info.hasLoop ? 1 : 0);                           // loop count
        put32(b, 0);                                              // sampler data
        if (info.hasLoop) {
            put32(b, 0);  // cue point id
            put32(b, 0);  // forward loop
            put32(b, info.loop.start);
            put32(b, info.loop.end);
            put32(b, 0);  // fraction
            put32(b, 0);  // play count: infinite
        }
    }

    putTag(b, "data");
    put32(b, dataBytes);
    ob6::Rng dither(0xD17E5u);
    for (size_t i = 0; i < frames; ++i) {
        for (uint16_t c = 0; c < numCh; ++c) {
            const float x = ob6::clampf(channels[c][i], -1.0f, 1.0f);
            if (info.bits == 24) {
                const int32_t v = static_cast<int32_t>(std::lround(x * 8388607.0f));
                b.push_back(v & 0xFF);
                b.push_back((v >> 8) & 0xFF);
                b.push_back((v >> 16) & 0xFF);
            } else {
                const float tpdf = (dither.uniform() - dither.uniform());
                const long v = std::lround(ob6::clampf(x * 32767.0f + tpdf, -32768.0f, 32767.0f));
                put16(b, static_cast<uint32_t>(static_cast<int16_t>(v)) & 0xFFFF);
            }
        }
    }
    if (dataBytes & 1) b.push_back(0);  // RIFF chunks are word aligned

    const uint32_t riffSize = static_cast<uint32_t>(b.size() - 8);
    for (int i = 0; i < 4; ++i) b[4 + i] = (riffSize >> (8 * i)) & 0xFF;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        if (error) *error = "cannot write " + path;
        return false;
    }
    const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    if (!ok && error) *error = "short write to " + path;
    return ok;
}

}  // namespace ob6tools
