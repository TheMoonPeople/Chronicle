#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace audio {

constexpr int kVagBlockBytes = 16;
constexpr int kVagBlockSamples = 28;

enum VagFlag : std::uint8_t {
    kVagLoopEnd = 0x01,
    kVagLoopRepeat = 0x02,
    kVagLoopStart = 0x04,
};

// The SPU's ADPCM prediction filters, in 1/64 units.
constexpr int kVagFilter[5][2] = {
    {0,   0  },
    {60,  0  },
    {115, -52},
    {98,  -55},
    {122, -60},
};

struct VagSample {
    std::vector<std::int16_t> pcm;
    int                       loop_start = 0;
    bool                      loops = false;
};

struct VagDecoder {
    int history[2] = {0, 0};

    void DecodeBlock(const std::uint8_t *block, std::int16_t *out);
};

// Decodes from the start of a VAG body up to and including the first block with the loop-end flag,
// or to the end of the span. A sample whose end block also repeats loops back to the last block
// marked loop-start, or to its first sample when none is marked, as the SPU does.
VagSample DecodeVag(std::span<const std::uint8_t> data);

} // namespace audio
