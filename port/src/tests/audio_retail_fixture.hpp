// Retail fixture derived from TheMoonPeople/Kazarov 611182d.
// Copyright (c) 2026 TheMoonPeople; see ../../third_party/kazarov-retail-LICENSE.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio_retail_format.hpp"

namespace chronicle_audio::fixture {

inline void Put(std::vector<std::uint8_t> &bytes, std::size_t at, std::uint32_t value, std::size_t width) {
    for (std::size_t i = 0; i < width; i++) {
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

inline std::vector<std::uint8_t> Chunk(std::uint32_t tag, std::size_t size) {
    std::vector<std::uint8_t> chunk(size, 0);
    Put(chunk, 0, chronicle_audio::retail::kChunkCreator, 4);
    Put(chunk, 4, tag, 4);
    Put(chunk, 8, static_cast<std::uint32_t>(size), 4);
    return chunk;
}

inline std::vector<std::uint8_t> TableChunk(std::uint32_t tag, const std::vector<std::vector<std::uint8_t>> &entries) {
    const std::size_t table_end = 0x10 + 4 * entries.size();
    std::size_t       total = table_end;
    for (const auto &entry : entries) {
        total += entry.size();
    }
    std::vector<std::uint8_t> chunk = Chunk(tag, total);
    Put(chunk, 0x0C, static_cast<std::uint32_t>(entries.size() - 1), 4);
    std::size_t at = table_end;
    for (std::size_t i = 0; i < entries.size(); i++) {
        Put(chunk, 0x10 + 4 * i, static_cast<std::uint32_t>(at), 4);
        std::copy(entries[i].begin(), entries[i].end(), chunk.begin() + static_cast<std::ptrdiff_t>(at));
        at += entries[i].size();
    }
    return chunk;
}

inline std::vector<std::uint8_t> SquareVag(int blocks) {
    std::vector<std::uint8_t> vag;
    for (int b = 0; b < blocks; b++) {
        std::uint8_t flags = 0;
        if (b == 0) {
            flags = static_cast<std::uint8_t>(flags | chronicle_audio::retail::kVagLoopStart);
        }
        if (b == blocks - 1) {
            flags = static_cast<std::uint8_t>(flags | chronicle_audio::retail::kVagLoopEnd | chronicle_audio::retail::kVagLoopRepeat);
        }
        vag.push_back(0x00);
        vag.push_back(flags);
        vag.insert(vag.end(), 14, b % 2 == 0 ? std::uint8_t{0x77} : std::uint8_t{0x88});
    }
    return vag;
}

struct BankSpec {
    int           programs = 1;
    int           key_low = 0;
    int           key_high = 127;
    int           base_note = 60;
    int           rate = 48000;
    std::uint16_t adsr1 = 0x80FF;
    std::uint16_t adsr2 = 0x5FC0;
};

inline std::vector<std::uint8_t> MakeHd(const BankSpec &spec) {
    const std::size_t programs = static_cast<std::size_t>(std::max(1, spec.programs));

    std::vector<std::vector<std::uint8_t>> prog_entries;
    for (std::size_t p = 0; p < programs; p++) {
        std::vector<std::uint8_t> entry(0x38, 0);
        Put(entry, 0x00, 0x24, 4);
        Put(entry, 0x04, 1, 1);
        Put(entry, 0x05, 0x14, 1);
        Put(entry, 0x06, 127, 1);
        Put(entry, 0x07, 64, 1);
        Put(entry, 0x24 + 0x00, 0, 2);
        Put(entry, 0x24 + 0x02, static_cast<std::uint32_t>(spec.key_low), 1);
        Put(entry, 0x24 + 0x04, static_cast<std::uint32_t>(spec.key_high), 1);
        Put(entry, 0x24 + 0x06, 2 * chronicle_audio::retail::kFinePerSemitone, 2);
        Put(entry, 0x24 + 0x08, 2 * chronicle_audio::retail::kFinePerSemitone, 2);
        Put(entry, 0x24 + 0x10, 127, 1);
        Put(entry, 0x24 + 0x11, 64, 1);
        prog_entries.push_back(entry);
    }

    const std::vector<std::vector<std::uint8_t>> sset_entries = {
        {0x00, 0x00, 127, 1, 0x00, 0x00, 0x00, 0x00},
    };

    std::vector<std::uint8_t> sample(0x2C, 0);
    Put(sample, 0x00, 0, 2);
    Put(sample, 0x02, 0, 1);
    Put(sample, 0x04, 127, 1);
    Put(sample, 0x0B, static_cast<std::uint32_t>(spec.base_note), 1);
    Put(sample, 0x0C, 0, 1);
    Put(sample, 0x0D, 64, 1);
    Put(sample, 0x10, 127, 1);
    Put(sample, 0x12, spec.adsr1, 2);
    Put(sample, 0x14, spec.adsr2, 2);
    Put(sample, 0x29, 0x0F, 1);
    const std::vector<std::vector<std::uint8_t>> smpl_entries = {sample};

    std::vector<std::uint8_t> vagi_entry(8, 0);
    Put(vagi_entry, 0, 0, 4);
    Put(vagi_entry, 4, static_cast<std::uint32_t>(spec.rate), 2);
    const std::vector<std::vector<std::uint8_t>> vagi_entries = {vagi_entry};

    const std::vector<std::uint8_t> vers = Chunk(chronicle_audio::retail::kChunkVers, 16);
    const std::vector<std::uint8_t> prog = TableChunk(chronicle_audio::retail::kChunkProg, prog_entries);
    const std::vector<std::uint8_t> sset = TableChunk(chronicle_audio::retail::kChunkSset, sset_entries);
    const std::vector<std::uint8_t> smpl = TableChunk(chronicle_audio::retail::kChunkSmpl, smpl_entries);
    const std::vector<std::uint8_t> vagi = TableChunk(chronicle_audio::retail::kChunkVagi, vagi_entries);

    std::vector<std::uint8_t> head = Chunk(chronicle_audio::retail::kChunkHead, 0x24);
    const std::size_t         head_at = vers.size();
    const std::size_t         prog_at = head_at + head.size();
    const std::size_t         sset_at = prog_at + prog.size();
    const std::size_t         smpl_at = sset_at + sset.size();
    const std::size_t         vagi_at = smpl_at + smpl.size();
    Put(head, 0x14, static_cast<std::uint32_t>(prog_at), 4);
    Put(head, 0x18, static_cast<std::uint32_t>(sset_at), 4);
    Put(head, 0x1C, static_cast<std::uint32_t>(smpl_at), 4);
    Put(head, 0x20, static_cast<std::uint32_t>(vagi_at), 4);

    const std::vector<std::uint8_t> *const parts[] = {&vers, &head, &prog, &sset, &smpl, &vagi};
    std::vector<std::uint8_t>              hd;
    for (const std::vector<std::uint8_t> *part : parts) {
        hd.insert(hd.end(), part->begin(), part->end());
    }
    return hd;
}

inline void AppendVarLen(std::vector<std::uint8_t> &out, std::uint32_t value) {
    std::uint8_t groups[5] = {};
    int          count = 0;
    do {
        groups[count++] = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
    } while (value != 0);
    for (int i = count - 1; i >= 0; i--) {
        out.push_back(static_cast<std::uint8_t>(groups[i] | (i > 0 ? 0x80 : 0)));
    }
}

inline std::vector<std::uint8_t> MakeSq(int note, int velocity, int ticks) {
    std::vector<std::uint8_t> events = {0x00, 0xC0, 0x00, 0x00, 0x90, static_cast<std::uint8_t>(note),
                                        static_cast<std::uint8_t>(velocity)};
    AppendVarLen(events, static_cast<std::uint32_t>(ticks));
    events.insert(events.end(), {0x80, static_cast<std::uint8_t>(note), 0x00, 0xFF, 0x2F, 0x00});

    constexpr std::size_t     kBlock = 0x14;
    constexpr std::size_t     kEventsAt = kBlock + 6;
    std::vector<std::uint8_t> midi = Chunk(chronicle_audio::retail::kChunkMidi, kEventsAt + events.size());
    Put(midi, 0x0C, 0, 4);
    Put(midi, 0x10, static_cast<std::uint32_t>(kBlock), 4);
    Put(midi, kBlock, 6, 4);
    Put(midi, kBlock + 4, 480, 2);
    std::copy(events.begin(), events.end(), midi.begin() + static_cast<std::ptrdiff_t>(kEventsAt));
    return midi;
}

} // namespace chronicle_audio::fixture
