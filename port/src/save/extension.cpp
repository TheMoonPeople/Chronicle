#include "save/extension.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>

#include "platform/files.hpp"
#include "platform/paths.hpp"
#include "save/save_slots.hpp"

namespace dc::save {

namespace fs = std::filesystem;

namespace {

constexpr std::array<char, 4> kMagic = {'C', 'H', 'R', 'N'};
constexpr std::uint32_t       kFormat = 1;
constexpr std::string_view    kFileName = "chronicle.dat";

// Far more than any save's chunks; a larger file is not one.
constexpr std::size_t kMaxFile = 64 << 20;

struct Registered {
    ExtensionWriter  writer;
    ExtensionReader  reader;
    ExtensionChecker missing;
};

std::map<std::array<char, 4>, Registered> &Frameworks() {
    static std::map<std::array<char, 4>, Registered> frameworks;
    return frameworks;
}

// Chunks of the loaded save that no registered framework reads, written back unchanged so a newer
// port's data survives a save made here.
std::vector<ExtensionChunk> g_carried;

// What ExtensionPrepare took, until ExtensionSeal knows the image it goes with.
std::vector<ExtensionChunk> g_prepared;

// The last few sealed images' chunks, by hash: the save screen builds its image before it writes it.
std::deque<std::pair<std::uint64_t, std::vector<ExtensionChunk>>> g_sealed;

std::array<char, 4> Tag(std::string_view tag) {
    std::array<char, 4> out{};
    std::ranges::copy(tag.substr(0, out.size()), out.begin());
    return out;
}

std::uint32_t Crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

template <typename T>
void Put(std::vector<std::uint8_t> &out, T value) {
    if constexpr (std::endian::native == std::endian::big) {
        value = std::byteswap(value);
    }
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    template <typename T>
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

    bool Take(std::size_t size, std::span<const std::uint8_t> &out) {
        if (bytes_.size() < size) {
            return false;
        }
        out = bytes_.first(size);
        bytes_ = bytes_.subspan(size);
        return true;
    }

    bool Done() const {
        return bytes_.empty();
    }

private:
    std::span<const std::uint8_t> bytes_;
};

fs::path ExtensionPath(int file_no) {
    return SaveSlotDirectory(file_no) / kFileName;
}

std::optional<std::vector<std::uint8_t>> ReadFile(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (bytes.size() > kMaxFile) {
        return std::nullopt;
    }
    return bytes;
}

bool WriteSets(const fs::path &path, const ExtensionSets &sets) {
    const std::vector<std::uint8_t> bytes = ExtensionEncode(sets);
    if (FilesWrite(path, bytes.data(), bytes.size(), true) != FilesResult::kWritten) {
        std::println(stderr, "save: could not write {}", PathsDisplay(path));
        return false;
    }
    return true;
}

// Keeps a file that could not be read whole for recovery, and says so.
void SetAside(const fs::path &path, std::string_view why) {
    fs::path        damaged = path;
    std::error_code error;
    damaged += ".damaged";
    fs::copy_file(path, damaged, fs::copy_options::overwrite_existing, error);
    std::println(stderr, "save: {} {}; kept as {}", PathsDisplay(path), why, PathsDisplay(damaged));
}

void ReadEvery(const std::vector<ExtensionChunk> *chunks) {
    g_carried.clear();
    for (auto &[tag, framework] : Frameworks()) {
        const ExtensionChunk *found = nullptr;
        if (chunks != nullptr) {
            const auto chunk = std::ranges::find(*chunks, tag, &ExtensionChunk::tag);
            found = chunk != chunks->end() && chunk->intact ? &*chunk : nullptr;
        }
        framework.reader(found);
    }
    if (chunks != nullptr) {
        for (const ExtensionChunk &chunk : *chunks) {
            if (chunk.intact && !Frameworks().contains(chunk.tag)) {
                g_carried.push_back(chunk);
            }
        }
    }
}

} // namespace

void ExtensionRegister(std::string_view tag, ExtensionWriter writer, ExtensionReader reader, ExtensionChecker missing) {
    Frameworks().insert_or_assign(Tag(tag), Registered{std::move(writer), std::move(reader), std::move(missing)});
}

void ExtensionPrepare(game::CSaveData &image) {
    g_prepared.clear();
    for (const Registered &framework : std::views::values(Frameworks())) {
        if (std::optional<ExtensionChunk> chunk = framework.writer(image)) {
            g_prepared.push_back(std::move(*chunk));
        }
    }
    g_prepared.insert(g_prepared.end(), g_carried.begin(), g_carried.end());
}

void ExtensionSeal(std::span<const char> image) {
    const std::uint64_t hash = ExtensionHash(image);
    std::erase_if(g_sealed, [&](const auto &sealed) { return sealed.first == hash; });
    g_sealed.emplace_back(hash, std::exchange(g_prepared, {}));
    while (g_sealed.size() > 4) {
        g_sealed.pop_front();
    }
}

// Three writes, each through a temporary file: chunks for both the save.dat on disk and the new one,
// then save.dat, then the new one's alone. An interruption anywhere leaves chronicle.dat holding the
// chunks of whichever save.dat is there.
bool ExtensionSave(int file_no, std::span<const char> image, const std::function<bool()> &write_save) {
    const std::uint64_t         hash = ExtensionHash(image);
    std::vector<ExtensionChunk> chunks;
    if (const auto sealed = std::ranges::find(g_sealed, hash, &decltype(g_sealed)::value_type::first); sealed != g_sealed.end()) {
        chunks = sealed->second;
    } else {
        std::println(stderr, "save: no chunks were taken for this save's image");
    }

    const fs::path path = ExtensionPath(file_no);
    ExtensionSets  sets;
    if (const auto bytes = ReadFile(path)) {
        const auto old_save = ReadFile(SaveSlotPath(file_no));
        const auto old_sets = ExtensionDecode(*bytes);
        if (old_save && old_sets) {
            const std::uint64_t on_disk = ExtensionHash({reinterpret_cast<const char *>(old_save->data()), old_save->size()});
            for (const auto &set : *old_sets) {
                if (set.first == on_disk && set.first != hash) {
                    sets.push_back(set);
                }
            }
        }
    }
    if (!chunks.empty()) {
        sets.emplace_back(hash, chunks);
    }
    if (!sets.empty() && !WriteSets(path, sets)) {
        return false;
    }
    if (!write_save()) {
        return false;
    }
    if (chunks.empty()) {
        std::error_code error;
        fs::remove(path, error);
        return true;
    }
    if (sets.size() > 1) {
        // The save stands either way: the file already holds this save's chunks beside the old ones.
        WriteSets(path, {{hash, chunks}});
    }
    return true;
}

void ExtensionLoad(int file_no, std::span<const char> image) {
    const fs::path path = ExtensionPath(file_no);
    const auto     bytes = ReadFile(path);
    if (!bytes) {
        ReadEvery(nullptr);
        return;
    }
    std::optional<ExtensionSets> sets = ExtensionDecode(*bytes);
    if (!sets) {
        SetAside(path, "is damaged");
        ReadEvery(nullptr);
        return;
    }
    const std::uint64_t hash = ExtensionHash(image);
    const auto          set = std::ranges::find(*sets, hash, &ExtensionSets::value_type::first);
    if (set == sets->end()) {
        std::println(stderr, "save: {} belongs to another save.dat; its contents are left out", PathsDisplay(path));
        ReadEvery(nullptr);
        return;
    }
    if (std::ranges::any_of(set->second, [](const ExtensionChunk &chunk) { return !chunk.intact; })) {
        SetAside(path, "has a damaged chunk");
    }
    ReadEvery(&set->second);
}

bool ExtensionMissing(int file_no, std::span<const char> image) {
    const auto bytes = ReadFile(ExtensionPath(file_no));
    if (!bytes) {
        return false;
    }
    const std::optional<ExtensionSets> sets = ExtensionDecode(*bytes);
    if (!sets) {
        return false;
    }
    const auto set = std::ranges::find(*sets, ExtensionHash(image), &ExtensionSets::value_type::first);
    if (set == sets->end()) {
        return false;
    }
    return std::ranges::any_of(set->second, [](const ExtensionChunk &chunk) {
        const auto framework = Frameworks().find(chunk.tag);
        return chunk.intact && framework != Frameworks().end() && framework->second.missing && framework->second.missing(chunk);
    });
}

void ExtensionNewGame() {
    ReadEvery(nullptr);
}

void ExtensionDelete(int file_no) {
    std::error_code error;
    fs::path        path = ExtensionPath(file_no);
    fs::remove(path, error);
    path += ".damaged";
    fs::remove(path, error);
}

std::vector<std::uint8_t> ExtensionEncode(const ExtensionSets &sets) {
    std::vector<std::uint8_t> out(kMagic.begin(), kMagic.end());
    Put(out, kFormat);
    Put(out, static_cast<std::uint32_t>(sets.size()));
    for (const auto &[hash, chunks] : sets) {
        Put(out, hash);
        Put(out, static_cast<std::uint32_t>(chunks.size()));
        for (const ExtensionChunk &chunk : chunks) {
            out.insert(out.end(), chunk.tag.begin(), chunk.tag.end());
            Put(out, chunk.version);
            Put(out, static_cast<std::uint32_t>(chunk.payload.size()));
            Put(out, Crc32(chunk.payload));
            out.insert(out.end(), chunk.payload.begin(), chunk.payload.end());
        }
    }
    return out;
}

std::optional<ExtensionSets> ExtensionDecode(std::span<const std::uint8_t> bytes) {
    Reader                        reader(bytes);
    std::span<const std::uint8_t> magic;
    std::uint32_t                 format = 0;
    std::uint32_t                 set_count = 0;
    if (!reader.Take(kMagic.size(), magic) || !std::ranges::equal(magic, kMagic, {}, {}, [](char c) { return static_cast<std::uint8_t>(c); }) ||
        !reader.Get(format) || format != kFormat || !reader.Get(set_count)) {
        return std::nullopt;
    }
    ExtensionSets sets;
    for (std::uint32_t set = 0; set < set_count; set++) {
        std::uint64_t hash = 0;
        std::uint32_t chunk_count = 0;
        if (!reader.Get(hash) || !reader.Get(chunk_count)) {
            return std::nullopt;
        }
        std::vector<ExtensionChunk> chunks;
        for (std::uint32_t index = 0; index < chunk_count; index++) {
            ExtensionChunk                chunk;
            std::span<const std::uint8_t> tag;
            std::span<const std::uint8_t> payload;
            std::uint32_t                 size = 0;
            std::uint32_t                 crc = 0;
            if (!reader.Take(chunk.tag.size(), tag) || !reader.Get(chunk.version) || !reader.Get(size) || !reader.Get(crc) ||
                !reader.Take(size, payload)) {
                return std::nullopt;
            }
            std::ranges::copy(tag, chunk.tag.begin());
            chunk.payload.assign(payload.begin(), payload.end());
            chunk.intact = Crc32(payload) == crc;
            chunks.push_back(std::move(chunk));
        }
        sets.emplace_back(hash, std::move(chunks));
    }
    if (!reader.Done()) {
        return std::nullopt;
    }
    return sets;
}

// FNV-1a: the hash only tells one save.dat from another, nothing chooses the bytes to collide.
std::uint64_t ExtensionHash(std::span<const char> image) {
    std::uint64_t hash = 0xCBF29CE484222325ull;
    for (const char byte : image) {
        hash = (hash ^ static_cast<std::uint8_t>(byte)) * 0x100000001B3ull;
    }
    return hash;
}

} // namespace dc::save
