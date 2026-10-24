#include "dataread.hpp"
#include "language.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../../tools/dcdata/dcdata.hpp"
#include "dataread_port.hpp"
#include "localize.hpp"
#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

std::unordered_map<std::string, fs::path> data_index;

struct PackCapacity {
    std::size_t capacity;
    const void *owner;
};

std::unordered_map<const void *, PackCapacity> loaded_capacities;
bool                                          data_indexed;
BG_READ_INFO                                  bg_read_info[32];

void ForgetPackCapacities(const void *owner) {
    for (auto at = loaded_capacities.begin(); at != loaded_capacities.end();) {
        if (at->second.owner == owner) {
            at = loaded_capacities.erase(at);
        } else {
            ++at;
        }
    }
}

void TrackPackCapacity(const void *pack, std::size_t capacity, const void *owner) {
    if (pack != nullptr) {
        loaded_capacities[pack] = {capacity, owner};
    }
}

// Everything up to the first colon is a device, as retail's LoadFile2 has it; sim: and host:
// were development devices that reached the same tree, so they are dropped like cdrom0:.
std::string_view StripDevice(std::string_view path) {
    std::size_t colon = path.find(':');
    return colon == std::string_view::npos ? path : path.substr(colon + 1);
}

[[noreturn]] void NoData(const fs::path &root, const char *why) {
    std::fprintf(stderr,
                 "no game data: %s %s; extract the disc with `dcdata extract <disc image> %s` "
                 "or pass --data <dir>\n",
                 PathsDisplay(root).c_str(), why, PathsDisplay(root).c_str());
    std::abort();
}

void CheckAgainstIndex(const fs::path &root) {
    auto found = data_index.find("data.hd2");
    if (found == data_index.end()) {
        return;
    }
    try {
        dcdata::Extent              extent{found->second, 0, fs::file_size(found->second)};
        std::vector<unsigned char>  hd2 = dcdata::ReadExtent(extent);
        std::vector<dcdata::Record> records = dcdata::ParseIndex(hd2);
        std::size_t                 bad = dcdata::Mismatched(records, root).size();
        if (bad != 0) {
            std::fprintf(stderr,
                         "%s: %zu of the %zu files data.hd2 lists are missing or the wrong size; "
                         "run dcdata extract again\n",
                         PathsDisplay(root).c_str(), bad, records.size());
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s: %s\n", PathsDisplay(root).c_str(), error.what());
    }
}

const fs::path *Lookup(std::string_view path) {
    if (!data_indexed) {
        InitCDFile();
    }
    auto found = data_index.find(dcdata::FoldPath(StripDevice(path)));
    return found == data_index.end() ? nullptr : &found->second;
}

// The drive wrote whole sectors, so up to 2047 bytes past the file were overwritten and callers
// sized their buffers for that; the tail is zeroed rather than left as whatever was there.
int ReadWhole(const fs::path &file, void *buffer, std::size_t *out_capacity = nullptr) {
    std::error_code error;
    std::uintmax_t  size = fs::file_size(file, error);
    std::ifstream   stream(file, std::ios::binary);
    if (error || !stream || size > INT32_MAX || buffer == nullptr) {
        std::fprintf(stderr, "cannot read %s\n", PathsDisplay(file).c_str());
        std::abort();
    }
    stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
    if (static_cast<std::uintmax_t>(stream.gcount()) != size) {
        std::fprintf(stderr, "short read on %s\n", PathsDisplay(file).c_str());
        std::abort();
    }
    std::size_t padded = dcdata::SectorsFor(size) * dcdata::kSector;
    std::memset(static_cast<char *>(buffer) + size, 0, padded - size);
    if (out_capacity != nullptr) {
        *out_capacity = padded;
    }
    return static_cast<int>(size);
}

// One file inside a pack, as ps2/src/dataread.cpp has it: offsets are bytes from the entry, and a
// first byte of zero ends the table.
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
            entry->next < static_cast<int>(sizeof(PackEntry)) || static_cast<std::size_t>(entry->next) > capacity - offset) {
            return nullptr;
        }
        if (index == wanted) {
            return entry;
        }
        offset += static_cast<std::size_t>(entry->next);
    }
    return nullptr;
}

u_int *PackEntryData(u_int *pack, std::size_t capacity, PackEntry *entry) {
    auto             *bytes = reinterpret_cast<unsigned char *>(pack);
    const std::size_t offset = static_cast<std::size_t>(reinterpret_cast<unsigned char *>(entry) - bytes);
    if (entry->offset < static_cast<int>(sizeof(PackEntry)) || entry->size < 0 ||
        static_cast<std::size_t>(entry->offset) > capacity - offset ||
        static_cast<std::size_t>(entry->size) > capacity - offset - static_cast<std::size_t>(entry->offset)) {
        return nullptr;
    }
    return reinterpret_cast<u_int *>(reinterpret_cast<unsigned char *>(entry) + entry->offset);
}

// Hands a file just read to the localization layer: a message file may be rewritten from the
// language's JSON in place (it fits the whole sectors ReadWhole zeroed), and a pack is noted so
// its message entries can be told from GetPackFile.
void Localized(std::string_view path, void *buffer, int *size) {
    LocalizeLoaded(path, buffer);
    LocalizeFile(path, buffer, dcdata::SectorsFor(static_cast<std::uint64_t>(*size)) * dcdata::kSector, size);
}

} // namespace

PORT_OVERRIDE void InitCDFile() {
    const fs::path &root = PathsDataRoot();
    std::error_code error;
    data_index.clear();
    loaded_capacities.clear();
    data_indexed = false;
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
            data_index.emplace(dcdata::FoldPath(relative), it->path());
        }
    }
    fs::path normalized = root / "normalized";
    if (fs::is_directory(normalized)) {
        for (const fs::directory_entry &entry : fs::recursive_directory_iterator(normalized)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().lexically_relative(normalized).generic_string();
                data_index[dcdata::FoldPath(path)] = entry.path();
            }
        }
    }
    if (error) {
        std::fprintf(stderr, "%s: %s\n", PathsDisplay(root).c_str(), error.message().c_str());
    }
    if (data_index.empty()) {
        NoData(root, "is empty");
    }
    data_indexed = true;
    CheckAgainstIndex(root);
}

PORT_OVERRIDE int LoadFile(char *path, void *buffer, int *out_size) {
    if (!LoadFile2(path, buffer, out_size, 0)) {
        printf("File open error \"%s\"\n \n \n", path);
        __assert("etc.cpp", 753, "FALSE");
    }

    return 1;
}

PORT_OVERRIDE int LoadFile2(char *path, void *buffer, int *out_size, int mode) {
    if (out_size) {
        *out_size = 0;
    }
    const fs::path *file = Lookup(path);
    if (!file) {
        return 0;
    }
    ForgetPackCapacities(buffer);
    std::size_t capacity = 0;
    int         size = ReadWhole(*file, buffer, &capacity);
    TrackPackCapacity(buffer, capacity, buffer);
    Localized(path, buffer, &size);
    if (out_size) {
        *out_size = size;
    }
    return 1;
}

// languages.json is {"release": "...", "languages": [n, ...]}; the numbers in the brackets after
// "languages" are the languages.
std::uintmax_t DataFileSize(std::string_view path) {
    const fs::path      *file = Lookup(path);
    std::error_code      error;
    const std::uintmax_t size = file != nullptr ? fs::file_size(*file, error) : 0;
    return error ? 0 : size;
}

void DataReadClearPackCapacities() {
    loaded_capacities.clear();
}

std::vector<s32> DataLanguages() {
    std::vector<s32> languages;
    const fs::path  *file = Lookup(dcdata::kLanguagesFile);
    if (!file) {
        return languages;
    }
    std::ifstream stream(*file);
    std::string   text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    std::size_t   key = text.find("\"languages\"");
    std::size_t   open = key == std::string::npos ? key : text.find('[', key);
    std::size_t   close = open == std::string::npos ? open : text.find(']', open);
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

PORT_OVERRIDE void InitReadBG() {
    for (BG_READ_INFO &info : bg_read_info) {
        info.busy = false;
    }
}

PORT_OVERRIDE void StartReadBG() {
    InitReadBG();
}

PORT_OVERRIDE void BreakReadBG() {
    InitReadBG();
}

// The read happens here rather than a vertical sync later, so the slot is handed out already
// complete: issued (id) and done, as retail's ReadBG leaves it.
PORT_OVERRIDE int LoadFileBG(char *name, u_long128 *buffer, int *out_size) {
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
    if (reinterpret_cast<std::uintptr_t>(buffer) % 64) {
        printf("/*/*/*/*/not 64byte align at %p %s\n", static_cast<void *>(buffer), name);
    }
    const fs::path *file = Lookup(name);
    if (!file) {
        return 0;
    }
    ForgetPackCapacities(buffer);
    std::size_t capacity = 0;
    int         size = ReadWhole(*file, buffer, &capacity);
    TrackPackCapacity(buffer, capacity, buffer);
    Localized(name, buffer, &size);
    std::snprintf(info->name, sizeof info->name, "%s", name);
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

PORT_OVERRIDE BG_READ_INFO *GetReadBGFile(int index) {
    if (index < 0 || index >= 32) {
        return nullptr;
    }
    return bg_read_info[index].busy ? &bg_read_info[index] : nullptr;
}

PORT_OVERRIDE void ReadBG() {
}

PORT_OVERRIDE int ReadBGSync() {
    for (const BG_READ_INFO &info : bg_read_info) {
        if (info.busy && (info.id == 0 || info.done == 0)) {
            return 1;
        }
    }
    return 0;
}

// Only development builds wrote files, to the host PC's drive; the device and any drive letter
// after it are dropped so the path lands under save/host0/.
PORT_OVERRIDE int WriteFile(char *path, void *buffer, int size) {
    std::string_view name = path;
    if (std::size_t colon = name.rfind(':'); colon != std::string_view::npos) {
        name = name.substr(colon + 1);
    }
    std::string relative(name);
    std::replace(relative.begin(), relative.end(), '\\', '/');
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
PORT_OVERRIDE u_int *GetPackFile(u_int *pack, char *name, int *out_size) {
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
            TrackPackCapacity(data, static_cast<std::size_t>(size), capacity->second.owner);
            if (out_size) {
                *out_size = size;
            }
            return data;
        }
    }
}

PORT_OVERRIDE u_int *GetPackFile(u_int *pack, int index, char **out_name, int *out_size) {
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
    TrackPackCapacity(data, static_cast<std::size_t>(size), capacity->second.owner);
    if (out_name != nullptr) {
        *out_name = entry->name;
    }
    if (out_size != nullptr) {
        *out_size = size;
    }
    return data;
}

PORT_OVERRIDE int GetPackFileExt(u_int *pack, char *extension, u_int **files, int max_files, int *sizes, char **names) {
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
