#include <cstddef>
#include <cstdint>

#include "data/data_read.hpp"
#include "dataalloc.hpp"
#include "platform/arena.hpp"

namespace dc::game {

using namespace dc::data;

// Arena methods keep retail's arithmetic and off-by-one checks; overflow aborts naming the arena.

namespace {

constexpr int kGlobalQuads = 1690000;

// GlobalDataBuffer hands out a host block with the same headroom as every other arena.
unsigned char *GlobalBlock() {
    return ArenaBlock(&GlobalDataBuffer, static_cast<std::size_t>(kGlobalQuads) * 16 * kArenaHeadroom);
}

constexpr std::ptrdiff_t QuadBytes(int quads) {
    return static_cast<std::ptrdiff_t>(quads) * 16;
}

int AlignedUsed(const unsigned char *start, int used) {
    std::uintptr_t slack = reinterpret_cast<std::uintptr_t>(start + QuadBytes(used)) & 63;
    if (slack != 0) {
        used += static_cast<int>((64 - slack) >> 4);
    }
    return used;
}

void CheckAllocation(const void *arena, int used, int quads, int limit) {
    if (quads < 0 || used < 0 || used > limit || quads > limit - used) {
        ArenaOverflow(arena, -1, limit);
    }
}

// The run a CDataAlloc2<1> handed out last, which the placement operators below may grow.
struct Run {
    CDataAlloc2<1> *arena;
    u_char         *block;
    int             quads;
};

Run g_last_run;

// Callers size a placement run in target quadwords, but the host object is larger wherever it
// holds a pointer. While the run is still the arena's last it is grown to the host size so nothing
// allocated afterwards lies under the object's tail.
void *Place(std::size_t size, u_long128 *block) {
    Run   run = g_last_run;
    auto *bytes = reinterpret_cast<u_char *>(block);
    int   quads = static_cast<int>((size + 15) / 16);
    if (run.arena != nullptr && run.block == bytes && quads > run.quads &&
        run.arena->base + QuadBytes(run.arena->used) == bytes + QuadBytes(run.quads)) {
        run.arena->Alloc(quads - run.quads);
        g_last_run = {.arena = run.arena, .block = bytes, .quads = quads};
    }
    return block;
}

} // namespace

void *PlaceDataAllocObject(std::size_t size, u_long128 *block) {
    return Place(size, block);
}

u_char *CDataAlloc2<1>::Alloc(int quads) {
    CheckAllocation(this, used, quads, limit);
    DataReadInvalidateCopiedSources(this, static_cast<std::size_t>(quads));
    u_char *block = base + QuadBytes(used);
    used += quads;
    g_last_run = {.arena = this, .block = block, .quads = quads};
    return block;
}

u_char *CDataAlloc2<1>::Alloc64(int quads) {
    if (quads < 0) {
        ArenaOverflow(this, -1, limit);
    }
    Align64();
    if (quads >= limit - used) {
        ArenaOverflow(this, -1, limit);
    }
    DataReadInvalidateCopiedSources(this, static_cast<std::size_t>(quads));
    u_char *block = base + QuadBytes(used);
    used += quads;
    g_last_run = {.arena = this, .block = block, .quads = quads};
    return block;
}

void CDataAlloc2<1>::Align64() {
    if (used < 0 || used > limit) {
        ArenaOverflow(this, used, limit);
    }
    used = AlignedUsed(base, used);
    if (used >= limit) {
        ArenaOverflow(this, used, limit);
    }
}

u_char *CDataAlloc<1, 1690000>::Alloc(int quads) {
    constexpr int limit = kGlobalQuads * kArenaHeadroom;
    CheckAllocation(this, used, quads, limit);
    u_char *allocation = GlobalBlock() + QuadBytes(used);
    used += quads;
    return allocation;
}

u_char *CDataAlloc<1, 1690000>::Alloc64(int quads) {
    constexpr int limit = kGlobalQuads * kArenaHeadroom;
    if (quads < 0) {
        ArenaOverflow(this, -1, limit);
    }
    Align64();
    if (quads >= limit - used) {
        ArenaOverflow(this, -1, limit);
    }
    u_char *allocation = GlobalBlock() + QuadBytes(used);
    used += quads;
    return allocation;
}

void CDataAlloc<1, 1690000>::Align64() {
    constexpr int limit = kGlobalQuads * kArenaHeadroom;
    if (used < 0 || used > limit) {
        ArenaOverflow(this, used, limit);
    }
    used = AlignedUsed(GlobalBlock(), used);
    if (used >= limit) {
        ArenaOverflow(this, used, limit);
    }
}

void CDataAlloc<1, 6000>::Align64() {
    if (used < 0 || used > 6000) {
        ArenaOverflow(this, used, 6000);
    }
    used = AlignedUsed(reinterpret_cast<const unsigned char *>(block), used);
    if (used >= 6000) {
        ArenaOverflow(this, used, 6000);
    }
}

CDataAlloc2<1>::CDataAlloc2(int limit) : buffer(nullptr), used(0), limit(limit) {
    // A negative limit leaves the base untouched.
    if (limit < 0) {
        return;
    }

    base = buffer;
}

} // namespace dc::game
