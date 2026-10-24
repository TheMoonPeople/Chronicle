#include <algorithm>

#include "audio/vag.hpp"

namespace audio {

void VagDecoder::DecodeBlock(const std::uint8_t *block, std::int16_t *out) {
    int shift = block[0] & 0x0F;
    int filter = std::min(block[0] >> 4, 4);
    // The SPU treats the reserved shifts 13 to 15 like 9.
    if (shift > 12) {
        shift = 9;
    }
    const int f0 = kVagFilter[filter][0];
    const int f1 = kVagFilter[filter][1];
    for (int i = 0; i < kVagBlockSamples; i++) {
        const int byte = block[2 + i / 2];
        const int nibble = (i & 1) ? (byte >> 4) : (byte & 0x0F);
        int       sample = static_cast<std::int16_t>(nibble << 12) >> shift;
        sample += (history[0] * f0 + history[1] * f1 + 32) >> 6;
        sample = std::clamp(sample, -32768, 32767);
        history[1] = history[0];
        history[0] = sample;
        out[i] = static_cast<std::int16_t>(sample);
    }
}

VagSample DecodeVag(std::span<const std::uint8_t> data) {
    VagSample         result;
    VagDecoder        decoder;
    const std::size_t blocks = data.size() / kVagBlockBytes;
    result.pcm.reserve(blocks * kVagBlockSamples);
    for (std::size_t b = 0; b < blocks; b++) {
        const std::uint8_t *block = data.data() + b * kVagBlockBytes;
        const std::uint8_t  flags = block[1];
        const std::size_t   start = result.pcm.size();
        if (flags & kVagLoopStart) {
            result.loop_start = static_cast<int>(start);
        }
        result.pcm.resize(start + kVagBlockSamples);
        decoder.DecodeBlock(block, result.pcm.data() + start);
        if (flags & kVagLoopEnd) {
            result.loops = (flags & kVagLoopRepeat) != 0;
            break;
        }
    }
    if (result.loop_start >= static_cast<int>(result.pcm.size())) {
        result.loops = false;
    }
    return result;
}

} // namespace audio
