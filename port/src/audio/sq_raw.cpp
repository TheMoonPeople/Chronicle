#include <nlohmann/json.hpp>

#include <algorithm>

#include "audio/hdbank.hpp"
#include "audio/sq.hpp"

namespace legacy_sq {

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

} // namespace legacy_sq

namespace legacy_sq {
using audio::FindChunk;
using audio::kChunkMidi;
using audio::kUnused;
using audio::ReadU16;
using audio::ReadU32;

std::shared_ptr<SqFile> SqFile::Create(std::span<const std::uint8_t> sq) {
    auto file = std::make_shared<SqFile>();
    file->data_ = std::vector<std::uint8_t>(sq.begin(), sq.end());
    const std::span<const std::uint8_t> data(file->data_);

    const std::uint32_t midi = FindChunk(data, kChunkMidi);
    if (midi == kUnused) {
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

} // namespace legacy_sq

namespace audio {
std::shared_ptr<SqFile> SqFile::Create(std::span<const std::uint8_t> bytes) {
    const auto raw = legacy_sq::SqFile::Create(bytes);
    if (!raw) {
        return nullptr;
    }
    nlohmann::json root = {
        {"schema_version", 1                      },
        {"kind",           "sequence"             },
        {"songs",          nlohmann::json::array()}
    };
    for (const auto &song : raw->Songs()) {
        nlohmann::json converted = {
            {"division", song.division          },
            {"events",   nlohmann::json::array()}
        };
        legacy_sq::SqReader reader(raw->Events(song));
        while (true) {
            const auto     event = reader.Next();
            nlohmann::json e = {
                {"delta", event.delta}
            };
            switch (event.kind) {
                case legacy_sq::SqEventKind::Channel:
                    e["kind"] = "channel";
                    e["status"] = event.status;
                    e["data"] = {event.data[0], event.data[1]};
                    break;
                case legacy_sq::SqEventKind::Meta:
                    e["kind"] = "meta";
                    e["meta"] = event.meta;
                    break;
                case legacy_sq::SqEventKind::SysEx:
                    e["kind"] = "sysex";
                    break;
                default:
                    e["kind"] = "end";
                    break;
            }
            e["payload"] = std::vector<std::uint8_t>(event.payload.begin(), event.payload.end());
            converted["events"].push_back(std::move(e));
            if (event.kind == legacy_sq::SqEventKind::End) {
                break;
            }
        }
        root["songs"].push_back(std::move(converted));
    }
    return CreateFromJson(root.dump());
}
} // namespace audio
