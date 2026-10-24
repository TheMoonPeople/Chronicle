#include <cstddef>
#include <cstdint>

#include "dataalloc.hpp"
#include "platform/arena.hpp"

// Retail's arena methods with its arithmetic and its off-by-one checks kept;
// only the endless loop on overflow becomes an abort that names the arena.

namespace {

constexpr int kGlobalQuads = 1690000;

// GlobalDataBuffer's own block stays where the PS2 build put it; what it hands
// out comes from a host block with the same headroom as every other arena.
unsigned char *GlobalBlock() {
    return ArenaBlock(&GlobalDataBuffer, static_cast<std::size_t>(kGlobalQuads) * 16 * kArenaHeadroom);
}

int AlignedUsed(const unsigned char *start, int used) {
    std::uintptr_t slack = reinterpret_cast<std::uintptr_t>(start + used * 16) & 63;
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

// Retail sizes the run it places an object in by hand, in the PS2's quadwords, and the host's
// object is larger wherever it holds a pointer. The run is still the arena's last when the object
// is placed, so it is grown to the host's size and nothing allocated after it lies under the
// object's tail.
void *Place(std::size_t size, u_long128 *block) {
    Run   run = g_last_run;
    auto *bytes = reinterpret_cast<u_char *>(block);
    int   quads = static_cast<int>((size + 15) / 16);
    if (run.arena != nullptr && run.block == bytes && quads > run.quads &&
        run.arena->base + run.arena->used * 16 == bytes + run.quads * 16) {
        run.arena->Alloc(quads - run.quads);
        g_last_run = {run.arena, bytes, quads};
    }
    return block;
}

} // namespace

void *PlaceDataAllocObject(std::size_t size, u_long128 *block) {
    return Place(size, block);
}

PORT_OVERRIDE u_char *CDataAlloc2<1>::Alloc(int quads) {
    CheckAllocation(this, used, quads, limit);
    u_char *block = base + used * 16;
    used += quads;
    g_last_run = {this, block, quads};
    return block;
}

PORT_OVERRIDE u_char *CDataAlloc2<1>::Alloc64(int quads) {
    if (quads < 0) {
        ArenaOverflow(this, -1, limit);
    }
    Align64();
    if (quads >= limit - used) {
        ArenaOverflow(this, -1, limit);
    }
    u_char *block = base + used * 16;
    used += quads;
    g_last_run = {this, block, quads};
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
    u_char *allocation = GlobalBlock() + used * 16;
    used += quads;
    return allocation;
}

PORT_OVERRIDE u_char *CDataAlloc<1, 1690000>::Alloc64(int quads) {
    constexpr int limit = kGlobalQuads * kArenaHeadroom;
    if (quads < 0) {
        ArenaOverflow(this, -1, limit);
    }
    Align64();
    if (quads >= limit - used) {
        ArenaOverflow(this, -1, limit);
    }
    u_char *allocation = GlobalBlock() + used * 16;
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
