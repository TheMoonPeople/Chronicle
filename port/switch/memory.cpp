// The arenas' memory on the Switch (platform/memory.hpp), in place of Kazarov's mmap one: page-aligned
// heap blocks, each followed by a page svcSetMemoryPermission makes inaccessible, so a run past the end
// still faults at once. Horizon commits the whole heap up front, so zeroing is a memset and returns
// nothing to the system.

#include "platform/memory.hpp"

#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::size_t kPage = 0x1000;

std::size_t RoundUp(std::size_t value, std::size_t to) { return (value + to - 1) / to * to; }

void SetPermission(unsigned char *page, u32 permission) {
    Result result = svcSetMemoryPermission(page, kPage, permission);
    if (R_FAILED(result)) {
        std::fprintf(stderr, "arena: cannot set the guard page's permission (0x%x)\n", result);
        std::abort();
    }
}

} // namespace

std::size_t ArenaMemoryPageSize() { return kPage; }

ArenaMemory ArenaMemoryMap(std::size_t bytes, std::size_t lead) {
    std::size_t capacity = RoundUp(bytes == 0 ? 64 : bytes, 64);
    std::size_t map_size = RoundUp(lead + capacity, kPage) + kPage;
    auto       *map = static_cast<unsigned char *>(std::aligned_alloc(kPage, map_size));
    if (map == nullptr) {
        std::fprintf(stderr, "arena: cannot allocate %zu bytes\n", map_size);
        std::abort();
    }
    std::memset(map, 0, map_size - kPage);
    SetPermission(map + map_size - kPage, Perm_None);
    return {map, map_size, map + map_size - kPage - capacity, capacity, lead};
}

void ArenaMemoryZero(const ArenaMemory &memory) { ArenaMemoryZeroByRemap(memory); }

void ArenaMemoryZeroByRemap(const ArenaMemory &memory) { std::memset(memory.map, 0, memory.map_size - kPage); }

const unsigned char *ArenaMemoryGuard(const ArenaMemory &memory) { return memory.base + memory.capacity; }

void ArenaMemoryUnmap(ArenaMemory &memory) {
    if (memory.map != nullptr) {
        SetPermission(memory.map + memory.map_size - kPage, Perm_Rw);
        std::free(memory.map);
    }
    memory = {};
}
