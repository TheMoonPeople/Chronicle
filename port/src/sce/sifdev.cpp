#include <fcntl.h>
#include <sifdev.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

#include "platform/paths.hpp"

// The host file server the development kit reached through host0:, on save/host0/. Every device
// lands there: edit.cpp's host0:debug.txt dump is the one caller left (mwBload's overlay reads
// and InitCDFile's cdrom0: index read are replaced). Errors are negative errno values, as the
// IOP's file manager returned them.

namespace fs = std::filesystem;

namespace {

constexpr int kAccessMask = 0x0003;
constexpr int kReadWrite = 0x0003;
constexpr int kAppend = 0x0100;
constexpr int kExclusive = 0x0800;

std::unordered_set<int> g_open;

bool HostPath(const char *name, fs::path &out) {
    if (name == nullptr) {
        return false;
    }
    std::string_view path = name;
    if (std::size_t colon = path.rfind(':'); colon != std::string_view::npos) {
        path = path.substr(colon + 1);
    }
    std::string relative(path);
    std::ranges::replace(relative, '\\', '/');
    relative.erase(0, relative.find_first_not_of('/'));
    fs::path normal = fs::path(relative).lexically_normal();
    if (relative.empty() || normal.empty() || *normal.begin() == ".." || normal.is_absolute()) {
        return false;
    }
    out = PathsSaveRoot() / "host0" / normal;
    return true;
}

int HostFlags(int flags) {
    int host;
    switch (flags & kAccessMask) {
        case SCE_WRONLY:
            host = O_WRONLY;
            break;
        case kReadWrite:
            host = O_RDWR;
            break;
        default:
            host = O_RDONLY;
            break;
    }
    if (flags & kAppend) {
        host |= O_APPEND;
    }
    if (flags & SCE_CREAT) {
        host |= O_CREAT;
    }
    if (flags & SCE_TRUNC) {
        host |= O_TRUNC;
    }
    if (flags & kExclusive) {
        host |= O_EXCL;
    }
#ifdef _WIN32
    // CRT text mode translates CR/LF and treats Ctrl-Z as EOF, corrupting binary game files.
    return host | O_BINARY | O_NOINHERIT;
#else
    return host | O_CLOEXEC;
#endif
}

bool Known(int fd) {
    return g_open.contains(fd);
}

} // namespace

void sceFsReset() {}

int sceOpen(const char *name, int flags) {
    fs::path path;
    if (!HostPath(name, path)) {
        return -ENOENT;
    }
    if (flags & SCE_CREAT) {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
    }
#ifdef _WIN32
    int fd = ::_wopen(path.c_str(), HostFlags(flags), 0644);
#else
    int fd = ::open(path.c_str(), HostFlags(flags), 0644);
#endif
    if (fd < 0) {
        return -errno;
    }
    g_open.insert(fd);
    return fd;
}

int sceClose(int fd) {
    if (!Known(fd)) {
        return -EBADF;
    }
    g_open.erase(fd);
    return ::close(fd) < 0 ? -errno : 0;
}

int sceLseek(int fd, int offset, int whence) {
    if (!Known(fd)) {
        return -EBADF;
    }
    off_t result = ::lseek(fd, offset, whence);
    return result < 0 ? -errno : static_cast<int>(result);
}

int sceRead(int fd, void *buffer, int size) {
    if (!Known(fd)) {
        return -EBADF;
    }
    ssize_t result = ::read(fd, buffer, static_cast<std::size_t>(std::max(size, 0)));
    return result < 0 ? -errno : static_cast<int>(result);
}

int sceWrite(int fd, void *buffer, int size) {
    if (!Known(fd)) {
        return -EBADF;
    }
    ssize_t result = ::write(fd, buffer, static_cast<std::size_t>(std::max(size, 0)));
    return result < 0 ? -errno : static_cast<int>(result);
}
