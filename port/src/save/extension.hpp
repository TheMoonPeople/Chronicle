#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dc::game {
class CSaveData;
}

namespace dc::save {

// What a save holds beyond retail's image lives in chronicle.dat beside its save.dat (docs/PC.md,
// "Saves"): chunks, one per framework, kept under a hash of the exact save.dat bytes they belong to,
// so a save.dat copied, restored or replaced without its chronicle.dat never takes another save's.

/**
 * One framework's part of a save.
 */
struct ExtensionChunk {
    std::array<char, 4>       tag{};
    std::uint32_t             version = 0;
    std::vector<std::uint8_t> payload;
    bool                      intact = true; /**< False when its CRC failed on reading. */
};

/**
 * Takes what a framework keeps out of the save's image about to be written, leaving there what retail
 * would hold, and returns it as a chunk; nothing when the save has none of it.
 */
using ExtensionWriter = std::function<std::optional<ExtensionChunk>(game::CSaveData &image)>;

/**
 * Puts a loaded save's chunk back into the game; null for a save without one, which starts the
 * framework afresh.
 */
using ExtensionReader = std::function<void(const ExtensionChunk *chunk)>;

/**
 * Whether a saved chunk names something no loaded pack provides, which the save's board marks.
 */
using ExtensionChecker = std::function<bool(const ExtensionChunk &chunk)>;

/**
 * Registers a framework's chunk under a four-letter tag, replacing any registered with it.
 */
void ExtensionRegister(std::string_view tag, ExtensionWriter writer, ExtensionReader reader, ExtensionChecker missing = {});

/**
 * Runs every writer over an image about to have its checksums made.
 */
void ExtensionPrepare(game::CSaveData &image);

/**
 * Keeps the chunks ExtensionPrepare took for the finished image they go with.
 */
void ExtensionSeal(std::span<const char> image);

/**
 * Writes file_no's sealed image through write_save with its chunks beside it, so that whatever is
 * interrupted, chronicle.dat holds the chunks of the save.dat on disk. False when either write fails.
 */
bool ExtensionSave(int file_no, std::span<const char> image, const std::function<bool()> &write_save);

/**
 * Gives every reader its chunk of file_no's chronicle.dat for the image just loaded.
 */
void ExtensionLoad(int file_no, std::span<const char> image);

/**
 * Whether file_no's chronicle.dat, for the image just read, names something no loaded pack provides.
 * Reads the file and nothing else: a damaged file is left for loading to report.
 */
bool ExtensionMissing(int file_no, std::span<const char> image);

/**
 * Gives every reader no chunk, for a new game.
 */
void ExtensionNewGame();

/**
 * Removes file_no's chronicle.dat.
 */
void ExtensionDelete(int file_no);

/**
 * The bytes of a chronicle.dat holding chunks for each image hash, and the reverse; nullopt for bytes
 * that are not one. For tests and the save code.
 */
using ExtensionSets = std::vector<std::pair<std::uint64_t, std::vector<ExtensionChunk>>>;
std::vector<std::uint8_t>    ExtensionEncode(const ExtensionSets &sets);
std::optional<ExtensionSets> ExtensionDecode(std::span<const std::uint8_t> bytes);

/**
 * The hash an image's chunks are kept under.
 */
std::uint64_t ExtensionHash(std::span<const char> image);

/**
 * Builds a chunk's payload: numbers little-endian, text as a u16 length and its bytes.
 */
class ChunkWriter {
public:
    template <class T>
    void Put(T value) {
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
        bytes_.insert(bytes_.end(), bytes, bytes + sizeof(T));
    }

    void PutText(std::string_view text) {
        Put(static_cast<std::uint16_t>(text.size()));
        bytes_.insert(bytes_.end(), text.begin(), text.end());
    }

    std::vector<std::uint8_t> Take() { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

/**
 * Reads what ChunkWriter wrote; each Get is false once the payload runs short.
 */
class ChunkReader {
public:
    explicit ChunkReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    template <class T>
    bool Get(T &value) {
        if (bytes_.size() < sizeof(T)) {
            return false;
        }
        std::memcpy(&value, bytes_.data(), sizeof(T));
        if constexpr (std::endian::native == std::endian::big) {
            value = std::byteswap(value);
        }
        bytes_ = bytes_.subspan(sizeof(T));
        return true;
    }

    bool GetText(std::string &text) {
        std::uint16_t size = 0;
        if (!Get(size) || bytes_.size() < size) {
            return false;
        }
        text.assign(reinterpret_cast<const char *>(bytes_.data()), size);
        bytes_ = bytes_.subspan(size);
        return true;
    }

    [[nodiscard]] bool Done() const { return bytes_.empty(); }

private:
    std::span<const std::uint8_t> bytes_;
};

} // namespace dc::save
