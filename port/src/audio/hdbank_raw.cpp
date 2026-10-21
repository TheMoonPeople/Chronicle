#include <algorithm>
#include <bit>

#include "audio/hdbank.hpp"

namespace audio {

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
    if (chunk.empty()) {
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

std::shared_ptr<Bank> Bank::Create(std::span<const std::uint8_t> hd, std::span<const std::uint8_t> bd) {
    auto header = HdBank::Parse(hd);
    if (!header) {
        return nullptr;
    }
    auto bank = std::make_shared<Bank>();
    bank->header_ = std::move(*header);
    bank->decoded_.resize(bank->header_.vags.size());
    for (std::size_t i = 0; i < bank->header_.vags.size(); i++) {
        const HdVagInfo &info = bank->header_.vags[i];
        if (info.offset != kUnused && info.offset < bd.size()) {
            bank->decoded_[i] = DecodeVag(bd.subspan(info.offset));
        }
    }
    return bank;
}

} // namespace audio
