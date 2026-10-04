#include "memory.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>

namespace {

#ifdef MAP_NORESERVE
constexpr int kReserve = MAP_NORESERVE;
#else
constexpr int kReserve = 0;
#endif

std::size_t RoundUp(std::size_t value, std::size_t to) { return (value + to - 1) / to * to; }

void *Map(void *at, std::size_t size, int extra) {
#ifdef _WIN32
    return VirtualAlloc(at, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *map = mmap(at, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | kReserve | extra, -1, 0);
    return map == MAP_FAILED ? nullptr : map;
#endif
}

} // namespace

std::size_t ArenaMemoryPageSize() {
#ifdef _WIN32
    static const std::size_t size = [] {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return static_cast<std::size_t>(info.dwPageSize);
    }();
#else
    static const std::size_t size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
    return size;
}

ArenaMemory ArenaMemoryMap(std::size_t bytes, std::size_t lead) {
    std::size_t page = ArenaMemoryPageSize();
    std::size_t capacity = RoundUp(bytes == 0 ? 64 : bytes, 64);
    std::size_t map_size = RoundUp(lead + capacity, page) + page;
    void       *map = Map(nullptr, map_size, 0);
    if (map == nullptr) {
        std::fprintf(stderr, "arena: cannot map %zu bytes\n", map_size);
        std::abort();
    }
    auto *bytes_map = static_cast<unsigned char *>(map);
#ifdef _WIN32
    DWORD previous;
    if (!VirtualProtect(bytes_map + map_size - page, page, PAGE_NOACCESS, &previous)) {
        std::fprintf(stderr, "arena: cannot protect guard page\n");
        std::abort();
    }
#else
    mprotect(bytes_map + map_size - page, page, PROT_NONE);
#endif
    return {bytes_map, map_size, bytes_map + map_size - page - capacity, capacity, lead};
}

void ArenaMemoryZero(const ArenaMemory &memory) {
#if defined(__linux__)
    // Private anonymous pages read back as zero after MADV_DONTNEED; elsewhere it only hints.
    madvise(memory.map, memory.map_size - ArenaMemoryPageSize(), MADV_DONTNEED);
#else
    ArenaMemoryZeroByRemap(memory);
#endif
}

// macOS has no zeroing madvise: MADV_FREE_REUSABLE leaves the old bytes until the pager takes the
// page, and a memset would commit every page of arenas sized at four times retail's. A fresh
// mapping over the same range is zero and uncommitted, and keeps the address.
void ArenaMemoryZeroByRemap(const ArenaMemory &memory) {
    std::size_t usable = memory.map_size - ArenaMemoryPageSize();
#ifdef _WIN32
    // MEM_RESET does not guarantee zeros; decommit/recommit keeps the reservation and guard.
    if (!VirtualFree(memory.map, usable, MEM_DECOMMIT) ||
        VirtualAlloc(memory.map, usable, MEM_COMMIT, PAGE_READWRITE) != memory.map) {
#else
    if (Map(memory.map, usable, MAP_FIXED) != memory.map) {
#endif
        std::fprintf(stderr, "arena: cannot re-map %zu bytes at %p to zero them\n", usable,
                     static_cast<void *>(memory.map));
        std::abort();
    }
}

const unsigned char *ArenaMemoryGuard(const ArenaMemory &memory) { return memory.base + memory.capacity; }

void ArenaMemoryUnmap(ArenaMemory &memory) {
    if (memory.map != nullptr) {
#ifdef _WIN32
        VirtualFree(memory.map, 0, MEM_RELEASE);
#else
        munmap(memory.map, memory.map_size);
#endif
    }
    memory = {};
}
