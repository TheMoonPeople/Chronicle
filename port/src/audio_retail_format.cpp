// Retail format readers derived from TheMoonPeople/Kazarov at 611182d.
// Copyright (c) 2026 TheMoonPeople; see ../third_party/kazarov-retail-LICENSE.
// Chronicle owns conversion of its existing data; synthesis stays in official Kazarov.
#include "audio_retail_format.hpp"

#include <algorithm>

namespace chronicle_audio::retail {

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

} // namespace chronicle_audio::retail

#include <algorithm>
#include <bit>

#include "audio_retail_format.hpp"

namespace chronicle_audio::retail {

std::uint32_t ReadU32(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4) {
        return kUnused;
    }
    const std::uint8_t *p = data.data() + offset;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t ReadU16(std::span<const std::uint8_t> data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2) {
        return 0;
    }
    return data[offset] | (data[offset + 1] << 8);
}

namespace {

std::uint8_t ReadU8(std::span<const std::uint8_t> data, std::size_t offset) {
    return offset < data.size() ? data[offset] : 0;
}

int ReadS8(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::int8_t>(ReadU8(data, offset));
}

bool IsTag(std::uint32_t value, std::uint32_t tag) {
    return value == tag || value == std::byteswap(tag);
}

bool IsChunk(std::span<const std::uint8_t> data, std::size_t offset, std::uint32_t type) {
    return IsTag(ReadU32(data, offset), kChunkCreator) && IsTag(ReadU32(data, offset + 4), type);
}

// The chunk the Head chunk points to, or failing that the first one found by scanning: the Head
// addresses are the documented route, the scan covers banks whose Head disagrees with the layout.
std::span<const std::uint8_t> LocateChunk(std::span<const std::uint8_t> hd, std::uint32_t head, std::size_t field,
                                          std::uint32_t type) {
    std::uint32_t offset = kUnused;
    if (head != kUnused) {
        const std::uint32_t pointed = ReadU32(hd, head + field);
        if (pointed != kUnused && IsChunk(hd, pointed, type)) {
            offset = pointed;
        }
    }
    if (offset == kUnused) {
        offset = FindChunk(hd, type);
    }
    if (offset == kUnused) {
        return {};
    }
    std::span<const std::uint8_t> chunk = hd.subspan(offset);
    const std::uint32_t           size = ReadU32(chunk, 8);
    if (size >= hd::kTableOffsets && size <= chunk.size()) {
        chunk = chunk.first(size);
    }
    return chunk;
}

// Walks a chunk's count-and-offsets table, calling parse with the span each used entry starts at.
template <class F>
void ForEachEntry(std::span<const std::uint8_t> chunk, F parse) {
    if (chunk.size() < hd::kTableOffsets) {
        return;
    }
    const std::uint32_t max_index = ReadU32(chunk, hd::kTableCount);
    if (max_index == kUnused) {
        return;
    }
    const std::size_t count = std::min<std::size_t>(max_index + 1, (chunk.size() - hd::kTableOffsets) / 4);
    for (std::size_t i = 0; i < count; i++) {
        const std::uint32_t offset = ReadU32(chunk, hd::kTableOffsets + i * 4);
        if (offset == kUnused || offset >= chunk.size()) {
            parse(i, std::span<const std::uint8_t>{});
            continue;
        }
        parse(i, chunk.subspan(offset));
    }
}

} // namespace

std::uint32_t FindChunk(std::span<const std::uint8_t> data, std::uint32_t type, std::size_t from) {
    for (std::size_t offset = from & ~std::size_t{3}; offset + 8 <= data.size(); offset += 4) {
        if (IsChunk(data, offset, type)) {
            return static_cast<std::uint32_t>(offset);
        }
    }
    return kUnused;
}

std::optional<HdBank> HdBank::Parse(std::span<const std::uint8_t> hd) {
    const std::uint32_t head = FindChunk(hd, kChunkHead);
    const auto          prog = LocateChunk(hd, head, hd::kHeadProgramAddr, kChunkProg);
    const auto          sset = LocateChunk(hd, head, hd::kHeadSampleSet, kChunkSset);
    const auto          smpl = LocateChunk(hd, head, hd::kHeadSample, kChunkSmpl);
    const auto          vagi = LocateChunk(hd, head, hd::kHeadVagInfo, kChunkVagi);
    if (prog.empty() || smpl.empty() || vagi.empty()) {
        return std::nullopt;
    }

    HdBank bank;
    ForEachEntry(prog, [&](std::size_t, std::span<const std::uint8_t> p) {
        HdProgram &program = bank.programs.emplace_back();
        if (p.size() < hd::kProgramParamSize) {
            return;
        }
        program.present = true;
        const std::uint32_t base = ReadU32(p, 0);
        const int           n = ReadU8(p, 4);
        std::size_t         size = ReadU8(p, 5);
        if (size < hd::kSplitSize) {
            size = hd::kSplitSize;
        }
        program.volume = ReadU8(p, 6);
        program.pan = ReadU8(p, 7);
        program.transpose = ReadS8(p, 8);
        program.detune = ReadS8(p, 9);
        for (int s = 0; s < n; s++) {
            const std::size_t at = base + s * size;
            if (base == kUnused || at + hd::kSplitSize > p.size()) {
                break;
            }
            HdSplit &split = program.splits.emplace_back();
            split.sample_set = ReadU16(p, at);
            split.key_low = ReadU8(p, at + 2);
            split.key_high = ReadU8(p, at + 4);
            split.bend_low = ReadU16(p, at + 6);
            split.bend_high = ReadU16(p, at + 8);
            split.volume = ReadU8(p, at + 0x10);
            split.pan = ReadU8(p, at + 0x11);
            split.transpose = ReadS8(p, at + 0x12);
            split.detune = ReadS8(p, at + 0x13);
        }
    });
    ForEachEntry(sset, [&](std::size_t, std::span<const std::uint8_t> p) {
        HdSampleSet &set = bank.sample_sets.emplace_back();
        if (p.size() < 4) {
            return;
        }
        set.vel_curve = ReadU8(p, 0);
        set.vel_low = ReadU8(p, 1);
        set.vel_high = ReadU8(p, 2);
        const int n = ReadU8(p, 3);
        for (int i = 0; i < n && 4 + i * 2 + 2 <= static_cast<int>(p.size()); i++) {
            set.samples.push_back(ReadU16(p, 4 + i * 2));
        }
    });
    ForEachEntry(smpl, [&](std::size_t, std::span<const std::uint8_t> p) {
        HdSample &sample = bank.samples.emplace_back();
        if (p.size() < hd::kSampleParamSize) {
            sample.vag = -1;
            return;
        }
        sample.vag = ReadU16(p, 0);
        sample.vel_low = ReadU8(p, 2);
        sample.vel_high = ReadU8(p, 4);
        sample.base_note = ReadU8(p, 0x0B);
        sample.detune = ReadS8(p, 0x0C);
        sample.pan = ReadU8(p, 0x0D);
        sample.volume = ReadU8(p, 0x10);
        sample.adsr1 = ReadU16(p, 0x12);
        sample.adsr2 = ReadU16(p, 0x14);
        sample.spu_attr = ReadU8(p, hd::kSampleSpuAttr);
    });
    ForEachEntry(vagi, [&](std::size_t, std::span<const std::uint8_t> p) {
        HdVagInfo &info = bank.vags.emplace_back();
        if (p.size() < hd::kVagInfoSize) {
            info.offset = kUnused;
            return;
        }
        info.offset = ReadU32(p, 0);
        info.rate = ReadU16(p, 4);
        info.attr = ReadU8(p, 6);
    });
    return bank;
}

} // namespace chronicle_audio::retail

#include <algorithm>

#include "audio_retail_format.hpp"

namespace chronicle_audio::retail {

std::shared_ptr<SqFile> SqFile::Create(std::span<const std::uint8_t> sq) {
    auto file = std::make_shared<SqFile>();
    file->data_ = std::vector<std::uint8_t>(sq.begin(), sq.end());
    const std::span<const std::uint8_t> data(file->data_);

    const std::uint32_t midi = FindChunk(data, kChunkMidi);
    if (midi == kUnused || data.size() - midi < sq::kMidiOffsets) {
        return nullptr;
    }
    std::size_t         chunk_end = data.size();
    const std::uint32_t size = ReadU32(data, midi + 8);
    if (size != kUnused && size >= sq::kMidiOffsets && size <= data.size() - midi) {
        chunk_end = midi + size;
    }
    const std::uint32_t max_index = ReadU32(data, midi + sq::kMidiCount);
    if (max_index == kUnused) {
        return nullptr;
    }
    const std::size_t        count = std::min<std::size_t>(max_index + 1, (chunk_end - midi - sq::kMidiOffsets) / 4);
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < count; i++) {
        const std::uint32_t offset = ReadU32(data, midi + sq::kMidiOffsets + i * 4);
        if (offset != kUnused && midi + std::size_t{offset} + sq::kBlockMinSize <= chunk_end) {
            starts.push_back(midi + offset);
        }
    }
    for (std::size_t i = 0; i < count; i++) {
        SqSong             &song = file->songs_.emplace_back();
        const std::uint32_t offset = ReadU32(data, midi + sq::kMidiOffsets + i * 4);
        if (offset == kUnused || midi + std::size_t{offset} + sq::kBlockMinSize > chunk_end) {
            continue;
        }
        const std::size_t block = midi + offset;
        // A song's events run to the next song's block, or to the end of the chunk.
        std::size_t end = chunk_end;
        for (std::size_t start : starts) {
            if (start > block) {
                end = std::min(end, start);
            }
        }
        std::size_t data_offset = ReadU32(data, block + sq::kBlockDataOffset);
        if (data_offset < sq::kBlockMinSize || block + data_offset > end) {
            data_offset = sq::kBlockMinSize;
        }
        const int division = ReadU16(data, block + sq::kBlockDivision);
        song.begin = block + data_offset;
        song.end = end;
        song.division = division > 0 && division < 0x8000 ? division : 480;
    }
    return file;
}

bool SqReader::ReadVarLen(std::uint32_t &value) {
    value = 0;
    for (int i = 0; i < 4; i++) {
        if (position_ >= events_.size()) {
            return false;
        }
        const std::uint8_t byte = events_[position_++];
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return true;
        }
    }
    return true;
}

int SqReader::DataBytes(std::uint8_t status) {
    switch (status & 0xF0) {
        case 0x80:
        case 0xC0:
        case 0xD0:
            return 1;
        default:
            return 2;
    }
}

SqEvent SqReader::Next() {
    SqEvent event;
    while (true) {
        if (no_delta_) {
            event.delta = 0;
            no_delta_ = false;
        } else if (!ReadVarLen(event.delta)) {
            return SqEvent{};
        }
        if (position_ >= events_.size()) {
            return SqEvent{};
        }
        std::uint8_t status = events_[position_];
        if (status & 0x80) {
            position_++;
        } else if (running_ != 0) {
            status = running_;
        } else {
            // A data byte with no status to run on: skip it and its delta.
            position_++;
            continue;
        }

        if (status == 0xFF) {
            if (position_ >= events_.size()) {
                return SqEvent{};
            }
            event.kind = SqEventKind::Meta;
            event.meta = events_[position_++];
            std::uint32_t length;
            if (!ReadVarLen(length) || length > events_.size() - position_) {
                return SqEvent{};
            }
            event.payload = events_.subspan(position_, length);
            position_ += length;
            if (event.meta == 0x2F) {
                event.kind = SqEventKind::End;
            }
            return event;
        }
        if (status == 0xF0 || status == 0xF7) {
            std::uint32_t length;
            if (!ReadVarLen(length) || length > events_.size() - position_) {
                return SqEvent{};
            }
            event.kind = SqEventKind::SysEx;
            event.payload = events_.subspan(position_, length);
            position_ += length;
            return event;
        }
        if (status >= 0xF0) {
            const int length = status == 0xF2 ? 2 : (status == 0xF1 || status == 0xF3) ? 1
                                                                                       : 0;
            position_ = std::min(position_ + length, events_.size());
            event.kind = SqEventKind::SysEx;
            return event;
        }

        running_ = status;
        const int length = DataBytes(status);
        if (position_ + length > events_.size()) {
            return SqEvent{};
        }
        event.kind = SqEventKind::Channel;
        event.status = status;
        for (int i = 0; i < length; i++) {
            event.data[i] = events_[position_++] & 0x7F;
        }
        no_delta_ = (events_[position_ - 1] & 0x80) != 0;
        return event;
    }
}

} // namespace chronicle_audio::retail
