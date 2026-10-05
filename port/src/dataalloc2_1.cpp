#include <cstddef>
#include <cstdint>

#include "arena.hpp"
#include "dataalloc.hpp"

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

PC_OVERRIDE void *operator new(size_t size, u_long128 *block) {
    return Place(size, block);
}

PC_OVERRIDE void *operator new[](size_t size, u_long128 *block) {
    return Place(size, block);
}

PC_OVERRIDE u_char *CDataAlloc2<1>::Alloc(int quads) {
    if (used + quads > limit) {
        ArenaOverflow(this, used + quads, limit);
    }
    u_char *block = base + used * 16;
    used += quads;
    g_last_run = {this, block, quads};
    return block;
}

PC_OVERRIDE u_char *CDataAlloc2<1>::Alloc64(int quads) {
    Align64();
    u_char *block = base + used * 16;
    used += quads;
    if (used >= limit) {
        ArenaOverflow(this, used, limit);
    }
    g_last_run = {this, block, quads};
    return block;
}

void CDataAlloc2<1>::Align64() {
    used = AlignedUsed(base, used);
    if (used >= limit) {
        ArenaOverflow(this, used, limit);
    }
}

u_char *CDataAlloc<1, 1690000>::Alloc(int quads) {
    if (used + quads > kGlobalQuads * kArenaHeadroom) {
        ArenaOverflow(this, used + quads, kGlobalQuads * kArenaHeadroom);
    }
    u_char *allocation = GlobalBlock() + used * 16;
    used += quads;
    return allocation;
}

PC_OVERRIDE u_char *CDataAlloc<1, 1690000>::Alloc64(int quads) {
    Align64();
    u_char *allocation = GlobalBlock() + used * 16;
    used += quads;
    if (used >= kGlobalQuads * kArenaHeadroom) {
        ArenaOverflow(this, used, kGlobalQuads * kArenaHeadroom);
    }
    return allocation;
}

void CDataAlloc<1, 1690000>::Align64() {
    used = AlignedUsed(GlobalBlock(), used);
    if (used >= kGlobalQuads * kArenaHeadroom) {
        ArenaOverflow(this, used, kGlobalQuads * kArenaHeadroom);
    }
}

// SystemMesBuffer keeps its embedded storage: LoadSystemMessage points
// SystemMes at block[used] directly.
PC_OVERRIDE u_char *CDataAlloc<1, 6000>::Alloc(int quads) {
    if (used + quads > 6000) {
        ArenaOverflow(this, used + quads, 6000);
    }
    u_char *run = reinterpret_cast<u_char *>(block) + used * 16;
    used += quads;
    return run;
}

void CDataAlloc<1, 6000>::Align64() {
    used = AlignedUsed(reinterpret_cast<const unsigned char *>(block), used);
    if (used >= 6000) {
        ArenaOverflow(this, used, 6000);
    }
}
