#include "common.h"
#include "dataread.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include "../../../tools/dcdata/dcdata.hpp"
#include "data/data_read.hpp"
#include "localize/localize.hpp"
#include "platform/load_work_profile.hpp"
#include "platform/paths.hpp"

namespace dc::game {

using namespace dc::data;
using namespace dc::localize;

namespace fs = std::filesystem;

namespace {

u_int *packfile_buff;

std::array<BG_READ_INFO, 32> bg_read_info;

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

// The member's offset from the start of the pack.
std::size_t PackEntryOffset(const u_int *pack, const PackEntry *entry) {
    const auto *pack_bytes = reinterpret_cast<const std::uint8_t *>(pack);
    const auto *entry_bytes = reinterpret_cast<const std::uint8_t *>(entry);
    return static_cast<std::size_t>(entry_bytes - pack_bytes) + static_cast<std::size_t>(entry->offset);
}

// Hands a file just read to the localization layer: a message file may be rewritten from the
// language's JSON in place (it fits the whole sectors DataReadLoadFile zeroed), and a pack is noted
// so its message entries can be told from GetPackFile.
void Localized(std::string_view path, void *buffer, int *size) {
    LocalizeLoaded(path, buffer);
    LocalizeFile(path, buffer, dcdata::SectorsFor(static_cast<std::uint64_t>(*size)) * dcdata::kSector, size);
}

} // namespace

void InitReadBG() {
    for (BG_READ_INFO &info : bg_read_info) {
        info.busy = false;
    }
}

// The read happens here rather than a vertical sync later, so the slot is handed out already
// complete: issued (id) and done, as retail's ReadBG leaves it.
int LoadFileBG(char *name, u_long128 *buffer, int *out_size) {
    const dc::platform::load_profile::Span profile(dc::platform::load_profile::Kind::BackgroundRead);
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
    int size = 0;
    if (!DataReadLoadFile(name, buffer, &size)) {
        return 0;
    }
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

void StartReadBG() {
    InitReadBG();
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

void BreakReadBG() {
    InitReadBG();
}

void InitCDFile() {
    DataReadIndex();
}

void InitMemoryFile() {
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
    int size = 0;
    if (!DataReadLoadFile(path, buffer, &size)) {
        return 0;
    }
    Localized(path, buffer, &size);
    if (out_size) {
        *out_size = size;
    }
    return 1;
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

int LoadPackFile(char *path, u_int *buffer, int *out_size) {
    packfile_buff = buffer;

    if (!LoadFile2(path, buffer, out_size, 0)) {
        packfile_buff = nullptr;
        return 0;
    }

    return 1;
}

u_int *GetPackFile(char *name, int *out_size) {
    return GetPackFile(packfile_buff, name, out_size);
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
    std::size_t capacity = 0;
    if (!DataReadPackCapacity(pack, capacity)) {
        return nullptr;
    }
    std::string_view basename = name;
    if (std::size_t slash = basename.rfind('/'); slash != std::string_view::npos) {
        basename = basename.substr(slash + 1);
    }
    for (int index = 0;; index++) {
        PackEntry *entry = PackEntryAt(pack, capacity, index);
        if (entry == nullptr) {
            return nullptr;
        }
        if (SameName(entry->name, basename)) {
            u_int *data = PackEntryData(pack, capacity, entry);
            if (data == nullptr) {
                return nullptr;
            }
            int size = entry->size;
            data = const_cast<u_int *>(LocalizePack(pack, basename, data, &size));
            DataReadTrackPackMember(pack, data, static_cast<std::size_t>(size), PackEntryOffset(pack, entry), entry->name,
                                    static_cast<std::size_t>(entry->size), index, PackNameDuplicated(pack, capacity, entry));
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
    std::size_t capacity = 0;
    if (!DataReadPackCapacity(pack, capacity)) {
        return nullptr;
    }
    PackEntry *entry = PackEntryAt(pack, capacity, index);
    if (entry == nullptr) {
        return nullptr;
    }
    u_int *data = PackEntryData(pack, capacity, entry);
    if (data == nullptr) {
        return nullptr;
    }
    int size = entry->size;
    data = const_cast<u_int *>(LocalizePack(pack, entry->name, data, &size));
    DataReadTrackPackMember(pack, data, static_cast<std::size_t>(size), PackEntryOffset(pack, entry), entry->name,
                            static_cast<std::size_t>(entry->size), index, PackNameDuplicated(pack, capacity, entry));
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

} // namespace dc::game
