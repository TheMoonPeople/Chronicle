#include "data/data_read.hpp"
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
#include <fstream>
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

#include "../../../tools/dcdata/dcdata.hpp"
#include "dataalloc.hpp"
#include "platform/data_archive.hpp"
#include "platform/load_work_profile.hpp"
#include "platform/paths.hpp"

namespace dc::data {

using namespace dc::game;
using namespace dc::platform;

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

std::unordered_map<std::string, IndexedFile>         data_index;
std::unordered_map<std::string, IndexedFile>         data_base_index;
std::unordered_map<std::string, const IndexedFile *> source_index;
std::unique_ptr<DataArchive>                         data_archive;

constexpr std::size_t kMetadataCacheFiles = 4;
constexpr std::size_t kMetadataCacheBytes = 16 * 1024 * 1024;

struct MetadataSnapshot {
    std::string                                      source_key;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
    std::uint64_t                                    epoch = 0;
};

std::array<MetadataSnapshot, kMetadataCacheFiles> metadata_snapshots;
std::size_t                                       metadata_snapshot_count = 0;
std::size_t                                       metadata_snapshot_bytes = 0;
std::uint64_t                                     data_index_epoch = 0;

bool MetadataSnapshotsEnabled() {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_TEXTURE_SOURCE_CACHE");
        return value == nullptr || std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

bool SourceLookupIndexed() {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_SOURCE_LOOKUP_INDEX");
        return value == nullptr || std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

void ResetMetadataSnapshots() {
    ++data_index_epoch;
    metadata_snapshots = {};
    metadata_snapshot_count = 0;
    metadata_snapshot_bytes = 0;
}

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

// A read writes over whatever earlier loads left at those addresses, whichever buffer they were
// read into. A record starting inside the new bytes no longer describes anything; one starting
// before them keeps only the bytes in front of them.
void ForgetOverwrittenCapacities(const void *bytes, std::size_t size) {
    const auto start = reinterpret_cast<std::uintptr_t>(bytes);
    const auto end = start + size;
    for (auto at = loaded_capacities.begin(); at != loaded_capacities.end();) {
        PackCapacity &range = at->second;
        const auto    base = reinterpret_cast<std::uintptr_t>(at->first);
        if (base >= end || base + std::max(range.capacity, range.source_size) <= start) {
            ++at;
        } else if (base >= start) {
            ForgetCopiedCapacity(at->first, range);
            at = loaded_capacities.erase(at);
        } else {
            const std::size_t kept = static_cast<std::size_t>(start - base);
            range.capacity = std::min(range.capacity, kept);
            range.source_size = std::min(range.source_size, kept);
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
        DataReadIndex();
    }
    const std::string key = data_archive ? FoldArchivePath(StripDevice(path))
                                         : dcdata::FoldPath(StripDevice(path));
    auto              found = data_index.find(key);
    return found == data_index.end() ? nullptr : &found->second;
}

const IndexedFile *ResolveSource(std::string_view key) {
    if (!SourceLookupIndexed()) {
        const IndexedFile *resolved = nullptr;
        for (const auto &entry : data_index) {
            const IndexedFile &file = entry.second;
            if (FoldArchivePath(file.source_path) != key) {
                continue;
            }
            if (resolved != nullptr && resolved != &file) {
                return nullptr;
            }
            resolved = &file;
        }
        return resolved;
    }
    const auto found = source_index.find(std::string(key));
    return found == source_index.end() ? nullptr : found->second;
}

bool ReadResolved(const IndexedFile *resolved, std::vector<std::uint8_t> &bytes) {
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

std::string IndexedPath(const IndexedFile &file) {
    std::string path = file.source_path;
    std::ranges::replace(path, '\\', '/');
    return path;
}

} // namespace

void DataReadIndex() {
    ResetMetadataSnapshots();
    const fs::path &root = PathsDataRoot();
    std::error_code error;
    source_index.clear();
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
    // Index the effective bindings after normalized overrides. Null preserves rejection
    // of source names that become ambiguous after escaping and case folding.
    if (SourceLookupIndexed()) {
        source_index.reserve(data_index.size());
        for (const auto &[logical, file] : data_index) {
            auto [at, inserted] = source_index.emplace(FoldArchivePath(file.source_path), &file);
            if (!inserted && at->second != &file) {
                at->second = nullptr;
            }
        }
    }
    data_indexed = true;
    CheckAgainstIndex(root);
}

bool DataReadLoadFile(std::string_view path, void *buffer, int *size) {
    const IndexedFile *file = Lookup(path);
    if (!file) {
        return false;
    }
    ForgetPackCapacities(buffer);
    std::size_t capacity = 0;
    *size = ReadWhole(*file, buffer, &capacity);
    ForgetOverwrittenCapacities(buffer, capacity);
    TrackPackCapacity(buffer, capacity, buffer, IndexedPath(*file), {}, 0, static_cast<std::size_t>(*size));
    return true;
}

std::uintmax_t DataFileSize(std::string_view path) {
    const IndexedFile *file = Lookup(path);
    return file != nullptr ? file->size : 0;
}

bool DataReadIndexed() {
    return data_indexed;
}

std::vector<std::string> DataReadListFiles() {
    if (!data_indexed) {
        DataReadIndex();
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
        DataReadIndex();
    }
    return ReadResolved(ResolveSource(FoldArchivePath(source_path)), bytes);
}

std::shared_ptr<const std::vector<std::uint8_t>> DataReadResolvedMetadataSnapshot(std::string_view source_path) {
    const platform::load_profile::Span profile(platform::load_profile::Kind::Metadata);
    if (!data_indexed) {
        DataReadIndex();
    }
    const bool        cache = MetadataSnapshotsEnabled() && data_archive != nullptr;
    const std::string key = FoldArchivePath(source_path);
    if (cache) {
        for (std::size_t index = 0; index < metadata_snapshot_count; ++index) {
            if (metadata_snapshots[index].epoch == data_index_epoch && metadata_snapshots[index].source_key == key) {
                auto bytes = metadata_snapshots[index].bytes;
                std::rotate(metadata_snapshots.begin(), metadata_snapshots.begin() + static_cast<std::ptrdiff_t>(index),
                            metadata_snapshots.begin() + static_cast<std::ptrdiff_t>(index + 1));
                return bytes;
            }
        }
    }
    const IndexedFile *resolved = ResolveSource(key);
    auto               bytes = std::make_shared<std::vector<std::uint8_t>>();
    if (!ReadResolved(resolved, *bytes)) {
        return nullptr;
    }
    if (cache && resolved->archived && bytes->size() <= kMetadataCacheBytes) {
        while (metadata_snapshot_count != 0 &&
               (metadata_snapshot_count == kMetadataCacheFiles ||
                metadata_snapshot_bytes > kMetadataCacheBytes - bytes->size())) {
            auto &last = metadata_snapshots[--metadata_snapshot_count];
            metadata_snapshot_bytes -= last.bytes->size();
            last = {};
        }
        std::move_backward(metadata_snapshots.begin(),
                           metadata_snapshots.begin() + static_cast<std::ptrdiff_t>(metadata_snapshot_count),
                           metadata_snapshots.begin() + static_cast<std::ptrdiff_t>(metadata_snapshot_count + 1));
        metadata_snapshots[0] = {key, bytes, data_index_epoch};
        ++metadata_snapshot_count;
        metadata_snapshot_bytes += bytes->size();
    }
    return bytes;
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
    // The copy overwrote whatever was recorded there, from this arena or any other that shares the memory.
    ForgetOverwrittenCapacities(destination, size);
    const std::size_t arena_end = static_cast<std::size_t>(start - arena_start) + size;
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
    ResetMetadataSnapshots();
    loaded_capacities.clear();
    source_index.clear();
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

bool DataReadPackCapacity(const void *pack, std::size_t &capacity) {
    auto found = loaded_capacities.find(pack);
    if (found == loaded_capacities.end()) {
        return false;
    }
    capacity = found->second.capacity;
    return true;
}

void DataReadTrackPackMember(const void *pack, const void *data, std::size_t size, std::size_t entry_offset,
                             std::string_view entry_name, std::size_t entry_size, int index, bool duplicate_name) {
    auto found = loaded_capacities.find(pack);
    if (found == loaded_capacities.end()) {
        return;
    }
    const PackCapacity &parent = found->second;
    std::string         member = parent.member;
    if (!member.empty()) {
        member.push_back('/');
    }
    constexpr std::string_view hex_digits = "0123456789ABCDEF";
    for (unsigned char byte : entry_name) {
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
    member_sizes.push_back(entry_size);
    TrackPackCapacity(data, size, parent.owner, parent.logical_path, member,
                      parent.source_offset + entry_offset, entry_size,
                      std::move(member_offsets), std::move(member_source_offsets), std::move(member_sizes));
}

} // namespace dc::data
