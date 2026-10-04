#pragma once

// The extractor's core, header-only so the port's tests and its data reader can use it without
// linking the tool. It includes no game header.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace dcdata {

namespace fs = std::filesystem;

inline constexpr std::uint64_t kSector = 2048;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <class... Args>
[[noreturn]] void Fail(std::format_string<Args...> format, Args &&...args) {
    throw Error(std::format(format, std::forward<Args>(args)...));
}

// strcasecmp in the C locale folds ASCII only, and that is the comparison every lookup in the
// game makes.
inline char FoldChar(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

inline bool EqualsFolded(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return FoldChar(x) == FoldChar(y);
           });
}

// The key a path is stored and looked up under: forward slashes, no leading separator, folded case.
inline std::string FoldPath(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
#if defined(__APPLE__) || defined(_WIN32)
        if (static_cast<unsigned char>(c) > 0x7F) {
            constexpr char kHex[] = "0123456789abcdef";
            out += '%';
            out += kHex[static_cast<unsigned char>(c) >> 4];
            out += kHex[static_cast<unsigned char>(c) & 0xF];
            continue;
        }
#endif
        out.push_back(c == '\\' ? '/' : FoldChar(c));
    }
    out.erase(0, out.find_first_not_of('/'));
    return out;
}

inline bool IsSafeRelative(std::string_view path) {
    if (path.empty() || path.back() == '/') {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t      end = std::min(path.find('/', start), path.size());
        std::string_view component = path.substr(start, end - start);
        if (component.empty() || component == "." || component == ".." ||
            component.find(':') != std::string_view::npos) {
            return false;
        }
        start = end + 1;
    }
    return true;
}

inline std::uint32_t Le32(const unsigned char *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

class Reader {
public:
    explicit Reader(const fs::path &path)
        : path_(path), stream_(path, std::ios::binary) {
        if (!stream_) {
            Fail("cannot open {}", path.string());
        }
        std::error_code error;
        size_ = fs::file_size(path, error);
        if (error) {
            Fail("cannot size {}: {}", path.string(), error.message());
        }
    }

    std::uint64_t Size() const {
        return size_;
    }

    const fs::path &Path() const {
        return path_;
    }

    void Read(std::uint64_t offset, void *destination, std::size_t size) {
        if (offset > size_ || size > size_ - offset) {
            Fail("{} is truncated: {} bytes at offset {} lie past its end ({} bytes)", path_.string(), size,
                 offset, size_);
        }
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(offset));
        stream_.read(static_cast<char *>(destination), static_cast<std::streamsize>(size));
        if (static_cast<std::size_t>(stream_.gcount()) != size) {
            Fail("read error in {} at offset {}", path_.string(), offset);
        }
    }

private:
    fs::path      path_;
    std::ifstream stream_;
    std::uint64_t size_ = 0;
};

// A byte range of a host file: DATA.DAT and DATA.HD2 are either files of their own or extents
// inside a disc image.
struct Extent {
    fs::path      file;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct IsoEntry {
    std::string   name;
    std::uint32_t extent = 0;
    std::uint32_t size = 0;
    bool          directory = false;
};

// ISO 9660 only: the PAL prototype's UDF bridge does not parse, and every file the game needs is
// reachable from the ISO 9660 tree.
class Iso9660 {
public:
    explicit Iso9660(const fs::path &image)
        : reader_(image) {
        unsigned char descriptor[kSector];
        for (std::uint64_t sector = 16;; sector++) {
            if ((sector + 1) * kSector > reader_.Size()) {
                Fail("{} has no ISO 9660 primary volume descriptor", image.string());
            }
            reader_.Read(sector * kSector, descriptor, sizeof descriptor);
            if (std::memcmp(descriptor + 1, "CD001", 5) != 0) {
                Fail("{} is not an ISO 9660 image", image.string());
            }
            if (descriptor[0] == 1) {
                break;
            }
            if (descriptor[0] == 255) {
                Fail("{} has no ISO 9660 primary volume descriptor", image.string());
            }
        }
        root_ = Parse(descriptor + 156, 34);
        if (!root_.directory) {
            Fail("{}: the root directory record is not a directory", image.string());
        }
    }

    const IsoEntry &Root() const {
        return root_;
    }

    std::vector<IsoEntry> List(const IsoEntry &directory) {
        std::vector<unsigned char> data(directory.size);
        reader_.Read(directory.extent * kSector, data.data(), data.size());
        std::vector<IsoEntry> entries;
        std::size_t           offset = 0;
        while (offset < data.size()) {
            std::size_t length = data[offset];
            // A record never crosses a sector; the rest of a sector after the last one is zero.
            if (length == 0) {
                offset = (offset / kSector + 1) * kSector;
                continue;
            }
            if (length < 34 || offset + length > data.size()) {
                Fail("{}: bad directory record at sector {} offset {}", reader_.Path().string(),
                     directory.extent, offset);
            }
            IsoEntry entry = Parse(&data[offset], length);
            offset += length;
            if (entry.name != std::string_view("\0", 1) && entry.name != "\1") {
                entries.push_back(std::move(entry));
            }
        }
        return entries;
    }

    std::optional<IsoEntry> Find(const IsoEntry &directory, std::string_view name) {
        for (IsoEntry &entry : List(directory)) {
            if (EqualsFolded(StripVersion(entry.name), name)) {
                return entry;
            }
        }
        return std::nullopt;
    }

    static std::string_view StripVersion(std::string_view identifier) {
        identifier = identifier.substr(0, identifier.find(';'));
        if (!identifier.empty() && identifier.back() == '.') {
            identifier.remove_suffix(1);
        }
        return identifier;
    }

private:
    static IsoEntry Parse(const unsigned char *record, std::size_t length) {
        std::size_t name_length = record[32];
        if (33 + name_length > length) {
            Fail("bad ISO 9660 directory record: name runs past the record");
        }
        IsoEntry entry;
        entry.extent = Le32(record + 2);
        entry.size = Le32(record + 10);
        entry.directory = (record[25] & 2) != 0;
        entry.name.assign(reinterpret_cast<const char *>(record + 33), name_length);
        return entry;
    }

    Reader   reader_;
    IsoEntry root_;
};

struct Archive {
    fs::path source;
    bool     image = false;
    Extent   dat;
    Extent   hd2;
};

inline Archive OpenArchive(const fs::path &source) {
    std::error_code error;
    if (fs::is_directory(source, error)) {
        Archive archive{source, false, {}, {}};
        for (const fs::directory_entry &entry : fs::directory_iterator(source)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            std::string name = entry.path().filename().string();
            if (EqualsFolded(name, "DATA.DAT")) {
                archive.dat = {entry.path(), 0, entry.file_size()};
            } else if (EqualsFolded(name, "DATA.HD2")) {
                archive.hd2 = {entry.path(), 0, entry.file_size()};
            }
        }
        if (archive.dat.file.empty() || archive.hd2.file.empty()) {
            Fail("{} holds no DATA.DAT and DATA.HD2", source.string());
        }
        return archive;
    }
    if (!fs::is_regular_file(source, error)) {
        Fail("{} is neither a disc image nor a directory", source.string());
    }
    Iso9660 iso(source);
    Archive archive{source, true, {}, {}};
    for (auto [name, extent] : {
             std::pair{"DATA.DAT", &archive.dat},
             std::pair{"DATA.HD2", &archive.hd2}
    }) {
        std::optional<IsoEntry> entry = iso.Find(iso.Root(), name);
        if (!entry || entry->directory) {
            Fail("{} has no {} in its root directory", source.string(), name);
        }
        *extent = {source, entry->extent * kSector, entry->size};
    }
    std::uint64_t image_size = fs::file_size(source);
    for (const Extent *extent : {&archive.dat, &archive.hd2}) {
        if (extent->offset + extent->size > image_size) {
            Fail("{} is truncated: it holds {} bytes, its directory places a file up to byte {}",
                 source.string(), image_size, extent->offset + extent->size);
        }
    }
    return archive;
}

inline std::vector<unsigned char> ReadExtent(const Extent &extent) {
    Reader                     reader(extent.file);
    std::vector<unsigned char> data(extent.size);
    reader.Read(extent.offset, data.data(), data.size());
    return data;
}

struct Record {
    std::string   path;
    std::string   name;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t sector = 0;
    std::uint32_t sectors = 0;
};

// DATA.HD2 as PAL's InitCDFile reads it: 32-byte records (name offset, three unused words, offset,
// size, sector, sector count) followed by the names, which the first record's name offset counts.
inline std::vector<Record> ParseIndex(std::span<const unsigned char> hd2) {
    if (hd2.size() < 32) {
        Fail("DATA.HD2 is {} bytes, too short for one record", hd2.size());
    }
    std::uint32_t count = Le32(hd2.data()) >> 5;
    if (count == 0 || static_cast<std::uint64_t>(count) * 32 > hd2.size()) {
        Fail("DATA.HD2 claims {} records but holds {} bytes", count, hd2.size());
    }
    std::vector<Record> records(count);
    for (std::uint32_t i = 0; i < count; i++) {
        const unsigned char *raw = hd2.data() + i * 32;
        std::uint32_t        name_offset = Le32(raw);
        // The retail index ends with one all-zero record, which the game's linear search
        // never matches; the count above includes it.
        if (name_offset >= hd2.size()) {
            Fail("DATA.HD2 record {}: name offset {} is past the end of the index", i, name_offset);
        }
        const char *name_start = reinterpret_cast<const char *>(hd2.data() + name_offset);
        const void *name_end = std::memchr(name_start, 0, hd2.size() - name_offset);
        if (!name_end) {
            Fail("DATA.HD2 record {}: name at {} is not terminated", i, name_offset);
        }
        Record &record = records[i];
        record.name.assign(name_start, static_cast<const char *>(name_end));
        if (record.name.empty() && i > 0 && Le32(raw + 20) == 0) {
            records.resize(i);
            break;
        }
        std::replace(record.name.begin(), record.name.end(), '\\', '/');
        record.path = FoldPath(record.name);
        if (!IsSafeRelative(record.path)) {
            Fail("DATA.HD2 record {}: unusable path \"{}\"", i, record.name);
        }
        record.offset = Le32(raw + 16);
        record.size = Le32(raw + 20);
        record.sector = Le32(raw + 24);
        record.sectors = Le32(raw + 28);
        if (record.size > 0x7FFFFFFF || record.sector > 0x7FFFFFFF) {
            Fail("DATA.HD2 record {} ({}): size {} or sector {} is negative", i, record.name, record.size,
                 record.sector);
        }
    }
    return records;
}

inline std::uint64_t SectorsFor(std::uint64_t size) {
    return (size + kSector - 1) / kSector;
}

struct Summary {
    std::size_t   records = 0;
    std::size_t   written = 0;
    std::size_t   kept = 0;
    std::size_t   duplicates = 0;
    std::size_t   warnings = 0;
    std::uint64_t bytes = 0;
    std::uint64_t written_bytes = 0;
};

// Reachable files and their bytes, kept files counting as done.
struct Progress {
    std::size_t   files = 0;
    std::size_t   total_files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t total_bytes = 0;
};

// Called after each file and each chunk written; false stops the extraction with an Error, leaving
// what was written so far for a later run to keep.
using ProgressCallback = std::function<bool(const Progress &)>;

template <class... Args>
void Log(std::FILE *log, std::format_string<Args...> format, Args &&...args) {
    if (log) {
        std::fputs(std::format(format, std::forward<Args>(args)...).c_str(), log);
    }
}

// Every record the game can reach: the first of each case-folded path, as PAL's linear search finds.
inline std::vector<const Record *> Reachable(const std::vector<Record> &records, std::FILE *log,
                                             std::size_t *duplicates) {
    std::vector<const Record *>     reachable;
    std::unordered_set<std::string> seen;
    for (const Record &record : records) {
        if (seen.insert(record.path).second) {
            reachable.push_back(&record);
        } else {
            ++*duplicates;
            Log(log, "warning: {} is listed again at sector {}; the game only ever finds the first\n",
                record.name, record.sector);
        }
    }
    return reachable;
}

inline void CheckFits(const std::vector<Record> &records, const Archive &archive, std::FILE *log,
                      std::size_t *warnings) {
    for (const Record &record : records) {
        std::uint64_t end = record.sector * kSector + record.size;
        if (end > archive.dat.size) {
            Fail("DATA.DAT is truncated: {} needs bytes up to {}, DATA.DAT holds {}", record.name, end,
                 archive.dat.size);
        }
        if (record.sectors < SectorsFor(record.size)) {
            ++*warnings;
            Log(log, "warning: {} is {} bytes but the index gives it {} sectors\n", record.name, record.size,
                record.sectors);
        }
    }
}

inline bool IsCurrent(const fs::path &path, std::uint64_t size) {
    std::error_code error;
    return fs::is_regular_file(path, error) && fs::file_size(path, error) == size && !error;
}

inline Summary Extract(const Archive &archive, const fs::path &out, std::FILE *log,
                       const ProgressCallback &progress = {}) {
    std::vector<unsigned char> hd2 = ReadExtent(archive.hd2);
    std::vector<Record>        records = ParseIndex(hd2);
    Summary                    summary;
    summary.records = records.size();
    CheckFits(records, archive, log, &summary.warnings);
    std::vector<const Record *> reachable = Reachable(records, log, &summary.duplicates);
    Progress                    done{.total_files = reachable.size()};
    for (const Record *record : reachable) {
        done.total_bytes += record->size;
    }
    auto report = [&] {
        if (progress && !progress(done)) {
            Fail("extraction cancelled");
        }
    };

    fs::create_directories(out);
    Reader                     dat(archive.dat.file);
    std::vector<unsigned char> chunk(4 << 20);
    for (const Record *record : reachable) {
        fs::path target = out / fs::path(record->path);
        summary.bytes += record->size;
        if (IsCurrent(target, record->size)) {
            summary.kept++;
            done.files++;
            done.bytes += record->size;
            report();
            continue;
        }
        fs::create_directories(target.parent_path());
        std::ofstream file(target, std::ios::binary | std::ios::trunc);
        if (!file) {
            Fail("cannot create {}", target.string());
        }
        std::uint64_t offset = archive.dat.offset + record->sector * kSector;
        for (std::uint64_t left = record->size; left != 0;) {
            std::size_t step = static_cast<std::size_t>(std::min<std::uint64_t>(left, chunk.size()));
            dat.Read(offset, chunk.data(), step);
            file.write(reinterpret_cast<const char *>(chunk.data()), static_cast<std::streamsize>(step));
            offset += step;
            left -= step;
            done.bytes += step;
            report();
        }
        file.close();
        if (!file) {
            Fail("write error on {}", target.string());
        }
        summary.written++;
        summary.written_bytes += record->size;
        done.files++;
        report();
    }

    fs::path      index_copy = out / "data.hd2";
    std::ofstream file(index_copy, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(hd2.data()), static_cast<std::streamsize>(hd2.size()));
    file.close();
    if (!file) {
        Fail("write error on {}", index_copy.string());
    }
    return summary;
}

// Files of the index that are missing from `root` or have the wrong size.
inline std::vector<const Record *> Mismatched(const std::vector<Record> &records, const fs::path &root) {
    std::vector<const Record *> bad;
    std::size_t                 duplicates = 0;
    for (const Record *record : Reachable(records, nullptr, &duplicates)) {
        if (!IsCurrent(root / fs::path(record->path), record->size)) {
            bad.push_back(record);
        }
    }
    return bad;
}

} // namespace dcdata
