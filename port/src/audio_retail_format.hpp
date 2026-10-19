#pragma once
// Retail format readers derived from TheMoonPeople/Kazarov at 611182d.
// Copyright (c) 2026 TheMoonPeople; see ../third_party/kazarov-retail-LICENSE.
// Chronicle owns conversion of its existing data; synthesis stays in official Kazarov.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace chronicle_audio::retail {

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

} // namespace chronicle_audio::retail

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace chronicle_audio::retail {

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
    std::uint32_t offset = 0;
    int           rate = 0;
    std::uint8_t  attr = 0;
};

struct HdBank {
    std::vector<HdProgram>   programs;
    std::vector<HdSampleSet> sample_sets;
    std::vector<HdSample>    samples;
    std::vector<HdVagInfo>   vags;

    // Missing or truncated chunks leave their tables empty or short rather than failing: a bank
    // that plays partly is more useful than none.
    static std::optional<HdBank> Parse(std::span<const std::uint8_t> hd);
};

std::uint32_t ReadU32(std::span<const std::uint8_t>, std::size_t);
std::uint16_t ReadU16(std::span<const std::uint8_t>, std::size_t);
std::uint32_t FindChunk(std::span<const std::uint8_t>, std::uint32_t, std::size_t from = 0);
} // namespace chronicle_audio::retail

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace chronicle_audio::retail {

namespace sq {
constexpr std::size_t kMidiCount = 0x0C;
constexpr std::size_t kMidiOffsets = 0x10;
constexpr std::size_t kBlockDataOffset = 0;
constexpr std::size_t kBlockDivision = 4;
constexpr std::size_t kBlockMinSize = 6;
} // namespace sq

struct SqSong {
    std::size_t begin = 0;
    std::size_t end = 0;
    int         division = 480;
};

class SqFile {
public:
    // Copies the file: the game reuses its load buffer as soon as the call returns.
    static std::shared_ptr<SqFile> Create(std::span<const std::uint8_t> sq);

    const std::vector<SqSong> &Songs() const { return songs_; }

    std::span<const std::uint8_t> Events(const SqSong &song) const {
        return std::span<const std::uint8_t>(data_).subspan(song.begin, song.end - song.begin);
    }

private:
    std::vector<std::uint8_t> data_;
    std::vector<SqSong>       songs_;
};

enum class SqEventKind {
    Channel,
    Meta,
    SysEx,
    End,
};

struct SqEvent {
    std::uint32_t                 delta = 0;
    SqEventKind                   kind = SqEventKind::End;
    std::uint8_t                  status = 0;
    std::uint8_t                  data[2] = {0, 0};
    std::uint8_t                  meta = 0;
    std::span<const std::uint8_t> payload;
};

// Sony's SQ track data: SMF-style variable-length deltas, running status, meta and SysEx events,
// with two differences every sequence on the disc relies on. A note-off carries only its note
// number. Bit 7 of a channel message's last data byte means the next event follows at once and
// its delta is left out.
class SqReader {
public:
    SqReader() = default;

    explicit SqReader(std::span<const std::uint8_t> events) : events_(events) {}

    // Reads the next event; a truncated stream reads as End.
    SqEvent Next();

    struct State {
        std::size_t  position = 0;
        std::uint8_t running = 0;
        bool         no_delta = false;
    };

    State Save() const { return {position_, running_, no_delta_}; }

    void Restore(const State &state) {
        position_ = state.position;
        running_ = state.running;
        no_delta_ = state.no_delta;
    }

    static int DataBytes(std::uint8_t status);

private:
    bool ReadVarLen(std::uint32_t &value);

    std::span<const std::uint8_t> events_;
    std::size_t                   position_ = 0;
    std::uint8_t                  running_ = 0;
    bool                          no_delta_ = false;
};

} // namespace chronicle_audio::retail
