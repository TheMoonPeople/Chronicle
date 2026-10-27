#include "dataread.hpp"
#include "language.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../tools/dcdata/dcdata.hpp"
#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "localize/localize.hpp"
#include "platform/data_archive.hpp"
#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

struct IndexedFile {
    fs::path       path;
    std::string    logical_path;
    std::string    source_path;
    std::size_t    archive_index = 0;
    std::uintmax_t size = 0;
    bool           archived = false;
};

std::unordered_map<std::string, IndexedFile> data_index;
std::unordered_map<std::string, IndexedFile> data_base_index;
std::unique_ptr<DataArchive>                 data_archive;

struct PackCapacity {
    std::size_t                capacity = 0;
    const void                *owner = nullptr;
    std::string                logical_path;
    std::string                member;
    std::vector<std::uint64_t> member_offsets;
    std::vector<std::uint64_t> member_source_offsets;
    std::vector<std::uint64_t> member_sizes;
    std::size_t                source_offset = 0;
    std::size_t                source_size = 0;
    bool                       copied = false;
    const void                *arena_base = nullptr;
    std::size_t                arena_end = 0;
};

std::unordered_map<const void *, PackCapacity>                     loaded_capacities;
std::unordered_map<const void *, std::unordered_set<const void *>> copied_capacities_by_owner;
constinit std::atomic<std::size_t>                                 copied_capacity_count = 0;
bool                                                               data_indexed;
std::array<BG_READ_INFO, 32>                                       bg_read_info;
std::mutex                                                         texture_read_stats_mutex;
DataTextureReadStats                                               texture_read_stats;
std::atomic<bool>                                                  texture_read_stats_enabled = false;

void ForgetCopiedCapacity(const void *key, const PackCapacity &range) {
    if (!range.copied) {
        return;
    }
    auto owner_at = copied_capacities_by_owner.find(range.owner);
    if (owner_at != copied_capacities_by_owner.end()) {
        owner_at->second.erase(key);
        if (owner_at->second.empty()) {
            copied_capacities_by_owner.erase(owner_at);
        }
    }
    copied_capacity_count.fetch_sub(1, std::memory_order_relaxed);
}

void TrackTextureRead(const IndexedFile &file) {
    if (!texture_read_stats_enabled.load(std::memory_order_relaxed)) {
        return;
    }
    const std::string path = dcdata::FoldPath(file.logical_path);
    if (!path.starts_with("textures/")) {
        return;
    }
    const bool is_json = path.ends_with(".json");
    const bool is_png = path.ends_with(".png");
    if (!is_json && !is_png) {
        return;
    }
    std::scoped_lock lock(texture_read_stats_mutex);
    if (file.archived) {
        ++(is_json ? texture_read_stats.archived_json : texture_read_stats.archived_png);
    } else {
        ++(is_json ? texture_read_stats.loose_json : texture_read_stats.loose_png);
    }
    std::vector<std::string> &sources = file.archived ? texture_read_stats.archived_sources
                                                      : texture_read_stats.loose_sources;
    if (sources.size() < 8 &&
        std::ranges::find(sources, file.source_path) == sources.end()) {
        sources.push_back(file.source_path);
    }
}

void ForgetPackCapacities(const void *owner) {
    for (auto at = loaded_capacities.begin(); at != loaded_capacities.end();) {
        if (at->second.owner == owner) {
            ForgetCopiedCapacity(at->first, at->second);
            at = loaded_capacities.erase(at);
        } else {
            ++at;
        }
    }
}

void TrackPackCapacity(const void *pack, std::size_t capacity, const void *owner,
                       std::string_view logical_path = {}, std::string_view member = {},
                       std::size_t source_offset = 0, std::size_t source_size = 0,
                       std::vector<std::uint64_t> member_offsets = {},
                       std::vector<std::uint64_t> member_source_offsets = {},
                       std::vector<std::uint64_t> member_sizes = {}) {
    if (pack != nullptr) {
        auto previous = loaded_capacities.find(pack);
        if (previous != loaded_capacities.end()) {
            ForgetCopiedCapacity(pack, previous->second);
        }
        loaded_capacities[pack] = {.capacity = capacity,
                                   .owner = owner,
                                   .logical_path = std::string(logical_path),
                                   .member = std::string(member),
                                   .member_offsets = std::move(member_offsets),
                                   .member_source_offsets = std::move(member_source_offsets),
                                   .member_sizes = std::move(member_sizes),
                                   .source_offset = source_offset,
                                   .source_size = source_size,
                                   .copied = false,
                                   .arena_base = nullptr,
                                   .arena_end = 0};
    }
}

// Everything up to the first colon is a device, as retail's LoadFile2 has it; sim: and host:
// were development devices that reached the same tree, so they are dropped like cdrom0:.
std::string_view StripDevice(std::string_view path) {
    std::size_t colon = path.find(':');
    return colon == std::string_view::npos ? path : path.substr(colon + 1);
}

// Use the same ZIP lookup keys for raw and host-escaped non-ASCII names.
std::string FoldArchivePath(std::string_view path) {
    constexpr std::string_view hex_digits = "0123456789abcdef";
    std::string                out;
    out.reserve(path.size());
    for (unsigned char c : path) {
        if (c > 0x7F) {
            out += '%';
            out += hex_digits[c >> 4];
            out += hex_digits[c & 0xF];
        } else {
            out += c == '\\' ? '/' : dcdata::FoldChar(static_cast<char>(c));
        }
    }
    out.erase(0, out.find_first_not_of('/'));
    return out;
}

[[noreturn]] void NoData(const fs::path &root, const char *why) {
    std::println(stderr,
                 "no game data: {} {}; extract the disc with `dcdata extract <disc image> {}` "
                 "or pass --data <dir>",
                 PathsDisplay(root), why, PathsDisplay(root));
    std::abort();
}

void CheckAgainstIndex(const fs::path &root) {
    auto found = data_index.find("data.hd2");
    if (found == data_index.end()) {
        return;
    }
    try {
        std::vector<unsigned char> hd2(static_cast<std::size_t>(found->second.size));
        if (found->second.archived) {
            if (!data_archive->Read(found->second.archive_index, hd2)) {
                throw std::runtime_error("cannot read data.hd2 from data.zip");
            }
        } else {
            std::ifstream stream(found->second.path, std::ios::binary);
            stream.read(reinterpret_cast<char *>(hd2.data()), static_cast<std::streamsize>(hd2.size()));
            if (static_cast<std::size_t>(stream.gcount()) != hd2.size()) {
                throw std::runtime_error("cannot read data.hd2");
            }
        }
        std::vector<dcdata::Record> records = dcdata::ParseIndex(hd2);
        std::size_t                 bad = 0;
        for (const dcdata::Record &record : records) {
            const std::string key = data_archive ? FoldArchivePath(record.path) : dcdata::FoldPath(record.path);
            const auto        asset = data_base_index.find(key);
            bad += asset == data_base_index.end() || asset->second.size != record.size;
        }
        if (bad != 0) {
            std::println(stderr,
                         "{}: {} of the {} files data.hd2 lists are missing or the wrong size; "
                         "extract the disc again",
                         PathsDisplay(root), bad, records.size());
        }
    } catch (const std::exception &error) {
        std::println(stderr, "{}: {}", PathsDisplay(root), error.what());
    }
}

const IndexedFile *Lookup(std::string_view path) {
    if (!data_indexed) {
        InitCDFile();
    }
    const std::string key = data_archive ? FoldArchivePath(StripDevice(path))
                                         : dcdata::FoldPath(StripDevice(path));
    auto              found = data_index.find(key);
    return found == data_index.end() ? nullptr : &found->second;
}

// The drive wrote whole sectors, so up to 2047 bytes past the file were overwritten and callers
// sized their buffers for that; the tail is zeroed rather than left as whatever was there.
int ReadWhole(const IndexedFile &file, void *buffer, std::size_t *out_capacity = nullptr) {
    const std::uintmax_t size = file.size;
    if (size > std::numeric_limits<std::int32_t>::max() || buffer == nullptr) {
        std::println(stderr, "cannot read {}", PathsDisplay(file.path));
        std::abort();
    }
    bool read = false;
    if (file.archived) {
        read = data_archive->Read(file.archive_index,
                                  std::span(static_cast<std::uint8_t *>(buffer), static_cast<std::size_t>(size)));
    } else {
        std::ifstream stream(file.path, std::ios::binary);
        stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
        read = std::cmp_equal(stream.gcount(), size);
    }
    if (!read) {
        if (file.archived) {
            std::println(stderr, "data.zip: cannot read or verify {}", file.path.generic_string());
        } else {
            std::println(stderr, "short read on {}", PathsDisplay(file.path));
        }
        std::abort();
    }
    std::size_t padded = dcdata::SectorsFor(size) * dcdata::kSector;
    std::ranges::fill(std::span(static_cast<std::uint8_t *>(buffer), padded).subspan(size), std::uint8_t{0});
    if (out_capacity != nullptr) {
        *out_capacity = padded;
    }
    return static_cast<int>(size);
}

// One file inside a pack: offsets are bytes from the entry, and a first byte of zero ends the table.
struct PackEntry {
    char name[64];
    int  offset;
    int  size;
    int  next;
};

bool SameName(const char *name, std::string_view other) {
    std::size_t length = std::strlen(name);
    return length == other.size() && dcdata::FoldPath(std::string_view(name, length)) == dcdata::FoldPath(other);
}

PackEntry *PackEntryAt(u_int *pack, std::size_t capacity, int wanted) {
    if (pack == nullptr || wanted < 0) {
        return nullptr;
    }
    auto       *bytes = reinterpret_cast<unsigned char *>(pack);
    std::size_t offset = 0;
    for (int index = 0; offset <= capacity && capacity - offset >= sizeof(PackEntry); index++) {
        auto *entry = reinterpret_cast<PackEntry *>(bytes + offset);
        if (entry->name[0] == 0 || std::memchr(entry->name, '\0', sizeof(entry->name)) == nullptr ||
            std::cmp_less(entry->next, sizeof(PackEntry)) || static_cast<std::size_t>(entry->next) > capacity - offset) {
            return nullptr;
        }
        if (index == wanted) {
            return entry;
        }
        offset += static_cast<std::size_t>(entry->next);
    }
    return nullptr;
}

bool PackNameDuplicated(u_int *pack, std::size_t capacity, const PackEntry *entry) {
    for (int index = 0;; index++) {
        PackEntry *candidate = PackEntryAt(pack, capacity, index);
        if (candidate == nullptr) {
            return false;
        }
        if (candidate != entry && SameName(candidate->name, entry->name)) {
            return true;
        }
    }
}

u_int *PackEntryData(u_int *pack, std::size_t capacity, PackEntry *entry) {
    auto      *bytes = reinterpret_cast<unsigned char *>(pack);
    const auto offset = static_cast<std::size_t>(reinterpret_cast<unsigned char *>(entry) - bytes);
    if (std::cmp_less(entry->offset, sizeof(PackEntry)) || entry->size < 0 ||
        static_cast<std::size_t>(entry->offset) > capacity - offset ||
        static_cast<std::size_t>(entry->size) > capacity - offset - static_cast<std::size_t>(entry->offset)) {
        return nullptr;
    }
    return reinterpret_cast<u_int *>(reinterpret_cast<unsigned char *>(entry) + entry->offset);
}

void TrackPackMember(const void *pack, const void *data, std::size_t size, const PackCapacity &parent,
                     const PackEntry *entry, int index, bool duplicate_name) {
    const auto       *pack_bytes = static_cast<const std::uint8_t *>(pack);
    const auto       *entry_bytes = reinterpret_cast<const std::uint8_t *>(entry);
    const std::size_t entry_offset = static_cast<std::size_t>(entry_bytes - pack_bytes) +
                                     static_cast<std::size_t>(entry->offset);
    std::string       member = parent.member;
    if (!member.empty()) {
        member.push_back('/');
    }
    constexpr std::string_view hex_digits = "0123456789ABCDEF";
    for (unsigned char byte : std::string_view(entry->name)) {
        if (byte >= 128 || byte == '%') {
            member.push_back('%');
            member.push_back(hex_digits[byte >> 4]);
            member.push_back(hex_digits[byte & 0xF]);
        } else {
            member.push_back(byte == '\\' ? '/' : static_cast<char>(byte));
        }
    }
    if (duplicate_name) {
        member.append("#entry:");
        member.append(std::to_string(index));
    }
    std::vector<std::uint64_t> member_offsets = parent.member_offsets;
    member_offsets.push_back(entry_offset);
    std::vector<std::uint64_t> member_source_offsets = parent.member_source_offsets;
    member_source_offsets.push_back(parent.source_offset + entry_offset);
    std::vector<std::uint64_t> member_sizes = parent.member_sizes;
    member_sizes.push_back(static_cast<std::size_t>(entry->size));
    TrackPackCapacity(data, size, parent.owner, parent.logical_path, member,
                      parent.source_offset + entry_offset, static_cast<std::size_t>(entry->size),
                      std::move(member_offsets), std::move(member_source_offsets), std::move(member_sizes));
}

std::string IndexedPath(const IndexedFile &file) {
    std::string path = file.source_path;
    std::ranges::replace(path, '\\', '/');
    return path;
}

// Hands a file just read to the localization layer: a message file may be rewritten from the
// language's JSON in place (it fits the whole sectors ReadWhole zeroed), and a pack is noted so
// its message entries can be told from GetPackFile.
void Localized(std::string_view path, void *buffer, int *size) {
    LocalizeLoaded(path, buffer);
    LocalizeFile(path, buffer, dcdata::SectorsFor(static_cast<std::uint64_t>(*size)) * dcdata::kSector, size);
}

} // namespace

void InitCDFile() {
    const fs::path &root = PathsDataRoot();
    std::error_code error;
    data_index.clear();
    data_base_index.clear();
    data_archive.reset();
    loaded_capacities.clear();
    copied_capacities_by_owner.clear();
    copied_capacity_count.store(0, std::memory_order_relaxed);
    data_indexed = false;
    if (DataArchivePresent(root)) {
        try {
            data_archive = std::make_unique<DataArchive>(root.parent_path() / "data.zip");
        } catch (const std::exception &exception) {
            std::println(stderr, "{}: {}", PathsDisplay(root.parent_path() / "data.zip"), exception.what());
            NoData(root, "has an invalid data.zip archive");
        }
        bool has_data_prefix = true;
        for (std::size_t index = 0; index < data_archive->Count(); ++index) {
            const std::string &name = data_archive->GetEntry(index).name;
            if (!name.ends_with('/') && !dcdata::FoldPath(name).starts_with("data/")) {
                has_data_prefix = false;
                break;
            }
        }
        auto add_archive_layer = [&](bool normalized) {
            std::unordered_set<std::string> layer_names;
            for (std::size_t index = 0; index < data_archive->Count(); ++index) {
                const DataArchive::Entry &entry = data_archive->GetEntry(index);
                std::string               relative = entry.name;
                if (relative.ends_with('/')) {
                    continue;
                }
                if (has_data_prefix) {
                    relative.erase(0, 5);
                }
                const std::string source_path = relative;
                if (relative.empty() || relative.starts_with("normalized/") != normalized) {
                    continue;
                }
                if (normalized) {
                    relative.erase(0, 11);
                }
                std::string key = FoldArchivePath(relative);
                if (!layer_names.emplace(key).second) {
                    NoData(root, "has duplicate case-insensitive asset names in data.zip");
                }
                IndexedFile indexed{.path = root / relative,
                                    .logical_path = relative,
                                    .source_path = source_path,
                                    .archive_index = index,
                                    .size = entry.size,
                                    .archived = true};
                if (normalized) {
                    data_index.insert_or_assign(std::move(key), std::move(indexed));
                } else {
                    data_base_index.emplace(key, indexed);
                    data_index.emplace(std::move(key), std::move(indexed));
                }
            }
        };
        add_archive_layer(false);
        add_archive_layer(true);
    } else {
        if (!fs::is_directory(root, error)) {
            NoData(root, "is not a directory");
        }
        auto options = fs::directory_options::follow_directory_symlink |
                       fs::directory_options::skip_permission_denied;
        for (fs::recursive_directory_iterator it(root, options, error), end; !error && it != end;
             it.increment(error)) {
            std::error_code entry_error;
            if (it->is_regular_file(entry_error)) {
                std::string relative = it->path().lexically_relative(root).generic_string();
                std::string key = dcdata::FoldPath(relative);
                IndexedFile indexed{.path = it->path(), .logical_path = relative, .source_path = relative, .size = it->file_size(entry_error)};
                if (!relative.starts_with("normalized/")) {
                    data_base_index.emplace(key, indexed);
                }
                if (!data_index.emplace(std::move(key), std::move(indexed)).second) {
                    NoData(root, "has duplicate case-insensitive asset names");
                }
            }
        }
        fs::path normalized = root / "normalized";
        if (fs::is_directory(normalized)) {
            for (const fs::directory_entry &entry : fs::recursive_directory_iterator(normalized)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().lexically_relative(normalized).generic_string();
                    data_index[dcdata::FoldPath(path)] = IndexedFile{.path = entry.path(),
                                                                     .logical_path = path,
                                                                     .source_path = std::string("normalized/") + path,
                                                                     .size = entry.file_size()};
                }
            }
        }
    }
    if (error) {
        std::println(stderr, "{}: {}", PathsDisplay(root), error.message());
    }
    if (data_index.empty()) {
        NoData(root, "is empty");
    }
    data_indexed = true;
    CheckAgainstIndex(root);
}

int LoadFile(char *path, void *buffer, int *out_size) {
    if (!LoadFile2(path, buffer, out_size, 0)) {
        std::print("File open error \"{}\"\n \n \n", path);
        __assert("etc.cpp", 753, "FALSE");
    }

    return 1;
}

int LoadFile2(char *path, void *buffer, int *out_size, int /*mode*/) {
    if (out_size) {
        *out_size = 0;
    }
    const IndexedFile *file = Lookup(path);
    if (!file) {
        return 0;
    }
    ForgetPackCapacities(buffer);
    std::size_t capacity = 0;
    int         size = ReadWhole(*file, buffer, &capacity);
    TrackPackCapacity(buffer, capacity, buffer, IndexedPath(*file), {}, 0, static_cast<std::size_t>(size));
    Localized(path, buffer, &size);
    if (out_size) {
        *out_size = size;
    }
    return 1;
}

std::uintmax_t DataFileSize(std::string_view path) {
    const IndexedFile *file = Lookup(path);
    return file != nullptr ? file->size : 0;
}

std::vector<std::string> DataReadListFiles() {
    if (!data_indexed) {
        InitCDFile();
    }
    std::vector<std::string> paths;
    paths.reserve(data_index.size());
    for (const auto &[path, file] : data_index) {
        paths.push_back(path);
    }
    return paths;
}

bool DataReadReadFile(std::string_view path, std::vector<std::uint8_t> &bytes) {
    const IndexedFile *file = Lookup(path);
    if (!file || file->size > std::numeric_limits<std::size_t>::max()) {
        bytes.clear();
        return false;
    }
    bytes.resize(static_cast<std::size_t>(file->size));
    if (file->archived) {
        if (data_archive->Read(file->archive_index, bytes)) {
            TrackTextureRead(*file);
            return true;
        }
        bytes.clear();
        return false;
    }
    std::ifstream stream(file->path, std::ios::binary);
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (static_cast<std::size_t>(stream.gcount()) != bytes.size()) {
        bytes.clear();
        return false;
    }
    TrackTextureRead(*file);
    return true;
}

DataTextureReadStats DataReadTextureReadStats() {
    std::scoped_lock lock(texture_read_stats_mutex);
    return texture_read_stats;
}

void DataReadSetTextureReadStatsEnabled(bool enabled) {
    std::scoped_lock lock(texture_read_stats_mutex);
    texture_read_stats = {};
    texture_read_stats_enabled.store(enabled, std::memory_order_relaxed);
}

bool DataReadReadResolvedFile(std::string_view source_path, std::vector<std::uint8_t> &bytes) {
    if (!data_indexed) {
        InitCDFile();
    }
    const std::string  key = FoldArchivePath(source_path);
    const IndexedFile *resolved = nullptr;
    for (const auto &entry : data_index) {
        const IndexedFile &file = entry.second;
        if (FoldArchivePath(file.source_path) != key) {
            continue;
        }
        if (resolved != nullptr && resolved != &file) {
            bytes.clear();
            return false;
        }
        resolved = &file;
    }
    if (resolved == nullptr || resolved->size > std::numeric_limits<std::size_t>::max()) {
        bytes.clear();
        return false;
    }
    bytes.resize(static_cast<std::size_t>(resolved->size));
    if (resolved->archived) {
        if (data_archive->Read(resolved->archive_index, bytes)) {
            return true;
        }
    } else {
        std::ifstream stream(resolved->path, std::ios::binary);
        stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (static_cast<std::size_t>(stream.gcount()) == bytes.size()) {
            return true;
        }
    }
    bytes.clear();
    return false;
}

bool DataReadSourceRange(const void *bytes, std::size_t size, DataReadSource &source) {
    if (bytes == nullptr) {
        return false;
    }
    const auto          address = reinterpret_cast<std::uintptr_t>(bytes);
    const PackCapacity *best = nullptr;
    std::size_t         best_offset = 0;
    bool                ambiguous = false;
    for (const auto &[base_pointer, range] : loaded_capacities) {
        if (range.logical_path.empty() || range.source_size == 0) {
            continue;
        }
        if (range.copied) {
            const auto *arena = static_cast<const CDataAlloc2<1> *>(range.owner);
            if (arena == nullptr || arena->base != range.arena_base || arena->used < 0 ||
                static_cast<std::size_t>(arena->used) > std::numeric_limits<std::size_t>::max() / 16 ||
                static_cast<std::size_t>(arena->used) * 16 < range.arena_end) {
                continue;
            }
        }
        const auto base = reinterpret_cast<std::uintptr_t>(base_pointer);
        if (address < base || address - base >= range.source_size) {
            continue;
        }
        const auto offset = static_cast<std::size_t>(address - base);
        if (best == nullptr || range.source_size < best->source_size) {
            best = &range;
            best_offset = offset;
            ambiguous = false;
        } else if (range.source_size == best->source_size &&
                   (range.logical_path != best->logical_path || range.member != best->member ||
                    range.source_offset != best->source_offset)) {
            ambiguous = true;
        }
    }
    if (best == nullptr || ambiguous || best_offset > best->source_size || size > best->source_size - best_offset) {
        return false;
    }
    source.path = best->logical_path;
    source.member = best->member;
    source.member_offsets = best->member_offsets;
    source.member_source_offsets = best->member_source_offsets;
    source.member_sizes = best->member_sizes;
    source.offset = best->source_offset + best_offset;
    source.range_size = best->source_size;
    source.size = size;
    return true;
}

bool DataReadRegisterCopiedSource(const void *destination, std::size_t size, const void *owner,
                                  const DataReadSource &source) {
    if (destination == nullptr || owner == nullptr || size == 0 || size > source.size ||
        source.path.empty() || source.range_size == 0 || size > source.range_size) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(destination);
    if (size > std::numeric_limits<std::uintptr_t>::max() - start) {
        return false;
    }
    const auto *arena = static_cast<const CDataAlloc2<1> *>(owner);
    const auto  arena_start = reinterpret_cast<std::uintptr_t>(arena->base);
    if (arena->base == nullptr || arena->used < 0 ||
        static_cast<std::size_t>(arena->used) > std::numeric_limits<std::size_t>::max() / 16 ||
        start < arena_start || start - arena_start > static_cast<std::size_t>(arena->used) * 16 ||
        size > static_cast<std::size_t>(arena->used) * 16 - (start - arena_start)) {
        return false;
    }
    const auto end = start + size;
    for (auto at = loaded_capacities.begin(); at != loaded_capacities.end();) {
        const auto existing_start = reinterpret_cast<std::uintptr_t>(at->first);
        const auto existing_end = existing_start + at->second.source_size;
        if (at->second.copied && at->second.owner == owner && start < existing_end && existing_start < end) {
            ForgetCopiedCapacity(at->first, at->second);
            at = loaded_capacities.erase(at);
        } else {
            ++at;
        }
    }
    const std::size_t arena_end = static_cast<std::size_t>(start - arena_start) + size;
    auto              previous = loaded_capacities.find(destination);
    if (previous != loaded_capacities.end()) {
        ForgetCopiedCapacity(destination, previous->second);
        loaded_capacities.erase(previous);
    }
    loaded_capacities[destination] = {.capacity = size,
                                      .owner = owner,
                                      .logical_path = source.path,
                                      .member = source.member,
                                      .member_offsets = source.member_offsets,
                                      .member_source_offsets = source.member_source_offsets,
                                      .member_sizes = source.member_sizes,
                                      .source_offset = static_cast<std::size_t>(source.offset),
                                      .source_size = size,
                                      .copied = true,
                                      .arena_base = arena->base,
                                      .arena_end = arena_end};
    copied_capacities_by_owner[owner].insert(destination);
    copied_capacity_count.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void DataReadInvalidateCopiedSources(const void *owner, std::size_t allocation_quads) {
    if (copied_capacity_count.load(std::memory_order_relaxed) == 0) {
        return;
    }
    const auto *arena = static_cast<const CDataAlloc2<1> *>(owner);
    auto        owner_at = copied_capacities_by_owner.find(owner);
    if (arena == nullptr || arena->base == nullptr || owner_at == copied_capacities_by_owner.end()) {
        return;
    }
    if (arena->used < 0 || std::cmp_greater(allocation_quads, std::numeric_limits<int>::max() - arena->used)) {
        ForgetPackCapacities(owner);
        return;
    }
    const std::size_t used_bytes = static_cast<std::size_t>(arena->used) * 16;
    const std::size_t allocation_start = used_bytes;
    const std::size_t allocation_end = (static_cast<std::size_t>(arena->used) + allocation_quads) * 16;
    const auto        arena_start = reinterpret_cast<std::uintptr_t>(arena->base);
    for (auto at = owner_at->second.begin(); at != owner_at->second.end();) {
        auto range = loaded_capacities.find(*at);
        if (range == loaded_capacities.end()) {
            at = owner_at->second.erase(at);
            continue;
        }
        const auto address = reinterpret_cast<std::uintptr_t>(range->first);
        if (address < arena_start) {
            loaded_capacities.erase(range);
            at = owner_at->second.erase(at);
            copied_capacity_count.fetch_sub(1, std::memory_order_relaxed);
            continue;
        }
        const auto        start = static_cast<std::size_t>(address - arena_start);
        const std::size_t end = range->second.arena_end;
        if (range->second.arena_base != arena->base || end > used_bytes ||
            (allocation_quads != 0 && start < allocation_end && allocation_start < end)) {
            loaded_capacities.erase(range);
            at = owner_at->second.erase(at);
            copied_capacity_count.fetch_sub(1, std::memory_order_relaxed);
        } else {
            ++at;
        }
    }
    if (owner_at->second.empty()) {
        copied_capacities_by_owner.erase(owner_at);
    }
}

void DataReadClearPackCapacities() {
    loaded_capacities.clear();
    copied_capacities_by_owner.clear();
    copied_capacity_count.store(0, std::memory_order_relaxed);
}

void DataReadForgetPackCapacities(const void *owner) {
    if (owner != nullptr) {
        ForgetPackCapacities(owner);
    }
}

void DataReadReset() {
    loaded_capacities.clear();
    data_index.clear();
    data_base_index.clear();
    data_archive.reset();
    data_indexed = false;
    copied_capacities_by_owner.clear();
    copied_capacity_count.store(0, std::memory_order_relaxed);
    std::scoped_lock lock(texture_read_stats_mutex);
    texture_read_stats = {};
}

// languages.json is {"release": "...", "languages": [n, ...]}; the numbers in the brackets after
// "languages" are the languages.
std::vector<s32> DataLanguages() {
    std::vector<s32>   languages;
    const IndexedFile *file = Lookup(dcdata::kLanguagesFile);
    if (!file) {
        return languages;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file->size));
    const bool                loaded = file->archived
                                           ? data_archive->Read(file->archive_index, bytes)
                                           : [&] {
                                  std::ifstream stream(file->path, std::ios::binary);
                                  stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                                  return static_cast<std::size_t>(stream.gcount()) == bytes.size();
                                             }();
    if (!loaded) {
        return languages;
    }
    std::string text(bytes.begin(), bytes.end());
    std::size_t key = text.find("\"languages\"");
    std::size_t open = key == std::string::npos ? key : text.find('[', key);
    std::size_t close = open == std::string::npos ? open : text.find(']', open);
    if (close == std::string::npos) {
        return languages;
    }
    const char *at = text.data() + open + 1;
    const char *end = text.data() + close;
    while (at < end) {
        s32 language = 0;
        auto [next, error] = std::from_chars(at, end, language);
        if (error == std::errc() && language >= LANG_JAPANESE && language < LANG_COUNT) {
            languages.push_back(language);
        }
        at = error == std::errc() ? next : at + 1;
    }
    return languages;
}

std::vector<s32> SupportedLanguages() {
    return DataLanguages();
}

void InitReadBG() {
    for (BG_READ_INFO &info : bg_read_info) {
        info.busy = false;
    }
}

void StartReadBG() {
    InitReadBG();
}

void BreakReadBG() {
    InitReadBG();
}

// The read happens here rather than a vertical sync later, so the slot is handed out already
// complete: issued (id) and done, as retail's ReadBG leaves it.
int LoadFileBG(char *name, u_long128 *buffer, int *out_size) {
    if (out_size) {
        *out_size = 0;
    }
    if (!name || *name == 0) {
        return 0;
    }
    BG_READ_INFO *info = nullptr;
    for (BG_READ_INFO &slot : bg_read_info) {
        if (!slot.busy) {
            info = &slot;
            break;
        }
    }
    if (!info) {
        return 0;
    }
    if (reinterpret_cast<std::uintptr_t>(buffer) % 64 != 0) {
        printf("/*/*/*/*/not 64byte align at %p %s\n", static_cast<void *>(buffer), name);
    }
    const IndexedFile *file = Lookup(name);
    if (!file) {
        return 0;
    }
    ForgetPackCapacities(buffer);
    std::size_t capacity = 0;
    int         size = ReadWhole(*file, buffer, &capacity);
    TrackPackCapacity(buffer, capacity, buffer, IndexedPath(*file), {}, 0, static_cast<std::size_t>(size));
    Localized(name, buffer, &size);
    *std::format_to_n(info->name, sizeof(info->name) - 1, "{}", name).out = '\0';
    info->busy = true;
    info->id = 1;
    info->done = 1;
    info->buffer = buffer;
    info->size = size;
    info->sector = 0;
    info->sectors = static_cast<int>(dcdata::SectorsFor(size));
    if (out_size) {
        *out_size = size;
    }
    return 1;
}

BG_READ_INFO *GetReadBGFile(int index) {
    if (index < 0 || std::cmp_greater_equal(index, bg_read_info.size())) {
        return nullptr;
    }
    return bg_read_info[index].busy ? &bg_read_info[index] : nullptr;
}

void ReadBG() {
}

int ReadBGSync() {
    for (const BG_READ_INFO &info : bg_read_info) {
        if (info.busy && (info.id == 0 || info.done == 0)) {
            return 1;
        }
    }
    return 0;
}

// Only development builds wrote files, to the host PC's drive; the device and any drive letter
// after it are dropped so the path lands under save/host0/.
int WriteFile(char *path, void *buffer, int size) {
    std::string_view name = path;
    if (std::size_t colon = name.rfind(':'); colon != std::string_view::npos) {
        name = name.substr(colon + 1);
    }
    std::string relative(name);
    std::ranges::replace(relative, '\\', '/');
    relative.erase(0, relative.find_first_not_of('/'));
    if (!dcdata::IsSafeRelative(relative) || size < 0 || (buffer == nullptr && size != 0)) {
        return 0;
    }
    fs::path        target = PathsSaveRoot() / "host0" / fs::path(relative);
    std::error_code error;
    fs::create_directories(target.parent_path(), error);
    std::ofstream stream(target, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return 0;
    }
    stream.write(static_cast<const char *>(buffer), size);
    return stream ? 1 : 0;
}

// A pack is looked up by the last component of a path, as retail does; a message file in it is
// given as the language's JSON has it.
u_int *GetPackFile(u_int *pack, char *name, int *out_size) {
    if (out_size != nullptr) {
        *out_size = 0;
    }
    if (pack == nullptr || name == nullptr) {
        return nullptr;
    }
    auto capacity = loaded_capacities.find(pack);
    if (capacity == loaded_capacities.end()) {
        return nullptr;
    }
    std::string_view basename = name;
    if (std::size_t slash = basename.rfind('/'); slash != std::string_view::npos) {
        basename = basename.substr(slash + 1);
    }
    for (int index = 0;; index++) {
        PackEntry *entry = PackEntryAt(pack, capacity->second.capacity, index);
        if (entry == nullptr) {
            return nullptr;
        }
        if (SameName(entry->name, basename)) {
            u_int *data = PackEntryData(pack, capacity->second.capacity, entry);
            if (data == nullptr) {
                return nullptr;
            }
            int size = entry->size;
            data = const_cast<u_int *>(LocalizePack(pack, basename, data, &size));
            TrackPackMember(pack, data, static_cast<std::size_t>(size), capacity->second, entry, index,
                            PackNameDuplicated(pack, capacity->second.capacity, entry));
            if (out_size) {
                *out_size = size;
            }
            return data;
        }
    }
}

u_int *GetPackFile(u_int *pack, int index, char **out_name, int *out_size) {
    if (out_size != nullptr) {
        *out_size = 0;
    }
    auto capacity = loaded_capacities.find(pack);
    if (capacity == loaded_capacities.end()) {
        return nullptr;
    }
    PackEntry *entry = PackEntryAt(pack, capacity->second.capacity, index);
    if (entry == nullptr) {
        return nullptr;
    }
    u_int *data = PackEntryData(pack, capacity->second.capacity, entry);
    if (data == nullptr) {
        return nullptr;
    }
    int size = entry->size;
    data = const_cast<u_int *>(LocalizePack(pack, entry->name, data, &size));
    TrackPackMember(pack, data, static_cast<std::size_t>(size), capacity->second, entry, index,
                    PackNameDuplicated(pack, capacity->second.capacity, entry));
    if (out_name != nullptr) {
        *out_name = entry->name;
    }
    if (out_size != nullptr) {
        *out_size = size;
    }
    return data;
}

int GetPackFileExt(u_int *pack, char *extension, u_int **files, int max_files, int *sizes, char **names) {
    if (extension == nullptr || files == nullptr || max_files <= 0) {
        return 0;
    }
    int found = 0;
    for (int index = 0; found < max_files; index++) {
        char  *name = nullptr;
        int    size = 0;
        u_int *data = GetPackFile(pack, index, &name, &size);
        if (data == nullptr) {
            break;
        }
        std::string_view       full_name(name);
        const std::size_t      dot = full_name.rfind('.');
        const std::string_view found_extension = dot == std::string_view::npos ? full_name : full_name.substr(dot + 1);
        if (dcdata::FoldPath(found_extension) != dcdata::FoldPath(extension)) {
            continue;
        }
        files[found] = data;
        if (sizes != nullptr) {
            sizes[found] = size;
        }
        if (names != nullptr) {
            names[found] = name;
        }
        found++;
    }
    return found;
}
