#pragma once

// The extractor's core, header-only so the port's tests and its data reader can use it without
// linking the tool. It includes no game header.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include <utility>
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

inline void Put32(unsigned char *p, std::uint32_t value) {
    for (int i = 0; i < 4; i++) {
        p[i] = static_cast<unsigned char>(value >> (8 * i));
    }
}

inline std::vector<unsigned char> ReadFile(const fs::path &path) {
    Reader                     reader(path);
    std::vector<unsigned char> data(reader.Size());
    reader.Read(0, data.data(), data.size());
    return data;
}

inline void WriteFile(const fs::path &path, std::span<const unsigned char> data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        Fail("cannot write {}", path.string());
    }
}

struct PackMember {
    std::string                name;
    std::vector<unsigned char> data;
};

inline std::vector<PackMember> ReadPack(std::span<const unsigned char> pack) {
    std::vector<PackMember> members;
    for (std::size_t entry = 0; entry + 76 <= pack.size() && pack[entry];) {
        const void *end = std::memchr(pack.data() + entry, 0, 64);
        if (!end) {
            Fail("pack entry at {} has no terminated name", entry);
        }
        std::size_t offset = Le32(pack.data() + entry + 64);
        std::size_t size = Le32(pack.data() + entry + 68);
        std::size_t next = Le32(pack.data() + entry + 72);
        if (offset < 76 || offset > pack.size() - entry || size > pack.size() - entry - offset ||
            next < 76 || next > pack.size() - entry) {
            Fail("invalid pack entry at {}", entry);
        }
        members.push_back({
            std::string(reinterpret_cast<const char *>(pack.data() + entry)),
            {pack.begin() + entry + offset, pack.begin() + entry + offset + size}
        });
        entry += next;
    }
    return members;
}

inline std::vector<unsigned char> WritePack(const std::vector<PackMember> &members) {
    std::vector<unsigned char> pack;
    for (const PackMember &member : members) {
        if (member.name.size() >= 64) {
            Fail("pack member name is too long: {}", member.name);
        }
        std::size_t entry = pack.size();
        pack.resize(entry + 80);
        std::memcpy(pack.data() + entry, member.name.c_str(), member.name.size() + 1);
        Put32(pack.data() + entry + 64, 80);
        Put32(pack.data() + entry + 68, static_cast<std::uint32_t>(member.data.size()));
        pack.insert(pack.end(), member.data.begin(), member.data.end());
        pack.resize((pack.size() + 15) & ~std::size_t{15});
        Put32(pack.data() + entry + 72, static_cast<std::uint32_t>(pack.size() - entry));
    }
    pack.resize(pack.size() + 76);
    return pack;
}

inline bool AddPackAlias(std::vector<PackMember> &members, std::string_view name,
                         std::string_view source) {
    for (const PackMember &member : members) {
        if (EqualsFolded(member.name, name)) {
            return false;
        }
    }
    for (const PackMember &member : members) {
        if (EqualsFolded(member.name, source)) {
            members.push_back({std::string(name), member.data});
            return true;
        }
    }
    return false;
}

// Where a picture named `name` lies in an IM2 bank: the offset of its TIM2 file and its length.
struct Im2Picture {
    std::size_t offset = 0;
    std::size_t size = 0;
};

inline Im2Picture FindIm2Picture(std::span<const unsigned char> bank, std::string_view name) {
    if (bank.size() < 16 || std::memcmp(bank.data(), "IM2", 3) != 0) {
        return {};
    }
    std::uint32_t count = Le32(bank.data() + 4);
    if (16 + std::uint64_t(count) * 48 > bank.size()) {
        return {};
    }
    for (std::uint32_t i = 0; i < count; i++) {
        const unsigned char *entry = bank.data() + 16 + i * 48;
        std::size_t          length = strnlen(reinterpret_cast<const char *>(entry), 32);
        if (std::string_view(reinterpret_cast<const char *>(entry), length) != name) {
            continue;
        }
        std::size_t offset = Le32(entry + 32);
        if (offset + 32 > bank.size() || std::memcmp(bank.data() + offset, "TIM2", 4) != 0) {
            return {};
        }
        // The picture header's total size is not reliable in the game's images; its parts are.
        const unsigned char *picture = bank.data() + offset + 16;
        std::size_t          size = 16 + std::size_t(picture[12] | picture[13] << 8) + Le32(picture + 8) + Le32(picture + 4);
        if (size > bank.size() - offset) {
            return {};
        }
        return {offset, size};
    }
    return {};
}

// An IM2 bank of the named TIM2 pictures, in order, as the game's banks lay them out.
inline std::vector<unsigned char> WriteIm2(
    const std::vector<std::pair<std::string, std::vector<unsigned char>>> &pictures) {
    std::vector<unsigned char> bank(16 + pictures.size() * 48, 0);
    std::memcpy(bank.data(), "IM2", 3);
    Put32(bank.data() + 4, static_cast<std::uint32_t>(pictures.size()));
    for (std::size_t i = 0; i < pictures.size(); i++) {
        const auto &[name, picture] = pictures[i];
        if (name.size() >= 32) {
            Fail("IM2 picture name is too long: {}", name);
        }
        std::memcpy(bank.data() + 16 + i * 48, name.c_str(), name.size());
        Put32(bank.data() + 16 + i * 48 + 32, static_cast<std::uint32_t>(bank.size()));
        bank.insert(bank.end(), picture.begin(), picture.end());
    }
    return bank;
}

// The colours of an 8-bit TIM2 picture's 256-entry CLUT as RGB, or none.
inline std::vector<std::uint32_t> Tim2Colours(std::span<const unsigned char> tim) {
    if (tim.size() < 48 || tim[16 + 0x13] != 5 || (tim[16 + 0x0E] | tim[16 + 0x0F] << 8) != 256) {
        return {};
    }
    std::size_t clut = 16 + (tim[16 + 0x0C] | tim[16 + 0x0D] << 8) + Le32(tim.data() + 16 + 8);
    if (clut + 1024 > tim.size()) {
        return {};
    }
    std::vector<std::uint32_t> colours(256);
    for (int i = 0; i < 256; i++) {
        colours[i] = Le32(tim.data() + clut + i * 4) & 0xFFFFFF;
    }
    return colours;
}

inline int SharedColours(const std::vector<std::uint32_t> &a, const std::vector<std::uint32_t> &b) {
    int shared = 0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); i++) {
        shared += a[i] == b[i];
    }
    return shared;
}

// The dungeon copies weapon icons by palette index from whichever wepicon sheet is loaded into the
// HUD's itempack: a dungeon-entry pack's on a new floor, then a menu's once the main menu or the
// quick-change ring closes or a script reloads the item list. So every wepicon sheet of a language
// has to share itempack's palette, as the American ones do. On the PAL disc only English's
// dungeon-entry sheets do, and the HUD icon comes out in the wrong colours after any of those; the
// other languages' fishing and shop sheets are off as well, though only the town shows them. A
// sheet in another palette takes the dungeon-entry sheet's place: the same icons in itempack's.
inline void NormalizeIconSheets(const fs::path &out, const std::vector<Record> &records) {
    // A sheet in another palette shares a handful of its colours with itempack's.
    constexpr int              kSharedPalette = 250;
    constexpr std::string_view prefix = "commenu/";
    constexpr std::string_view suffix = "/itempack.img";
    for (const Record &record : records) {
        if (!record.path.starts_with(prefix) || !record.path.ends_with(suffix)) {
            continue;
        }
        std::string language = record.path.substr(prefix.size(), record.path.size() - prefix.size() - suffix.size());
        if (language.empty() || language.find('/') != std::string::npos) {
            continue;
        }
        std::vector<unsigned char> itempack = ReadFile(out / record.path);
        Im2Picture                 hud = FindIm2Picture(itempack, "itempack");
        if (hud.size == 0) {
            continue;
        }
        std::vector<std::uint32_t> hud_colours = Tim2Colours(std::span(itempack).subspan(hud.offset, hud.size));

        std::vector<unsigned char> donor;
        std::string                entry_packs = std::format("commenu/{}/dunenter/", language);
        for (const Record &other : records) {
            if (!donor.empty() || !other.path.starts_with(entry_packs) || !other.path.ends_with(".pak")) {
                continue;
            }
            for (const PackMember &member : ReadPack(ReadFile(out / other.path))) {
                Im2Picture icons = FindIm2Picture(member.data, "wepicon");
                if (icons.size != 0 &&
                    SharedColours(Tim2Colours(std::span(member.data).subspan(icons.offset, icons.size)), hud_colours) >= kSharedPalette) {
                    donor.assign(member.data.begin() + icons.offset, member.data.begin() + icons.offset + icons.size);
                    break;
                }
            }
        }
        if (donor.empty()) {
            continue;
        }

        // Whether the bank's wepicon was in another palette and the donor took its place.
        auto replace = [&](std::vector<unsigned char> &bank) {
            Im2Picture icons = FindIm2Picture(bank, "wepicon");
            if (icons.size != donor.size() ||
                SharedColours(Tim2Colours(std::span(bank).subspan(icons.offset, icons.size)), hud_colours) >= kSharedPalette) {
                return false;
            }
            std::copy(donor.begin(), donor.end(), bank.begin() + icons.offset);
            return true;
        };
        // Every image bank and pack of the language, but the leftovers the game never names (_x, x.old).
        std::string folder = std::format("commenu/{}/", language);
        for (const Record &file : records) {
            std::string_view path = file.path;
            std::string_view name = path.substr(path.rfind('/') + 1);
            bool             bank = path.ends_with(".img");
            if (!path.starts_with(folder) || name.starts_with('_') ||
                !(bank || path.ends_with(".pak") || path.ends_with(".pac"))) {
                continue;
            }
            std::vector<unsigned char> data = ReadFile(out / file.path);
            if (bank) {
                if (replace(data)) {
                    WriteFile(out / "normalized" / file.path, data);
                }
                continue;
            }
            std::vector<PackMember> members;
            try {
                members = ReadPack(data);
            } catch (const Error &) {
                continue; // a .pac that holds no pack
            }
            bool changed = false;
            for (PackMember &member : members) {
                changed |= replace(member.data);
            }
            if (changed) {
                WriteFile(out / "normalized" / file.path, WritePack(members));
            }
        }
    }
}

// The town system pack: one on NTSC, which PAL split into a common part and one per language.
inline constexpr std::string_view kSystemPack = "gedit/system/esys.pak";
inline constexpr std::string_view kPalCommonSystemPack = "gedit/system/esys_cmn.pak";

// Whether a disc's files are NTSC's, by the presence of each system pack.
inline bool IsNtscLayout(bool has_system_pack, bool has_pal_common_system_pack) {
    return has_system_pack && !has_pal_common_system_pack;
}

// What the extraction can run, written to the data's root for the port: the release it came from
// and the languages its disc carries, as LanguageCode numbers. Localization packs, once they exist,
// add their own languages to these in the port.
inline constexpr std::string_view kLanguagesFile = "languages.json";

// A release the extractor knows by a fingerprint of its DATA.HD2, the index of every file on the
// disc, which differs from release to release.
struct Release {
    std::string_view name;
    std::uint64_t    index_size;
    std::uint64_t    index_fnv; // FNV-1a, 64-bit
    std::vector<int> languages;
    // Whether its other languages' floor plates are blank, to be filled from American English's.
    bool blank_floor_plates = false;
};

// NTSC shipped American English alone. The disc's other languages are early drafts whose images
// were never translated (its French and German title cards read "Nolun Village" and "Sun/Moon
// Temple"), its British-named files have no town dialogue, and its Spanish is missing files.
inline const std::vector<int> &NtscLanguages() {
    static const std::vector<int> languages = {1};
    return languages;
}

// PAL: the five languages its select offers, English being British.
inline const std::vector<int> &PalLanguages() {
    static const std::vector<int> languages = {2, 3, 4, 5, 6};
    return languages;
}

inline const std::vector<Release> &KnownReleases() {
    static const std::vector<Release> releases = {
        {"NTSC 1.02",                     294080, 0x806bb1d43be8a8dcULL, NtscLanguages()},
        {"PAL prototype (July 12, 2001)", 312864, 0x5942a32563c2a8b1ULL, PalLanguages(),  true},
    };
    return releases;
}

inline std::uint64_t Fnv1a64(std::span<const unsigned char> data) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char byte : data) {
        hash = (hash ^ byte) * 0x100000001b3ULL;
    }
    return hash;
}

inline const Release *IdentifyRelease(std::span<const unsigned char> index) {
    std::uint64_t fnv = Fnv1a64(index);
    for (const Release &release : KnownReleases()) {
        if (release.index_size == index.size() && release.index_fnv == fnv) {
            return &release;
        }
    }
    return nullptr;
}

// The release by its index, or for one the table lacks, its family by the file layout: NTSC's
// languages where the disc has NTSC's system pack alone, PAL's otherwise.
inline Release ReleaseOf(std::span<const unsigned char> index, const std::vector<Record> &records) {
    if (const Release *known = IdentifyRelease(index)) {
        return *known;
    }
    bool system_pack = false;
    bool pal_common = false;
    for (const Record &record : records) {
        system_pack |= record.path == kSystemPack;
        pal_common |= record.path == kPalCommonSystemPack;
    }
    if (IsNtscLayout(system_pack, pal_common)) {
        return {"unknown release, NTSC layout", index.size(), Fnv1a64(index), NtscLanguages()};
    }
    return {"unknown release, PAL layout", index.size(), Fnv1a64(index), PalLanguages()};
}

inline void WriteLanguages(const fs::path &out, const Release &release) {
    std::string text = std::format("{{\"release\": \"{}\", \"languages\": [", release.name);
    for (std::size_t i = 0; i < release.languages.size(); i++) {
        text += std::format("{}{}", i == 0 ? "" : ", ", release.languages[i]);
    }
    text += "]}\n";
    WriteFile(out / kLanguagesFile, std::span(reinterpret_cast<const unsigned char *>(text.data()), text.size()));
}

// PAL split NTSC's town system packs. esys_cmn.pak holds what every language shares, with
// sys_cmn.img the three pictures of NTSC's system.img that PAL kept; sys_N.img (sys.img for
// Japanese) holds a language's pause and day-of-adventure pictures, which NTSC packs in esys_N.pak
// as pause.img and whatsday.img. NTSC's gaiji, fuki256 and syst04 are meswin's, and its
// skip_bord and second pause PAL dropped. From NTSC 1.02 these are PAL's members and pictures exactly.
inline void SplitSystemPacks(const fs::path &out, const std::unordered_set<std::string> &paths) {
    auto picture = [](const std::vector<PackMember> &members, std::string_view image, std::string_view name,
                      std::string_view pack) {
        for (const PackMember &member : members) {
            if (EqualsFolded(member.name, image)) {
                Im2Picture found = FindIm2Picture(member.data, name);
                if (found.size == 0) {
                    break;
                }
                return std::vector<unsigned char>(member.data.begin() + found.offset,
                                                  member.data.begin() + found.offset + found.size);
            }
        }
        Fail("{} has no {} picture in {}", pack, name, image);
    };
    auto write = [&](std::string_view name, const std::vector<unsigned char> &data) {
        fs::path target = out / name;
        if (paths.contains(std::string(name)) || (fs::is_regular_file(target) && ReadFile(target) == data)) {
            return;
        }
        WriteFile(target, data);
    };
    for (int language = 0; language <= 6; language++) {
        std::string pack = language == 0 ? std::string(kSystemPack)
                                         : std::format("gedit/system/esys_{}.pak", language);
        if (!fs::is_regular_file(out / pack)) {
            continue;
        }
        std::vector<PackMember> members = ReadPack(ReadFile(out / pack));
        write(language == 0 ? "gedit/system/sys.img" : std::format("gedit/system/sys_{}.img", language),
              WriteIm2({
                  {"pause",    picture(members, "pause.img",    "pause",    pack)},
                  {"whatsday", picture(members, "whatsday.img", "whatsday", pack)}
        }));
        if (language != 0) {
            continue;
        }
        std::vector<PackMember> common;
        for (const PackMember &member : members) {
            bool moved = false;
            for (std::string_view name : {"gaiji.img", "syst04.img", "fuki256.img", "system.img", "pause.img", "whatsday.img"}) {
                moved |= EqualsFolded(member.name, name);
            }
            if (!moved) {
                common.push_back(member);
            }
        }
        common.push_back({
            "sys_cmn.img", WriteIm2({{"syst08", picture(members, "system.img", "syst08", pack)},
                                     {"pnplate", picture(members, "system.img", "pnplate", pack)},
                                     {"dayclock", picture(members, "system.img", "dayclock", pack)}}
             )
        });
        write(kPalCommonSystemPack, WritePack(common));
    }
}

inline void Normalize(const fs::path &out, const std::vector<Record> &records) {
    NormalizeIconSheets(out, records);
    std::unordered_set<std::string> paths;
    for (const Record &record : records) {
        paths.insert(record.path);
    }
    if (!IsNtscLayout(paths.contains(std::string(kSystemPack)),
                      paths.contains(std::string(kPalCommonSystemPack)))) {
        return;
    }
    auto alias = [&](std::string_view name, std::string_view source) {
        fs::path target = out / name;
        fs::path original = out / source;
        if (!paths.contains(std::string(name)) && fs::is_regular_file(original) &&
            !IsCurrent(target, fs::file_size(original))) {
            fs::create_directories(target.parent_path());
            fs::copy_file(original, target, fs::copy_options::overwrite_existing);
        }
    };
    for (int language = 1; language <= 6; language++) {
        alias(std::format("meswin/mes_tex_{}.pak", language), "meswin/mes_tex.pak");
        alias(std::format("dun/pack/teximg2_{}.pac", language), "dun/pack/teximg2.pac");
    }
    for (const char *language : {"jp", "us_e", "fr", "gr", "it", "sp"}) {
        for (int map = 0; map <= 6; map++) {
            alias(std::format("dun/img/{}/dname0{}.img", language, map),
                  std::format("dun/img/us/dname0{}.img", map));
        }
    }
    alias("gedit/system/editsys_2.mes", "gedit/system/editsys.bin");
    alias("meswin/system_2.mes", "meswin/systeme.bin");
    alias("meswin/system14_2.mes", "meswin/system14_1.mes");
    alias("opdat/optext_2.mes", "opdat/usa/fconv.bin");
    alias("opdat/optext_6.mes", "opdat/usa/fconv.bin");
    alias("titledat/title_eu.pak", "titledat/title.pak");
    for (char language : {'e', 'f', 'g', 'i', 's'}) {
        alias(std::format("titledat/title_{}.pak", language), "titledat/title.pak");
    }
    SplitSystemPacks(out, paths);
    for (int map = 1; map <= 7; map++) {
        fs::path event = out / std::format("dun/script/d{:02}/event.stb", map);
        if (!fs::is_regular_file(event)) {
            continue;
        }
        std::vector<unsigned char> data = ReadFile(event);
        if (data.size() < 24 || Le32(data.data() + 20) >= data.size()) {
            Fail("invalid event script in {}", event.string());
        }
        for (int language = 1; language <= 6; language++) {
            fs::path target = out / std::format("dun/script/d{:02}/d{:02}_{}.mes", map, map, language);
            if (!paths.contains(FoldPath(target.lexically_relative(out).generic_string()))) {
                WriteFile(target, std::span(data).subspan(Le32(data.data() + 20)));
            }
        }
    }
    for (const Record &record : records) {
        bool rooms = record.path.starts_with("rmdat/rmdat") && record.path.ends_with(".pak");
        bool pause = record.path == "opdat/dungeon/dungeon.pim" ||
                     record.path == "opdat/norn/norn.pak" ||
                     record.path == "opdat/norn2/norn2.pim" ||
                     record.path == "opdat/toan/toan.pim";
        bool battle = record.path.find("/dungeon/dunmenu5.pak") != std::string::npos;
        if (!rooms && !pause && !battle) {
            continue;
        }
        std::vector<PackMember> members = ReadPack(ReadFile(out / record.path));
        bool                    changed = false;
        for (char language : {'f', 'g', 'i', 's'}) {
            changed |= AddPackAlias(members, std::format("{}{}.img", rooms ? "start_" : "pause_", language),
                                    rooms ? "start.img" : "pause_e.img");
        }
        if (battle) {
            auto existing = std::find_if(members.begin(), members.end(), [](const PackMember &member) {
                return EqualsFolded(member.name, "btlmenu2.img");
            });
            auto image = std::find_if(members.begin(), members.end(), [](const PackMember &member) {
                return EqualsFolded(member.name, "btlmenu.img");
            });
            if (existing == members.end() && image != members.end()) {
                const auto &data = image->data;
                if (data.size() < 16 || std::memcmp(data.data(), "IM2", 3) != 0 ||
                    16 + std::uint64_t(Le32(data.data() + 4)) * 48 > data.size()) {
                    Fail("invalid battle menu image in {}", record.path);
                }
                // PAL moved the weapon status sheet out of btlmenu.img into btlmenu2.img, and its
                // battle menu loads both, so the sheet has to leave btlmenu.img as well: entered
                // twice, it runs the menu's block past the fixed textures in VRAM.
                std::uint32_t count = Le32(data.data() + 4);
                auto          build = [&](bool status) {
                    std::vector<std::uint32_t> picked;
                    for (std::uint32_t i = 0; i < count; i++) {
                        if ((std::memcmp(data.data() + 16 + i * 48, "wepstatus", 10) == 0) == status) {
                            picked.push_back(i);
                        }
                    }
                    std::vector<unsigned char> built(16 + picked.size() * 48);
                    std::memcpy(built.data(), data.data(), 16);
                    Put32(built.data() + 4, static_cast<std::uint32_t>(picked.size()));
                    for (std::size_t n = 0; n < picked.size(); n++) {
                        const unsigned char *entry = data.data() + 16 + picked[n] * 48;
                        std::size_t          offset = Le32(entry + 32);
                        std::size_t          end =
                            picked[n] + 1 < count ? Le32(entry + 48 + 32) : data.size();
                        if (offset < 16 + count * 48 || end > data.size() || end <= offset) {
                            Fail("invalid battle menu image in {}", record.path);
                        }
                        std::memcpy(built.data() + 16 + n * 48, entry, 48);
                        Put32(built.data() + 16 + n * 48 + 32, static_cast<std::uint32_t>(built.size()));
                        built.insert(built.end(), data.begin() + offset, data.begin() + end);
                    }
                    return std::pair(picked.size(), built);
                };
                auto [statuses, single] = build(true);
                if (statuses == 1) {
                    image->data = build(false).second;
                    members.push_back({"btlmenu2.img", std::move(single)});
                    changed = true;
                }
            }
        }
        if (changed) {
            WriteFile(out / "normalized" / record.path, WritePack(members));
        }
    }
}

// Where texel (x, y) of an 8-bit TIM2 picture `width` texels wide lies in its pixel data: the game's
// files keep them in the GS's PSMT8 block order.
inline std::size_t Tim2T8Texel(int x, int y, int width) {
    return (y & ~0xF) * width + (x & ~0xF) * 2 + ((((y & ~3) >> 1) + (y & 1)) & 7) * width * 2 +
           ((x + (((y + 2) >> 2) & 1) * 4) & 7) * 4 + ((y >> 1) & 1) + ((x >> 2) & 2);
}

// The CLUT entry a pixel value names: the GS reads 8-bit CLUTs in CSM1 order, bits 3 and 4 swapped.
inline int Tim2T8Slot(int value) {
    return (value & ~0x18) | ((value & 8) << 1) | ((value & 0x10) >> 1);
}

// The dungeon HUD's floor plates, in itempack.img's itempack picture: the plate the floor number is
// drawn on and the back floors' over its right side. Every release's carry the word for floor, the
// American and Japanese "Floor", and the HUD draws the number below it. The July 12 PAL prototype
// left its British, French, German, Italian and Spanish ones blank (the retail release lettered
// them), so they take American English's, each texel the nearest colour of the sheet's own palette.
inline void FillFloorPlates(const fs::path &out) {
    struct Rect {
        int x, y, width, height;
    };
    constexpr Rect kPlates[] = {
        {0x9A, 0x01, 0x66, 0x29},
        {0xDA, 0x2B, 0x26, 0x29},
    };
    // The picture's pixel and palette offsets in its bank, or none for one not an 8-bit 256 by 192.
    struct Sheet {
        std::vector<unsigned char> bank;
        std::size_t                pixels = 0;
        std::size_t                clut = 0;
        int                        width = 0;
    };
    auto open = [&](const std::string &path, bool prefer_normalized) {
        Sheet    sheet;
        fs::path file = out / "normalized" / path;
        if (!prefer_normalized || !fs::is_regular_file(file)) {
            file = out / path;
        }
        if (!fs::is_regular_file(file)) {
            return sheet;
        }
        sheet.bank = ReadFile(file);
        Im2Picture picture = FindIm2Picture(sheet.bank, "itempack");
        const unsigned char *header = sheet.bank.data() + picture.offset + 16;
        if (picture.size == 0 || header[0x13] != 5 || (header[0x14] | header[0x15] << 8) != 256 ||
            (header[0x16] | header[0x17] << 8) != 192 || (header[0x0E] | header[0x0F] << 8) != 256) {
            return Sheet{};
        }
        sheet.width = 256;
        sheet.pixels = picture.offset + 16 + (header[0x0C] | header[0x0D] << 8);
        sheet.clut = sheet.pixels + Le32(header + 8);
        return sheet;
    };
    Sheet american = open("commenu/a_usa/itempack.img", false);
    if (american.width == 0) {
        return;
    }
    for (const char *language : {"a_eng", "a_fre", "a_ger", "a_ita", "a_spa"}) {
        std::string path = std::format("commenu/{}/itempack.img", language);
        Sheet       sheet = open(path, true);
        if (sheet.width == 0) {
            continue;
        }
        const unsigned char *palette = sheet.bank.data() + sheet.clut;
        for (const Rect &plate : kPlates) {
            for (int y = plate.y; y < plate.y + plate.height; y++) {
                for (int x = plate.x; x < plate.x + plate.width; x++) {
                    int value = american.bank[american.pixels + Tim2T8Texel(x, y, american.width)];
                    const unsigned char *colour = american.bank.data() + american.clut + Tim2T8Slot(value) * 4;
                    int best = -1;
                    int best_distance = 0;
                    for (int slot = 0; slot < 256; slot++) {
                        const unsigned char *entry = palette + slot * 4;
                        if ((colour[3] == 0) != (entry[3] == 0)) {
                            continue;
                        }
                        int distance = colour[3] == 0 ? 0
                                                      : std::abs(entry[0] - colour[0]) + std::abs(entry[1] - colour[1]) +
                                                            std::abs(entry[2] - colour[2]) + 2 * std::abs(entry[3] - colour[3]);
                        if (best < 0 || distance < best_distance) {
                            best = slot;
                            best_distance = distance;
                        }
                    }
                    if (best >= 0) {
                        sheet.bank[sheet.pixels + Tim2T8Texel(x, y, sheet.width)] = static_cast<unsigned char>(Tim2T8Slot(best));
                    }
                }
            }
        }
        fs::path target = out / "normalized" / path;
        if (!fs::is_regular_file(target) || ReadFile(target) != sheet.bank) {
            WriteFile(target, sheet.bank);
        }
    }
}

// The Seda mod (docs/SEDA.md): a player pack for Toan's place built from a cutscene character of the
// disc, with that character's own motions put in the order of Toan's. The game drives a model's
// bones by their index in its scene, and no other character's skeleton is Toan's, so Toan's motions
// cannot move them; their own can.
inline constexpr std::string_view kSedaPack = "dun/mainchara/c01seda.chr";

// A member of the pack whose presence tells the port not to draw the equipped weapon.
inline constexpr std::string_view kSedaHideEquipped = "seda_hide_equipped";

struct SedaKey {
    int         start;
    int         end;
    float       speed;
    const char *what; // Toan's motion the key takes the place of, then the character's that plays.
};

// One key per KEY of Toan's dun/mainchara/c01d.chr, in its order, as frames of King Seda's
// e502p68b.mot (the cutscene of the assassin, near the story's end).
inline constexpr SedaKey kKingSedaKeys[] = {
    {10,  20,  0.1f,  "stand: stand"                            },
    {395, 415, 0.5f,  "run: run with the sword ready"           },
    {40,  60,  0.2f,  "walk: walk"                              },
    {560, 570, 0.2f,  "charged attack loop 2: trembling guard"  },
    {465, 470, 0.3f,  "damage: held fast"                       },
    {560, 560, 0.0f,  "charged attack loop 3: trembling guard"  },
    {465, 481, 0.3f,  "heavy damage: held fast"                 },
    {890, 895, 0.25f, "get up: stand up"                        },
    {350, 352, 0.8f,  "stand to guard: hold the sword"          },
    {350, 360, 0.1f,  "guard loop: hold the sword"              },
    {355, 360, 0.8f,  "guard to stand: hold the sword"          },
    {365, 375, 0.3f,  "attack 1: cut down"                      },
    {575, 625, 1.0f,  "attack 2: run the assassin through"      },
    {535, 545, 0.4f,  "stand to charge: trembling guard"        },
    {560, 570, 0.4f,  "charge loop: trembling guard"            },
    {365, 370, 0.4f,  "charged attack start: cut down"          },
    {370, 370, 0.0f,  "charged attack loop 1: cut down"         },
    {370, 390, 0.4f,  "charged attack end: cut down"            },
    {350, 360, 0.3f,  "attack stance: hold the sword"           },
    {295, 315, 0.4f,  "lock-on right: walk with the sword"      },
    {295, 315, 0.4f,  "lock-on left: walk with the sword"       },
    {295, 315, 0.4f,  "lock-on forward: walk with the sword"    },
    {295, 315, 0.4f,  "lock-on back: walk with the sword"       },
    {770, 780, 0.2f,  "defeated: crouch"                        },
    {670, 700, 0.4f,  "special: push the assassin off"          },
    {390, 390, 0.0f,  "charged attack loop 3: cut down"         },
    {745, 745, 0.0f,  "throw hold: throw the sword away"        },
    {745, 765, 0.4f,  "throw: throw the sword away"             },
    {85,  100, 0.15f, "drink: talk"                             },
    {645, 665, 0.5f,  "run while hurt: run with the sword drawn"},
    {295, 315, 0.3f,  "move while charging: walk with the sword"},
    {825, 835, 0.3f,  "no good: shake the head"                 },
    {495, 501, 0.3f,  "fall: tremble"                           },
    {295, 315, 0.3f,  "move while guarding: walk with the sword"},
    {450, 460, 0.3f,  "atla get in: look up"                    },
    {450, 460, 0.1f,  "atla get loop: look up"                  },
    {365, 375, 0.3f,  "combo 1: cut down"                       },
    {575, 600, 0.6f,  "combo 2: run the assassin through"       },
    {365, 375, 0.3f,  "combo 1 again: cut down"                 },
    {670, 700, 0.6f,  "combo 2 again: push the assassin off"    },
    {420, 445, 0.3f,  "combo 3: fight the assassin"             },
};

// One key per KEY of Toan's, as frames of Seda's c07a.mot (gedit/s99, the dungeon's cutscenes).
inline constexpr SedaKey kSedaC07Keys[] = {
    {10,  20,  0.1f,  "stand: stand"                         },
    {40,  60,  0.45f, "run: walk, quicker"                   },
    {40,  60,  0.25f, "walk: walk"                           },
    {258, 268, 0.2f,  "charged attack loop 2: hold the sword"},
    {142, 150, 0.3f,  "damage: talk to suffering"            },
    {268, 268, 0.0f,  "charged attack loop 3: hold the sword"},
    {142, 161, 0.3f,  "heavy damage: suffering"              },
    {172, 215, 0.4f,  "get up: lean forward, raise the face" },
    {247, 250, 0.6f,  "stand to guard: raise the sword"      },
    {258, 268, 0.1f,  "guard loop: hold the sword"           },
    {247, 250, 0.8f,  "guard to stand: raise the sword"      },
    {269, 285, 0.4f,  "attack 1: sweep"                      },
    {285, 300, 0.4f,  "attack 2: leap at"                    },
    {305, 311, 0.3f,  "stand to charge: magic pose"          },
    {312, 322, 0.3f,  "charge loop: magic pose"              },
    {269, 280, 0.4f,  "charged attack start: sweep"          },
    {280, 280, 0.0f,  "charged attack loop 1: sweep"         },
    {280, 300, 0.4f,  "charged attack end: leap at"          },
    {258, 268, 0.3f,  "attack stance: hold the sword"        },
    {40,  60,  0.3f,  "lock-on right: walk"                  },
    {40,  60,  0.3f,  "lock-on left: walk"                   },
    {40,  60,  0.3f,  "lock-on forward: walk"                },
    {40,  60,  0.3f,  "lock-on back: walk"                   },
    {153, 161, 0.2f,  "defeated: suffering"                  },
    {325, 343, 0.3f,  "special: point the sword"             },
    {300, 300, 0.0f,  "charged attack loop 3: leap at"       },
    {378, 378, 0.0f,  "throw hold: throw"                    },
    {378, 398, 0.4f,  "throw: throw"                         },
    {121, 141, 0.15f, "drink: talk"                          },
    {40,  60,  0.4f,  "run while hurt: walk"                 },
    {40,  60,  0.3f,  "move while charging: walk"            },
    {399, 410, 0.3f,  "no good: look back over the shoulder" },
    {162, 171, 0.3f,  "fall: howl"                           },
    {40,  60,  0.3f,  "move while guarding: walk"            },
    {344, 360, 0.3f,  "atla get in: start to laugh"          },
    {361, 369, 0.2f,  "atla get loop: laugh"                 },
    {269, 285, 0.4f,  "combo 1: sweep"                       },
    {285, 300, 0.4f,  "combo 2: leap at"                     },
    {325, 332, 0.4f,  "combo 1 again: thrust the sword"      },
    {269, 300, 0.5f,  "combo 2 again: sweep and leap"        },
    {269, 300, 0.4f,  "combo 3: sweep and leap"              },
};

// A character a pack can be built from, and how.
struct SedaVariant {
    std::string_view         name;         // As dcdata seda takes it.
    std::string_view         source;       // The character's pack on the disc.
    std::string_view         weapon;       // The object the equipped weapon hangs from, renamed "weapon".
    bool                     weapon_drawn; // Whether that object is still drawn (false: it is the character's own sword).
    std::string_view         item;         // The object items are held by, renamed "item".
    bool                     equipped_drawn; // Whether the equipped weapon is drawn (false: the character shows its own).
    std::span<const SedaKey> keys;
    std::string_view         feet; // FOOT lines for the walk and the run.
};

inline constexpr SedaVariant kSedaVariants[] = {
    // Seda (c07a, //シーダ): his sword is obj10 in his right hand; it stays drawn, and the equipped weapon,
    // at the hand, deals the blows unseen.
    {"seda", "gedit/s99/chara/c07a.chr", "r_hand4", true, "r_hand3", false, kSedaC07Keys, "FOOT 43,53\n"},
    // King Seda (p68b, //シーダ王): his sword is in his left hand and gives way to the equipped one.
    {"king", "gedit/s16/chara/e502p68b.chr", "seiken__m", false, "cube40_3_2", true, kKingSedaKeys,
     "FOOT 43,53\nFOOT 398,408\n"                                                                           },
};

inline const SedaVariant *FindSedaVariant(std::string_view name) {
    for (const SedaVariant &variant : kSedaVariants) {
        if (variant.name == name) {
            return &variant;
        }
    }
    return nullptr;
}

// Renames an object of a scene in place, and with draw false leaves it no model.
inline void RenameSceneObject(std::vector<unsigned char> &scene, std::string_view from, std::string_view to,
                              bool draw) {
    if (scene.size() < 16) {
        Fail("the scene is {} bytes", scene.size());
    }
    std::uint32_t count = Le32(scene.data() + 8);
    if (16 + static_cast<std::uint64_t>(count) * 112 > scene.size()) {
        Fail("the scene claims {} objects in {} bytes", count, scene.size());
    }
    for (std::uint32_t i = 0; i < count; i++) {
        unsigned char *object = scene.data() + 16 + i * 112;
        char          *name = reinterpret_cast<char *>(object + 8);
        if (std::string_view(name, strnlen(name, 32)) == from) {
            std::memset(name, 0, 32);
            std::memcpy(name, to.data(), to.size());
            if (!draw) {
                Put32(object + 40, 0);
            }
            return;
        }
    }
    Fail("the scene has no object {}", from);
}

// Every image a configuration loads (IMG n,"name").
inline std::vector<std::string> ConfigImages(std::string_view config) {
    std::vector<std::string> names;
    for (std::size_t start = 0; start < config.size();) {
        std::size_t end = config.find('\n', start);
        if (end == std::string_view::npos) {
            end = config.size();
        }
        std::string_view line = config.substr(start, end - start);
        start = end + 1;
        std::size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string_view::npos || !line.substr(first).starts_with("IMG") ||
            line.substr(first).starts_with("IMG_END")) {
            continue;
        }
        std::size_t open = line.find('"');
        std::size_t close = open == std::string_view::npos ? open : line.find('"', open + 1);
        if (close != std::string_view::npos) {
            names.emplace_back(line.substr(open + 1, close - open - 1));
        }
    }
    return names;
}

// Looks for a member of that name in a pack and the packs nested in it.
inline bool FindPackMember(std::span<const unsigned char> data, std::string_view name, int depth, PackMember &found) {
    if (data.size() < 76 || !data[0]) {
        return false;
    }
    std::vector<PackMember> members;
    try {
        members = ReadPack(data);
    } catch (const Error &) {
        return false;
    }
    for (PackMember &member : members) {
        if (EqualsFolded(member.name, name)) {
            found = std::move(member);
            return true;
        }
    }
    if (depth > 0) {
        for (const PackMember &member : members) {
            if (FindPackMember(member.data, name, depth - 1, found)) {
                return true;
            }
        }
    }
    return false;
}

// The images a character's configuration loads that its own pack lacks, as the other packs of its
// directory hold them: a cutscene loads its characters' shared images from a pack of its own (as
// gedit/s99/chara/e29img.pak), so without them parts of the character are drawn untextured, black.
inline std::vector<PackMember> MissingImages(const fs::path &directory, const std::vector<PackMember> &members,
                                             std::string_view config, std::FILE *log) {
    std::vector<PackMember> found;
    for (const std::string &name : ConfigImages(config)) {
        bool have = false;
        for (const PackMember &member : members) {
            have = have || EqualsFolded(member.name, name);
        }
        if (have) {
            continue;
        }
        PackMember image;
        bool       located = false;
        std::error_code error;
        for (const fs::directory_entry &entry : fs::directory_iterator(directory, error)) {
            if (!entry.is_regular_file() || entry.file_size() > (64u << 20)) {
                continue;
            }
            if (FindPackMember(ReadFile(entry.path()), name, 2, image)) {
                Log(log, "seda: {} from {}\n", name, entry.path().filename().string());
                located = true;
                break;
            }
        }
        if (located) {
            found.push_back(std::move(image));
        } else {
            Log(log, "seda: {} is in no pack beside it; that part will be untextured\n", name);
        }
    }
    return found;
}

// The names of an IM2 bank's pictures, which are the names textures are found by.
inline std::vector<std::string> Im2PictureNames(std::span<const unsigned char> bank) {
    std::vector<std::string> names;
    if (bank.size() < 16 || std::memcmp(bank.data(), "IM2", 3) != 0) {
        return names;
    }
    std::uint32_t count = Le32(bank.data() + 4);
    if (16 + std::uint64_t(count) * 48 > bank.size()) {
        return names;
    }
    for (std::uint32_t i = 0; i < count; i++) {
        const char *entry = reinterpret_cast<const char *>(bank.data() + 16 + i * 48);
        names.emplace_back(entry, strnlen(entry, 32));
    }
    return names;
}

// One object of a scene that has a model: what it hangs from, its size and the textures it draws with.
struct SceneModel {
    std::string              name;
    std::string              parent;
    std::uint32_t            vertices = 0;
    float                    extent = 0; // The longest side of its box, in the model's own units.
    std::vector<std::string> textures;
};

// The objects of a scene that have models (each model's MDT_HEADER and MDT_MATERIAL table,
// ps2/include/mdt.hpp). The material table has no count: it is read up to the first entry whose name
// is not one, or the model's end.
inline std::vector<SceneModel> SceneModels(std::span<const unsigned char> scene) {
    std::vector<SceneModel> models;
    if (scene.size() < 16) {
        return models;
    }
    std::uint32_t count = Le32(scene.data() + 8);
    if (16 + std::uint64_t(count) * 112 > scene.size()) {
        return models;
    }
    auto name_of = [&](std::int32_t index) {
        if (index < 0 || static_cast<std::uint32_t>(index) >= count) {
            return std::string();
        }
        const char *name = reinterpret_cast<const char *>(scene.data() + 16 + index * 112 + 8);
        return std::string(name, strnlen(name, 32));
    };
    for (std::uint32_t i = 0; i < count; i++) {
        const unsigned char *object = scene.data() + 16 + i * 112;
        std::size_t          at = Le32(object + 40);
        if (at == 0 || at + 64 > scene.size()) {
            continue;
        }
        SceneModel model;
        model.name = name_of(static_cast<std::int32_t>(i));
        model.parent = name_of(static_cast<std::int32_t>(Le32(object + 44)));
        const unsigned char *header = scene.data() + at;
        std::size_t          size = std::min<std::size_t>(Le32(header + 8), scene.size() - at);
        model.vertices = Le32(header + 12);
        std::size_t vertex_at = Le32(header + 16);
        if (vertex_at + std::uint64_t(model.vertices) * 16 <= size) {
            float low[3] = {0, 0, 0};
            float high[3] = {0, 0, 0};
            for (std::uint32_t v = 0; v < model.vertices; v++) {
                for (int axis = 0; axis < 3; axis++) {
                    float value;
                    std::memcpy(&value, header + vertex_at + v * 16 + axis * 4, 4);
                    low[axis] = v == 0 ? value : std::min(low[axis], value);
                    high[axis] = v == 0 ? value : std::max(high[axis], value);
                }
            }
            for (int axis = 0; axis < 3; axis++) {
                model.extent = std::max(model.extent, high[axis] - low[axis]);
            }
        }
        std::size_t materials = Le32(header + 56);
        for (std::size_t m = materials; materials > 0 && m + 96 <= size && m < materials + 64 * 96; m += 96) {
            const char *name = reinterpret_cast<const char *>(header + m + 52);
            std::size_t length = strnlen(name, 44);
            bool        valid = length > 0 && length < 44;
            for (std::size_t c = 0; valid && c < length; c++) {
                valid = name[c] > 0x20 && name[c] < 0x7F;
            }
            if (!valid) {
                break;
            }
            std::string texture(name, length);
            if (std::find(model.textures.begin(), model.textures.end(), texture) == model.textures.end()) {
                model.textures.push_back(texture);
            }
        }
        models.push_back(std::move(model));
    }
    return models;
}

// The textures a scene's models draw with, by name.
inline std::vector<std::string> SceneTextures(std::span<const unsigned char> scene) {
    std::vector<std::string> names;
    for (const SceneModel &model : SceneModels(scene)) {
        for (const std::string &texture : model.textures) {
            if (std::find(names.begin(), names.end(), texture) == names.end()) {
                names.push_back(texture);
            }
        }
    }
    return names;
}

// Leaves an object of a scene without its model, so it is not drawn.
inline void HideSceneObject(std::vector<unsigned char> &scene, std::string_view name) {
    RenameSceneObject(scene, name, name, false);
}

// The scene a configuration's MODEL names (not its SHADOW_MODEL), or empty.
inline std::string ConfigModel(std::string_view config) {
    std::size_t at = config.find("MODEL");
    while (at != std::string_view::npos && at > 0 && config[at - 1] == '_') {
        at = config.find("MODEL", at + 5);
    }
    std::size_t open = at == std::string_view::npos ? at : config.find('"', at);
    std::size_t close = open == std::string_view::npos ? open : config.find('"', open + 1);
    return close == std::string_view::npos ? std::string() : std::string(config.substr(open + 1, close - open - 1));
}

// The text of a pack's first configuration, or empty.
inline std::string PackConfig(const std::vector<PackMember> &members) {
    for (const PackMember &member : members) {
        std::string_view name = member.name;
        if (name.size() > 4 && EqualsFolded(name.substr(name.size() - 4), ".cfg")) {
            const char *text = reinterpret_cast<const char *>(member.data.data());
            return std::string(text, strnlen(text, member.data.size()));
        }
    }
    return std::string();
}

// The images holding the textures a character's model draws with that none of the images its
// configuration loads (nor those found already) hold. The game loads only the images named by IMG
// lines, so a picture in another image of the pack is never loaded; that image is taken first, then
// one from the packs beside it: a cutscene's shared images (its img pack) can hold textures of a
// character's model that its configuration never names. With at most one image: the pack has one
// image slot left.
inline std::vector<PackMember> MissingTextureImages(const fs::path &directory, const std::vector<PackMember> &members,
                                                    const std::vector<PackMember> &found, std::string_view model,
                                                    std::FILE *log) {
    std::vector<std::string> loaded = ConfigImages(PackConfig(members));
    std::vector<std::string> textures;
    std::vector<std::pair<std::string, std::string>> held; // picture, image
    auto hold = [&](const PackMember &member) {
        for (std::string &name : Im2PictureNames(member.data)) {
            held.emplace_back(std::move(name), member.name);
        }
    };
    for (const PackMember &member : members) {
        if (EqualsFolded(member.name, model)) {
            textures = SceneTextures(member.data);
        }
        for (const std::string &name : loaded) {
            if (EqualsFolded(member.name, name)) {
                hold(member);
            }
        }
    }
    for (const PackMember &member : found) {
        hold(member);
    }
    std::string listed;
    for (const std::string &texture : textures) {
        listed += (listed.empty() ? "" : ", ") + texture;
    }
    Log(log, "seda: {} draws with {}\n", model, listed.empty() ? "no texture found" : listed);
    auto holder = [&](const std::string &texture) {
        for (const auto &[picture, image] : held) {
            if (EqualsFolded(picture, texture)) {
                return image;
            }
        }
        return std::string();
    };
    std::vector<PackMember> images;
    for (const std::string &texture : textures) {
        if (std::string image = holder(texture); !image.empty()) {
            Log(log, "seda: texture {} is in {}, which it loads\n", texture, image);
            continue;
        }
        auto take = [&](PackMember candidate, std::string_view where) {
            std::vector<std::string> names = Im2PictureNames(candidate.data);
            if (std::find_if(names.begin(), names.end(), [&](const std::string &name) {
                    return EqualsFolded(name, texture);
                }) == names.end()) {
                return false;
            }
            Log(log, "seda: texture {} from {} in {}\n", texture, candidate.name, where);
            hold(candidate);
            images.push_back(std::move(candidate));
            return true;
        };
        bool located = false;
        for (const PackMember &member : members) {
            if (!located && member.name.size() > 4 &&
                EqualsFolded(std::string_view(member.name).substr(member.name.size() - 4), ".img")) {
                located = take(member, "its own pack, which does not load it");
            }
        }
        std::error_code error;
        for (const fs::directory_entry &entry : fs::directory_iterator(directory, error)) {
            if (located || !entry.is_regular_file() || entry.file_size() > (64u << 20)) {
                continue;
            }
            std::vector<unsigned char> data = ReadFile(entry.path());
            std::vector<PackMember>    candidates;
            try {
                candidates = ReadPack(data);
            } catch (const Error &) {
                continue;
            }
            for (PackMember &candidate : candidates) {
                if (take(std::move(candidate), entry.path().filename().string())) {
                    located = true;
                    break;
                }
            }
        }
        if (!located) {
            Log(log, "seda: texture {} is in no image beside it; what draws with it will be black\n", texture);
        }
    }
    if (images.size() > 1) {
        Log(log, "seda: {} images would be wanted for the textures, but one slot is left; the first is taken\n",
            images.size());
        images.resize(1);
    }
    return images;
}

// The image slot an image the configuration does not name takes: the characters use 0 to 2 of the four.
inline constexpr int kSedaExtraImageSlot = 3;

// The character's configuration with the variant's keys in place of its own, and its foot sounds on
// the walk and the run; everything else, its model, motion, textures and events (on its own frames),
// as it was.
inline std::string SedaConfig(std::string_view config, const SedaVariant &variant, std::string_view extra_image = {}) {
    std::string out;
    bool        in_keys = false;
    bool        image_added = extra_image.empty();
    for (std::size_t start = 0; start < config.size();) {
        std::size_t end = config.find('\n', start);
        if (end == std::string_view::npos) {
            end = config.size();
        }
        std::string_view line = config.substr(start, end - start);
        start = end + 1;
        std::size_t      first = line.find_first_not_of(" \t\r");
        std::string_view word = first == std::string_view::npos ? std::string_view() : line.substr(first);
        if (word.starts_with("KEY_START")) {
            in_keys = true;
            out += "KEY_START 0\n";
            for (std::size_t i = 0; i < variant.keys.size(); i++) {
                const SedaKey &key = variant.keys[i];
                out += std::format("\tKEY\t{},\t{},\t{},\t//{} {}\n", key.start, key.end, key.speed, i, key.what);
            }
            continue;
        }
        if (!image_added && word.starts_with("IMG_END")) {
            out += std::format("IMG {},\"{}\"\n", kSedaExtraImageSlot, extra_image);
            image_added = true;
        }
        if (in_keys) {
            if (word.starts_with("MOTION_END")) {
                in_keys = false;
                out += "MOTION_END\n";
                out += variant.feet;
            }
            continue;
        }
        out += line;
        out += '\n';
    }
    if (in_keys || out.find("MOTION_END") == std::string::npos) {
        Fail("{}'s configuration has no KEY_START ... MOTION_END", variant.source);
    }
    return "// " + std::string(variant.name) + " in Toan's place, made by dcdata from " + std::string(variant.source) +
           "\n" + out;
}

// The pack: the character's members, its scene renamed as the variant says, and its configuration
// rewritten as base.cfg, the name the dungeon loads a player character's by, with the images its
// configuration loads from elsewhere (MissingImages).
inline std::vector<unsigned char> MakeSedaPack(std::span<const unsigned char> source, const SedaVariant &variant,
                                               std::vector<PackMember> extra_images = {},
                                               std::vector<PackMember> texture_images = {},
                                               std::span<const std::string> hidden = {}) {
    std::vector<PackMember> members = ReadPack(source);
    std::string             config = PackConfig(members);
    if (config.empty()) {
        Fail("{} holds no configuration", variant.source);
    }
    std::string model = ConfigModel(config);
    if (model.empty()) {
        Fail("{}'s configuration names no MODEL", variant.source);
    }

    std::vector<PackMember> pack{
        {"base.cfg", {}}
    };
    std::string base = SedaConfig(config, variant, texture_images.empty() ? std::string() : texture_images[0].name);
    // No terminating NUL: the game's configuration reader (ps2/src/chararead.cpp) takes a NUL for
    // a letter, and would read on past the end of the file for the rest of the word.
    pack[0].data.assign(base.begin(), base.end());
    bool found = false;
    for (PackMember &member : members) {
        std::string_view name = member.name;
        if (name.size() > 4 && EqualsFolded(name.substr(name.size() - 4), ".cfg")) {
            continue;
        }
        if (EqualsFolded(name, model)) {
            RenameSceneObject(member.data, variant.weapon, "weapon", variant.weapon_drawn);
            RenameSceneObject(member.data, variant.item, "item", true);
            for (const std::string &name : hidden) {
                HideSceneObject(member.data, name);
            }
            found = true;
        }
        pack.push_back(std::move(member));
    }
    if (!found) {
        Fail("{} holds no {}", variant.source, model);
    }
    for (PackMember &image : extra_images) {
        pack.push_back(std::move(image));
    }
    if (!texture_images.empty() && std::none_of(pack.begin(), pack.end(), [&](const PackMember &member) {
            return EqualsFolded(member.name, texture_images[0].name);
        })) {
        pack.push_back(std::move(texture_images[0]));
    }
    if (!variant.equipped_drawn) {
        pack.push_back({std::string(kSedaHideEquipped), {'1'}});
    }
    return WritePack(pack);
}

// Writes the variant's pack into the extracted data, where the port finds it by its name. A disc
// without the character leaves nothing; the result says which.
inline bool WriteSedaPack(const fs::path &out, std::FILE *log, const SedaVariant &variant = kSedaVariants[0],
                          std::span<const std::string> hidden = {}) {
    fs::path source = out / variant.source;
    if (!fs::is_regular_file(source)) {
        Log(log, "seda: no {}; the Seda mod has no pack\n", variant.source);
        return false;
    }
    std::vector<unsigned char> source_pack = ReadFile(source);
    std::vector<PackMember>    source_members = ReadPack(source_pack);
    std::vector<PackMember>    images =
        MissingImages(source.parent_path(), source_members, PackConfig(source_members), log);
    std::vector<PackMember> texture_images = MissingTextureImages(source.parent_path(), source_members, images,
                                                                  ConfigModel(PackConfig(source_members)), log);
    std::string model_name = ConfigModel(PackConfig(source_members));
    for (const PackMember &member : source_members) {
        if (EqualsFolded(member.name, model_name)) {
            for (const SceneModel &model : SceneModels(member.data)) {
                std::string textures;
                for (const std::string &texture : model.textures) {
                    textures += (textures.empty() ? "" : " ") + texture;
                }
                bool is_hidden = std::find(hidden.begin(), hidden.end(), model.name) != hidden.end();
                Log(log, "seda:   {:<12} on {:<12} {:5} vertices, {:7.1f} long, {}{}\n", model.name, model.parent,
                    model.vertices, model.extent, textures.empty() ? "no texture" : textures,
                    is_hidden ? " (hidden)" : "");
            }
        }
    }
    std::vector<unsigned char> pack =
        MakeSedaPack(source_pack, variant, std::move(images), std::move(texture_images), hidden);
    fs::path                   target = out / "normalized" / kSedaPack;
    if (!fs::is_regular_file(target) || ReadFile(target) != pack) {
        WriteFile(target, pack);
    }
    Log(log, "seda: weapon at {}{}, items at {}; the equipped weapon is {}\n", variant.weapon,
        variant.weapon_drawn ? "" : " (not drawn)", variant.item, variant.equipped_drawn ? "drawn" : "not drawn");
    Log(log, "seda: wrote {} from {} ({} bytes)\n", (fs::path("normalized") / kSedaPack).generic_string(), variant.source,
        pack.size());
    return true;
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
    Normalize(out, records);
    Release release = ReleaseOf(hd2, records);
    Log(log, "release: {}\n", release.name);
    if (release.blank_floor_plates) {
        FillFloorPlates(out);
    }
    WriteLanguages(out, release);
    try {
        WriteSedaPack(out, log);
    } catch (const Error &error) {
        Log(log, "warning: seda: {}\n", error.what());
        summary.warnings++;
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
