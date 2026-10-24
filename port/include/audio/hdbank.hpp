#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "audio/vag.hpp"
#include "platform/vfs.hpp"

namespace audio {

constexpr std::uint32_t kChunkCreator = 0x53434549; // 'SCEI'
constexpr std::uint32_t kChunkVers = 0x56657273;
constexpr std::uint32_t kChunkHead = 0x48656164;
constexpr std::uint32_t kChunkProg = 0x50726F67;
constexpr std::uint32_t kChunkSset = 0x53736574;
constexpr std::uint32_t kChunkSmpl = 0x536D706C;
constexpr std::uint32_t kChunkVagi = 0x56616769;
constexpr std::uint32_t kChunkSequ = 0x53657175;
constexpr std::uint32_t kChunkMidi = 0x4D696469;
constexpr std::uint32_t kUnused = 0xFFFFFFFF;

// Byte offsets of the fields the player reads, relative to each record's start.
namespace hd {
constexpr std::size_t kChunkHeaderSize = 12;
constexpr std::size_t kHeadProgramAddr = 0x14;
constexpr std::size_t kHeadSampleSet = 0x18;
constexpr std::size_t kHeadSample = 0x1C;
constexpr std::size_t kHeadVagInfo = 0x20;
constexpr std::size_t kTableCount = 0x0C;
constexpr std::size_t kTableOffsets = 0x10;
constexpr std::size_t kProgramParamSize = 0x24;
constexpr std::size_t kSplitSize = 0x14;
constexpr std::size_t kSampleParamSize = 0x2A;
constexpr std::size_t kSampleSpuAttr = 0x29;
constexpr std::size_t kVagInfoSize = 8;
} // namespace hd

// Pitches below a semitone (detunes, bend ranges) are in 128ths of a semitone, as the synth adds
// them: a full bend moves the pitch by exactly the split's range.
constexpr int kFinePerSemitone = 128;

struct HdSplit {
    int          sample_set = 0;
    std::uint8_t key_low = 0;
    std::uint8_t key_high = 127;
    int          bend_low = 2 * kFinePerSemitone;
    int          bend_high = 2 * kFinePerSemitone;
    std::uint8_t volume = 127;
    int          pan = 64;
    int          transpose = 0;
    int          detune = 0;
};

struct HdProgram {
    bool                 present = false;
    std::uint8_t         volume = 127;
    int                  pan = 64;
    int                  transpose = 0;
    int                  detune = 0;
    std::vector<HdSplit> splits;
};

// How a sample set maps key-on velocity before it scales the volume, as the synth's curves 0 to 5.
enum class VelocityCurve : std::uint8_t {
    Linear,
    Inverse,
    Square,
    InverseSquare,
    SquareFromTop,
    SquareOfInverse,
};

int ApplyVelocityCurve(int curve, int velocity);

struct HdSampleSet {
    std::uint8_t     vel_curve = 0;
    std::uint8_t     vel_low = 0;
    std::uint8_t     vel_high = 127;
    std::vector<int> samples;
};

struct HdSample {
    int           vag = 0;
    std::uint8_t  vel_low = 0;
    std::uint8_t  vel_high = 127;
    std::uint8_t  base_note = 60;
    int           detune = 0;
    int           pan = 64;
    std::uint8_t  volume = 127;
    std::uint16_t adsr1 = 0x80FF;
    std::uint16_t adsr2 = 0x5FC0;
    // Bits 0-3: VMIXL, VMIXR, VMIXEL, VMIXER; bits 4-5: core 0 (0x10), core 1 (0x20) or either.
    std::uint8_t spu_attr = 0x0F;
};

struct HdVagInfo {
    std::uint32_t offset = kUnused;
    int           rate = 0;
    std::uint8_t  attr = 0;
};

struct HdBank {
    /**
     * Reads retail HD instrument tables while tolerating truncated optional chunks.
     */
    static std::optional<HdBank> Parse(std::span<const std::uint8_t> hd);
    std::vector<HdProgram>       programs;
    std::vector<HdSampleSet>     sample_sets;
    std::vector<HdSample>        samples;
    std::vector<HdVagInfo>       vags;
};

// One playable layer of a key-on: a sample, the split it came through and its decoded VAG.
struct Layer {
    const HdSplit   *split;
    const HdSample  *sample;
    const VagSample *vag;
    int              rate;
    int              vel_curve;
};

/**
 * Owns semantic instrument tables and PCM samples ready for allocation-free synthesis.
 */
class Bank {
public:
    /**
     * Owns retail HD instruments and decodes their BD samples before playback starts.
     */
    static std::shared_ptr<Bank> Create(std::span<const std::uint8_t> hd, std::span<const std::uint8_t> bd);
    /**
     * Loads semantic bank tables and standard PCM WAV samples through the asset resolver.
     */
    static std::shared_ptr<Bank> CreateFromJson(std::string_view                                                           json,
                                                const std::function<std::filesystem::path(const std::filesystem::path &)> &resolve);
    /**
     * Loads each sample from the first valid WAV in precedence order.
     */
    static std::shared_ptr<Bank> CreateFromJsonCandidates(std::string_view                                                           json,
                                                          const std::function<std::vector<vfs::Ref>(const std::filesystem::path &)> &resolve);

    const HdBank &Header() const { return header_; }

    const HdProgram *Program(int program) const;

    // Every layer a key-on of note at velocity sounds through program. Returns how many were written.
    int Resolve(int program, int note, int velocity, std::span<Layer> out) const;

    // The split whose number in program's split table is index, for effects addressed by tone.
    int ResolveSplit(int program, int split, int velocity, std::span<Layer> out) const;

private:
    int AddSampleSet(const HdSplit &split, int velocity, std::span<Layer> out, int count) const;

    HdBank                 header_;
    std::vector<VagSample> decoded_;
};

std::uint32_t ReadU32(std::span<const std::uint8_t> data, std::size_t offset);
std::uint16_t ReadU16(std::span<const std::uint8_t> data, std::size_t offset);

// The offset of the first chunk of type at a 4-byte boundary, or kUnused.
std::uint32_t FindChunk(std::span<const std::uint8_t> data, std::uint32_t type, std::size_t from = 0);

} // namespace audio
