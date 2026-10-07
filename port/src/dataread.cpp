#include "dataread.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../tools/dcdata/dcdata.hpp"
#include "localize.hpp"
#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

std::unordered_map<std::string, fs::path> data_index;
bool                                      data_indexed;
BG_READ_INFO                              bg_read_info[32];

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
int ReadWhole(const fs::path &file, void *buffer) {
    std::error_code error;
    std::uintmax_t  size = fs::file_size(file, error);
    std::ifstream   stream(file, std::ios::binary);
    if (error || !stream || size > INT32_MAX) {
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

// Hands a file just read to the localization layer: a message file may be rewritten from the
// language's JSON in place (it fits the whole sectors ReadWhole zeroed), and a pack is noted so
// its message entries can be told from GetPackFile.
void Localized(std::string_view path, void *buffer, int *size) {
    LocalizeLoaded(path, buffer);
    LocalizeFile(path, buffer, dcdata::SectorsFor(static_cast<std::uint64_t>(*size)) * dcdata::kSector, size);
}

} // namespace

PC_OVERRIDE void InitCDFile() {
    const fs::path &root = PathsDataRoot();
    std::error_code error;
    data_index.clear();
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

PC_OVERRIDE int LoadFile(char *path, void *buffer, int *out_size) {
    if (!LoadFile2(path, buffer, out_size, 0)) {
        printf("File open error \"%s\"\n \n \n", path);
        __assert("etc.cpp", 753, "FALSE");
    }

    return 1;
}

PC_OVERRIDE int LoadFile2(char *path, void *buffer, int *out_size, int mode) {
    if (out_size) {
        *out_size = 0;
    }
    const fs::path *file = Lookup(path);
    if (!file) {
        return 0;
    }
    int size = ReadWhole(*file, buffer);
    Localized(path, buffer, &size);
    if (out_size) {
        *out_size = size;
    }
    return 1;
}

PC_OVERRIDE void InitReadBG() {
    for (BG_READ_INFO &info : bg_read_info) {
        info.busy = false;
    }
}

PC_OVERRIDE void StartReadBG() {
    InitReadBG();
}

PC_OVERRIDE void BreakReadBG() {
    InitReadBG();
}

// The read happens here rather than a vertical sync later, so the slot is handed out already
// complete: issued (id) and done, as retail's ReadBG leaves it.
PC_OVERRIDE int LoadFileBG(char *name, u_long128 *buffer, int *out_size) {
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
    int size = ReadWhole(*file, buffer);
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

PC_OVERRIDE BG_READ_INFO *GetReadBGFile(int index) {
    if (index < 0 || index >= 32) {
        return 0;
    }
    return bg_read_info[index].busy ? &bg_read_info[index] : 0;
}

PC_OVERRIDE void ReadBG() {
}

PC_OVERRIDE int ReadBGSync() {
    for (const BG_READ_INFO &info : bg_read_info) {
        if (info.busy && (info.id == 0 || info.done == 0)) {
            return 1;
        }
    }
    return 0;
}

// Only development builds wrote files, to the host PC's drive; the device and any drive letter
// after it are dropped so the path lands under save/host0/.
PC_OVERRIDE int WriteFile(char *path, void *buffer, int size) {
    std::string_view name = path;
    if (std::size_t colon = name.rfind(':'); colon != std::string_view::npos) {
        name = name.substr(colon + 1);
    }
    std::string relative(name);
    std::replace(relative.begin(), relative.end(), '\\', '/');
    relative.erase(0, relative.find_first_not_of('/'));
    if (!dcdata::IsSafeRelative(relative) || size < 0) {
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
PC_OVERRIDE u_int *GetPackFile(u_int *pack, char *name, int *out_size) {
    if (!pack) {
        return 0;
    }
    std::string_view base = name;
    if (std::size_t slash = base.rfind('/'); slash != std::string_view::npos) {
        base = base.substr(slash + 1);
    }
    for (PackEntry *entry = reinterpret_cast<PackEntry *>(pack); entry->name[0];
         entry = reinterpret_cast<PackEntry *>(reinterpret_cast<char *>(entry) + entry->next)) {
        if (SameName(entry->name, base)) {
            u_int *data = reinterpret_cast<u_int *>(reinterpret_cast<char *>(entry) + entry->offset);
            int    size = entry->size;
            data = const_cast<u_int *>(LocalizePack(pack, base, data, &size));
            if (out_size) {
                *out_size = size;
            }
            return data;
        }
    }
    return 0;
}
