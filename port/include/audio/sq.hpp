#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace audio {

struct SqEvent;

struct SqSong {
    std::size_t begin = 0;
    std::size_t end = 0;
    int         division = 480;
};

class SqFile {
public:
    /**
     * Converts a retail SQ sequence into owned events for the shared sequence player.
     */
    static std::shared_ptr<SqFile> Create(std::span<const std::uint8_t> sq);
    /**
     * Loads and owns normalized musical events from the published JSON schema.
     */
    static std::shared_ptr<SqFile> CreateFromJson(std::string_view json);

    const std::vector<SqSong> &Songs() const { return songs_; }

    std::span<const SqEvent> Events(const SqSong &song) const;

private:
    std::vector<SqEvent>                   events_;
    std::vector<std::vector<std::uint8_t>> payloads_;
    std::vector<SqSong>                    songs_;
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

/**
 * Iterates normalized sequence events while retaining cursor positions for playback loops.
 */
class SqReader {
public:
    SqReader() = default;

    explicit SqReader(std::span<const SqEvent> events) : events_(events) {}

    // Reads the next event; a truncated stream reads as End.
    SqEvent Next();

    struct State {
        std::size_t position = 0;
    };

    State Save() const { return {position_}; }

    void Restore(const State &state) {
        position_ = state.position;
    }

private:
    std::span<const SqEvent> events_;
    std::size_t              position_ = 0;
};

} // namespace audio
