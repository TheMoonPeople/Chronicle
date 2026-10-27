// The arenas' memory on the Switch (platform/memory.hpp), in place of Kazarov's mmap one: page-aligned
// heap blocks, each followed by a page svcSetMemoryPermission makes inaccessible, so a run past the end
// still faults at once. Horizon commits the whole heap up front, so zeroing is a memset and returns
// nothing to the system.
//
// The heap outlives the game: the Homebrew Menu's loader starts the next program in the same process
// on the same heap, so a guard page left inaccessible faults whatever lands on it next. Every guard
// page still standing is made accessible again when the game exits.

#include "platform/memory.hpp"

#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>

#include "switch_memory.hpp"

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

// The guard pages standing; made accessible again at exit.
std::unordered_set<unsigned char *> &Guards() {
    static auto *guards = new std::unordered_set<unsigned char *>;
    return *guards;
}

void AddGuard(unsigned char *page) {
    SetPermission(page, Perm_None);
    Guards().insert(page);
}

void RemoveGuard(unsigned char *page) {
    SetPermission(page, Perm_Rw);
    Guards().erase(page);
}

} // namespace

namespace dc::platform {

void SwitchRestoreMemoryGuards() {
    for (unsigned char *guard : Guards()) {
        SetPermission(guard, Perm_Rw);
    }
    Guards().clear();
}

} // namespace dc::platform

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
    AddGuard(map + map_size - kPage);
    return {map, map_size, map + map_size - kPage - capacity, capacity, lead};
}

void ArenaMemoryZero(const ArenaMemory &memory) { ArenaMemoryZeroByRemap(memory); }

void ArenaMemoryZeroByRemap(const ArenaMemory &memory) { std::memset(memory.map, 0, memory.map_size - kPage); }

const unsigned char *ArenaMemoryGuard(const ArenaMemory &memory) { return memory.base + memory.capacity; }

void ArenaMemoryUnmap(ArenaMemory &memory) {
    if (memory.map != nullptr) {
        RemoveGuard(memory.map + memory.map_size - kPage);
        std::free(memory.map);
    }
    memory = {};
}
