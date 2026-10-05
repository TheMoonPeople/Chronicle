#include "dataset.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "arena.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "mglib.hpp"
#include "platform/memory.hpp"

namespace {

// InitWorkBuffer (editloop.cpp, edit_in.cpp) starts EdMenuBuffer this far
// before read_buffer, in what retail's carving leaves of the preceding arena.
constexpr std::size_t kReadBufferLead = 0x180000;

struct Block {
    const void *owner;
    ArenaMemory memory;
};

std::vector<Block> g_blocks;
// Blocks an owner outgrew stay mapped: retail's memory never went away, and a
// pointer the game kept into an old arena reads zeroes rather than faulting.
std::vector<Block> g_retired;

Block MapBlock(const void *owner, std::size_t bytes, std::size_t lead) {
    return {owner, ArenaMemoryMap(bytes, lead)};
}

// Every 32-bit truncation the game makes of an arena pointer is widened; one left behind only shows
// when the pointer has bits above 32, so a debug build refuses to run where none would.
void CheckSomeBlockHigh() {
#ifndef NDEBUG
    for (const Block &block : g_blocks) {
        if (IsAbove4GiB(block.memory.base)) {
            return;
        }
    }
    std::fprintf(stderr, "arena: none lies above 4 GiB, where a truncated pointer would show\n");
    std::abort();
#endif
}

void Carve(CDataAlloc2<1> *arena, int quads) {
    arena->base = ArenaBlock(arena, static_cast<std::size_t>(quads) * 16 * kArenaHeadroom);
    arena->limit = quads * kArenaHeadroom;
    arena->used = 0;
}

void CarveWorkBuffer(int quads) {
    Carve(&workbuffer, quads);
    WorkBuffer = &workbuffer;
}

u_long128 *CarvePackets(int quads, int which) {
    static const char keys[2] = {};
    return reinterpret_cast<u_long128 *>(ArenaBlock(&keys[which], static_cast<std::size_t>(quads) * 16 * kArenaHeadroom));
}

void CarveReadBuffer(int quads) {
    read_buffer = reinterpret_cast<u_int *>(ArenaBlock(&read_buffer, static_cast<std::size_t>(quads) * 16 * kArenaHeadroom, kReadBufferLead));
}

} // namespace

unsigned char *ArenaBlock(const void *owner, std::size_t bytes, std::size_t lead) {
    for (Block &block : g_blocks) {
        if (block.owner != owner) {
            continue;
        }
        if (block.memory.capacity >= bytes && block.memory.lead >= lead) {
            return block.memory.base;
        }
        ArenaMemoryZero(block.memory);
        g_retired.push_back(block);
        block = MapBlock(owner, bytes, lead);
        CheckSomeBlockHigh();
        return block.memory.base;
    }
    g_blocks.push_back(MapBlock(owner, bytes, lead));
    CheckSomeBlockHigh();
    return g_blocks.back().memory.base;
}

void ArenaClearAll() {
    for (const Block &block : g_blocks) {
        ArenaMemoryZero(block.memory);
    }
}

void *PortImagePointer(std::int32_t truncated) {
    static const char anchor = 0;
    if (truncated == 0) {
        return nullptr;
    }
    return reinterpret_cast<void *>(WidenNear(reinterpret_cast<std::uintptr_t>(&anchor), static_cast<std::uint32_t>(truncated)));
}

void ArenaOverflow(const void *arena, int used, int limit) {
    std::fprintf(stderr, "arena %p: allocation overflow, %d of %d quadwords\n", arena, used, limit);
    std::abort();
}

void InitializeDataBuffer() {
    ArenaClearAll();
    GlobalDataBuffer.used = 0;
    Carve(&WaterData, 10);
    Carve(&ActiveData0, 25000);
    Carve(&ActiveData1, 25000);
}

PC_OVERRIDE void SetDataBuffer(CDataAlloc2<1> *arena, int quads) {
    Carve(arena, quads);
}

PC_OVERRIDE void SetPacketReadBuffer(int packet_quads, int read_quads) {
    CarveReadBuffer(read_quads);
    MGInitVif1Packet(CarvePackets(packet_quads, 0), CarvePackets(packet_quads, 1));
    CarveWorkBuffer(2048);
}

void BufferAllClear() {
    ArenaClearAll();
    GlobalDataBuffer.used = 0;
    Carve(&VisualData, 600000);
    Carve(&MotionData, 200000);
    Carve(&TextureData, 300000);
    Carve(&WaterData, 90000);
    Carve(&ActiveData0, 25000);
    Carve(&ActiveData1, 25000);
    CarveReadBuffer(100000);
    CarveWorkBuffer(4096);
    MGInitVif1Packet(CarvePackets(50000, 0), CarvePackets(50000, 1));
}

// The renderer builds no VIF packets; nothing reads the packet builders.
PC_OVERRIDE void MGInitVif1Packet(u_long128 *buffer0, u_long128 *buffer1) {}
