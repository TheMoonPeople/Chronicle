#include <libmc.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../platform/paths.hpp"

// Each card is a directory, <save root>/mc0 and mc1, and every command
// finishes inside the call that issues it. The game still sees libmc's
// protocol: a call returns 0 when it accepts a command, and the next
// sceMcSync reports the command's function number and its result. Like the
// IOP's single command slot, a new command replaces a result nobody synced.

namespace {

namespace fs = std::filesystem;

constexpr int kPortCount = 2;
constexpr int kMaxFiles = 16;

// What a freshly formatted 8 MB card reports free, in kilobyte clusters.
constexpr int kFreeClusters = 8000;

constexpr int kResultNoCard = -10;

enum McFunction {
    kFuncGetInfo = 0x01,
    kFuncOpen = 0x02,
    kFuncClose = 0x03,
    kFuncRead = 0x05,
    kFuncWrite = 0x06,
    kFuncFlush = 0x0A,
    kFuncMkdir = 0x0B,
    kFuncChdir = 0x0C,
    kFuncGetDir = 0x0D,
    kFuncDelete = 0x0F,
    kFuncFormat = 0x10,
    kFuncUnformat = 0x11,
};

enum McOpenFlag {
    kOpenRead = 0x0001,
    kOpenWrite = 0x0002,
    kOpenCreate = 0x0200,
};

enum McAttribute : std::uint16_t {
    kAttrReadable = 0x0001,
    kAttrWriteable = 0x0002,
    kAttrExecutable = 0x0004,
    kAttrFile = 0x0010,
    kAttrSubdir = 0x0020,
    kAttrClosed = 0x0080,
    kAttrExists = 0x8000,
};

struct McDateTime {
    std::uint8_t  reserved;
    std::uint8_t  second;
    std::uint8_t  minute;
    std::uint8_t  hour;
    std::uint8_t  day;
    std::uint8_t  month;
    std::uint16_t year;
};

struct McDirEntry {
    McDateTime    created;
    McDateTime    modified;
    std::uint32_t size;
    std::uint16_t attributes;
    std::uint16_t reserved1;
    std::uint32_t reserved2;
    std::uint32_t pda_application;
    char          name[32];
};

static_assert(sizeof(McDirEntry) == 0x40);

struct Card {
    std::string cwd = "/";
    bool        unformatted = false;
    std::size_t listed = 0;
};

struct OpenFile {
    std::FILE *file = nullptr;
    int        flags = 0;
};

struct Pending {
    bool active = false;
    int  function;
    int  result;
};

std::array<Card, kPortCount>    g_cards;
std::array<OpenFile, kMaxFiles> g_files;
Pending                         g_pending;

int Finish(int function, int result) {
    g_pending = {true, function, result};
    return 0;
}

bool ValidPort(int port) {
    return port >= 0 && port < kPortCount;
}

fs::path CardRoot(int port) {
    return PathsSaveRoot() / ("mc" + std::to_string(port));
}

// The first card is always inserted; the second only once its directory exists.
bool CardPresent(int port) {
    std::error_code error;
    return ValidPort(port) && (port == 0 || fs::is_directory(CardRoot(port), error));
}

// The card's status for a command that needs a formatted card: 0, or the
// result the command fails with.
int CardStatus(int port) {
    if (!CardPresent(port)) {
        return kResultNoCard;
    }
    if (g_cards[port].unformatted) {
        return sceMcResNoFormat;
    }
    std::error_code error;
    fs::create_directories(CardRoot(port), error);
    return 0;
}

// Resolves a card path against the port's current directory into the
// components below the card's root; ".." stops at the root.
std::vector<std::string> Components(int port, std::string_view name) {
    std::vector<std::string> parts;
    std::string              path = name.starts_with('/') ? std::string(name) : g_cards[port].cwd + "/" + std::string(name);
    std::size_t              start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find('/', start);
        std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

#ifdef _WIN32
// Win32 opens these names as devices in any directory, with or without an extension.
bool ReservedDevice(const std::string &part) {
    std::wstring stem = fs::path(part).native();
    stem = stem.substr(0, stem.find(L'.'));
    while (!stem.empty() && stem.back() == L' ') {
        stem.pop_back();
    }
    for (wchar_t &c : stem) {
        if (c >= L'a' && c <= L'z') {
            c -= L'a' - L'A';
        }
    }
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" || stem == L"CONIN$" ||
        stem == L"CONOUT$" || stem == L"CLOCK$") {
        return true;
    }
    return stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
           ((stem[3] >= L'1' && stem[3] <= L'9') || stem[3] == L'¹' || stem[3] == L'²' ||
            stem[3] == L'³');
}
#endif

fs::path HostPath(int port, const std::vector<std::string> &parts) {
    fs::path path = CardRoot(port);
    for (const std::string &part : parts) {
#ifdef _WIN32
        if (part.find_first_of("\\:") != std::string::npos || part.back() == '.' || part.back() == ' ' ||
            ReservedDevice(part)) {
            return {};
        }
#endif
        path /= part;
    }
    return path;
}

std::string CardPath(const std::vector<std::string> &parts) {
    std::string path;
    for (const std::string &part : parts) {
        path += "/" + part;
    }
    return path.empty() ? "/" : path;
}

OpenFile *FileFor(int fd) {
    if (fd < 0 || fd >= kMaxFiles || g_files[fd].file == nullptr) {
        return nullptr;
    }
    return &g_files[fd];
}

bool Matches(const char *pattern, const char *name) {
    if (*pattern == '\0') {
        return *name == '\0';
    }
    if (*pattern == '*') {
        return Matches(pattern + 1, name) || (*name != '\0' && Matches(pattern, name + 1));
    }
    return *name != '\0' && (*pattern == '?' || *pattern == *name) && Matches(pattern + 1, name + 1);
}

McDateTime DateTime(const fs::path &path) {
    std::error_code error;
    auto            written = fs::last_write_time(path, error);
    std::time_t     seconds = error ? std::time(nullptr)
                                    : static_cast<std::time_t>(std::chrono::duration_cast<std::chrono::seconds>(fs::file_time_type::clock::to_sys(written).time_since_epoch()).count());
    std::tm         utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    return {0,
            static_cast<std::uint8_t>(utc.tm_sec),
            static_cast<std::uint8_t>(utc.tm_min),
            static_cast<std::uint8_t>(utc.tm_hour),
            static_cast<std::uint8_t>(utc.tm_mday),
            static_cast<std::uint8_t>(utc.tm_mon + 1),
            static_cast<std::uint16_t>(utc.tm_year + 1900)};
}

McDirEntry Entry(const fs::path &path, const std::string &name, bool directory) {
    McDirEntry entry{};
    entry.created = DateTime(path);
    entry.modified = entry.created;
    std::error_code error;
    entry.size = directory ? 0 : static_cast<std::uint32_t>(fs::file_size(path, error));
    entry.attributes = kAttrExists | kAttrReadable | kAttrWriteable | kAttrExecutable | (directory ? kAttrSubdir : kAttrFile | kAttrClosed);
    std::strncpy(entry.name, name.c_str(), sizeof(entry.name) - 1);
    return entry;
}

} // namespace

int sceMcInit() {
    g_pending = {};
    return sceMcIniSucceed;
}

int sceMcSync(int mode, int *cmd, int *result) {
    if (!g_pending.active) {
        return -1;
    }
    if (cmd != nullptr) {
        *cmd = g_pending.function;
    }
    if (result != nullptr) {
        *result = g_pending.result;
    }
    g_pending.active = false;
    return 1;
}

int sceMcGetInfo(int port, int slot, int *type, int *free_size, int *formatted) {
    if (!ValidPort(port)) {
        return -1;
    }
    bool present = CardPresent(port);
    *type = present ? sceMcTypePS2 : sceMcTypeNoCard;
    *free_size = present ? kFreeClusters : 0;
    *formatted = present && !g_cards[port].unformatted;
    if (!present) {
        return Finish(kFuncGetInfo, kResultNoCard);
    }
    return Finish(kFuncGetInfo, g_cards[port].unformatted ? sceMcResNoFormat : sceMcResSucceed);
}

int sceMcOpen(int port, int slot, char *name, int flag) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (int status = CardStatus(port); status != 0) {
        return Finish(kFuncOpen, status);
    }
    int fd = 0;
    while (fd < kMaxFiles && g_files[fd].file != nullptr) {
        ++fd;
    }
    if (fd == kMaxFiles) {
        return Finish(kFuncOpen, sceMcResDeniedPermit);
    }
    fs::path        path = HostPath(port, Components(port, name));
    std::error_code error;
#ifdef _WIN32
    if (path.empty()) {
        return Finish(kFuncOpen, sceMcResNoEntry);
    }
#endif
    if (fs::is_directory(path, error)) {
        return Finish(kFuncOpen, sceMcResNoEntry);
    }
    bool       exists = fs::exists(path, error);
    std::FILE *file = nullptr;
    if (exists) {
#ifdef _WIN32
        file = ::_wfopen(path.c_str(), (flag & kOpenWrite) != 0 ? L"r+b" : L"rb");
#else
        file = std::fopen(path.c_str(), (flag & kOpenWrite) != 0 ? "r+b" : "rb");
#endif
    } else if ((flag & kOpenCreate) != 0 && fs::is_directory(path.parent_path(), error)) {
#ifdef _WIN32
        file = ::_wfopen(path.c_str(), L"w+b");
#else
        file = std::fopen(path.c_str(), "w+b");
#endif
    }
    if (file == nullptr) {
        return Finish(kFuncOpen, sceMcResNoEntry);
    }
    g_files[fd] = {file, flag};
    return Finish(kFuncOpen, fd);
}

int sceMcClose(int fd) {
    OpenFile *open = FileFor(fd);
    if (open == nullptr) {
        return Finish(kFuncClose, sceMcResNoEntry);
    }
    std::fclose(open->file);
    *open = {};
    return Finish(kFuncClose, sceMcResSucceed);
}

int sceMcRead(int fd, void *buffer, int size) {
    OpenFile *open = FileFor(fd);
    if (open == nullptr) {
        return Finish(kFuncRead, sceMcResNoEntry);
    }
    if ((open->flags & kOpenRead) == 0) {
        return Finish(kFuncRead, sceMcResDeniedPermit);
    }
    std::fseek(open->file, 0, SEEK_CUR);
    return Finish(kFuncRead, static_cast<int>(std::fread(buffer, 1, size, open->file)));
}

int sceMcWrite(int fd, void *buffer, int size) {
    OpenFile *open = FileFor(fd);
    if (open == nullptr) {
        return Finish(kFuncWrite, sceMcResNoEntry);
    }
    if ((open->flags & kOpenWrite) == 0) {
        return Finish(kFuncWrite, sceMcResDeniedPermit);
    }
    std::fseek(open->file, 0, SEEK_CUR);
    return Finish(kFuncWrite, static_cast<int>(std::fwrite(buffer, 1, size, open->file)));
}

int sceMcFlush(int fd) {
    OpenFile *open = FileFor(fd);
    if (open == nullptr) {
        return Finish(kFuncFlush, sceMcResNoEntry);
    }
    return Finish(kFuncFlush, std::fflush(open->file) == 0 ? sceMcResSucceed : sceMcResFailReplace);
}

int sceMcChdir(int port, int slot, char *name, char *current) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (int status = CardStatus(port); status != 0) {
        return Finish(kFuncChdir, status);
    }
    std::vector<std::string> parts = Components(port, name);
    std::error_code          error;
    fs::path                 path = HostPath(port, parts);
#ifdef _WIN32
    if (path.empty()) {
        return Finish(kFuncChdir, sceMcResNoEntry);
    }
#endif
    if (!fs::is_directory(path, error)) {
        return Finish(kFuncChdir, sceMcResNoEntry);
    }
    if (current != nullptr) {
        std::strcpy(current, g_cards[port].cwd.c_str());
    }
    g_cards[port].cwd = CardPath(parts);
    return Finish(kFuncChdir, sceMcResSucceed);
}

int sceMcMkdir(int port, int slot, char *name) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (int status = CardStatus(port); status != 0) {
        return Finish(kFuncMkdir, status);
    }
    std::vector<std::string> parts = Components(port, name);
    fs::path                 path = HostPath(port, parts);
    std::error_code          error;
#ifdef _WIN32
    if (path.empty()) {
        return Finish(kFuncMkdir, sceMcResNoEntry);
    }
#endif
    if (parts.empty() || fs::exists(path, error) || !fs::is_directory(path.parent_path(), error)) {
        return Finish(kFuncMkdir, sceMcResNoEntry);
    }
    if (!fs::create_directory(path, error)) {
        return Finish(kFuncMkdir, sceMcResFullDevice);
    }
    return Finish(kFuncMkdir, sceMcResSucceed);
}

int sceMcDelete(int port, int slot, char *name) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (int status = CardStatus(port); status != 0) {
        return Finish(kFuncDelete, status);
    }
    std::vector<std::string> parts = Components(port, name);
    std::error_code          error;
    fs::path                 path = HostPath(port, parts);
#ifdef _WIN32
    if (path.empty()) {
        return Finish(kFuncDelete, sceMcResNoEntry);
    }
#endif
    if (parts.empty() || !fs::remove(path, error)) {
        return Finish(kFuncDelete, error ? sceMcResDeniedPermit : sceMcResNoEntry);
    }
    return Finish(kFuncDelete, sceMcResSucceed);
}

int sceMcGetDir(int port, int slot, char *name, unsigned int mode, int count, void *table) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (int status = CardStatus(port); status != 0) {
        return Finish(kFuncGetDir, status);
    }
    std::string_view         spec(name);
    std::size_t              slash = spec.rfind('/');
    std::string              pattern(slash == std::string_view::npos ? spec : spec.substr(slash + 1));
    std::vector<std::string> parts = Components(port, slash == std::string_view::npos ? "." : std::string(spec.substr(0, slash + 1)));
    fs::path                 dir = HostPath(port, parts);
    std::error_code          error;
#ifdef _WIN32
    if (dir.empty() || pattern.find_first_of("\\:") != std::string::npos) {
        return Finish(kFuncGetDir, sceMcResNoEntry);
    }
#endif
    if (!fs::is_directory(dir, error)) {
        return Finish(kFuncGetDir, sceMcResNoEntry);
    }

    std::vector<McDirEntry> entries;
    if (!parts.empty()) {
        for (const char *special : {".", ".."}) {
            if (Matches(pattern.c_str(), special)) {
                entries.push_back(Entry(dir, special, true));
            }
        }
    }
    std::vector<fs::directory_entry> found;
    for (const fs::directory_entry &entry : fs::directory_iterator(dir, error)) {
#ifdef _WIN32
        if (Matches(pattern.c_str(), entry.path().filename().string().c_str())) {
#else
        if (Matches(pattern.c_str(), entry.path().filename().c_str())) {
#endif
            found.push_back(entry);
        }
    }
    std::ranges::sort(found, {}, [](const fs::directory_entry &entry) { return entry.path().filename().string(); });
    for (const fs::directory_entry &entry : found) {
        entries.push_back(Entry(entry.path(), entry.path().filename().string(), entry.is_directory(error)));
    }

    Card &card = g_cards[port];
    if (mode == 0) {
        card.listed = 0;
    }
    int written = 0;
    for (; card.listed < entries.size() && written < count; ++card.listed, ++written) {
        std::memcpy(static_cast<McDirEntry *>(table) + written, &entries[card.listed], sizeof(McDirEntry));
    }
    return Finish(kFuncGetDir, written);
}

// Formatting and unformatting only change what the card reports: the port
// never erases a save directory on the game's say-so.
int sceMcFormat(int port, int slot) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (!CardPresent(port)) {
        return Finish(kFuncFormat, kResultNoCard);
    }
    g_cards[port].unformatted = false;
    g_cards[port].cwd = "/";
    return Finish(kFuncFormat, sceMcResSucceed);
}

int sceMcUnformat(int port, int slot) {
    if (!ValidPort(port)) {
        return -1;
    }
    if (!CardPresent(port)) {
        return Finish(kFuncUnformat, kResultNoCard);
    }
    g_cards[port].unformatted = true;
    g_cards[port].cwd = "/";
    return Finish(kFuncUnformat, sceMcResSucceed);
}
