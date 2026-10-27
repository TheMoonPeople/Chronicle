#include "dataset.hpp"

#include <array>
#include <limits>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataread_port.hpp"
#include "mglib.hpp"
#include "platform/arena.hpp"

namespace {

// InitWorkBuffer (editloop.cpp, edit_in.cpp) starts EdMenuBuffer this far
// before read_buffer, in what retail's carving leaves of the preceding arena.
constexpr std::size_t kReadBufferLead = 0x180000;

constexpr int kMaxQuads = std::numeric_limits<int>::max() / kArenaHeadroom;

void Carve(CDataAlloc2<1> *arena, int quads) {
    if (arena == nullptr || quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow(arena, quads, kMaxQuads);
    }
    DataReadForgetPackCapacities(arena);
    arena->base = ArenaBlock(arena, static_cast<std::size_t>(quads) * 16 * kArenaHeadroom);
    arena->limit = quads * kArenaHeadroom;
    arena->used = 0;
}

void CarveWorkBuffer(int quads) {
    Carve(&workbuffer, quads);
    WorkBuffer = &workbuffer;
}

u_long128 *CarvePackets(int quads, int which) {
    static constexpr std::array<char, 2> kPacketOwners{};
    if (which < 0 || which >= 2 || quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow("VIF packet buffer", quads, kMaxQuads);
    }
    return reinterpret_cast<u_long128 *>(ArenaBlock(&kPacketOwners[which], static_cast<std::size_t>(quads) * 16 * kArenaHeadroom));
}

void CarveReadBuffer(int quads) {
    if (quads <= 0 || quads > kMaxQuads) {
        ArenaOverflow("file read buffer", quads, kMaxQuads);
    }
    DataReadForgetPackCapacities(read_buffer);
    read_buffer = reinterpret_cast<u_int *>(ArenaBlock(static_cast<const void *>(&read_buffer), static_cast<std::size_t>(quads) * 16 * kArenaHeadroom, kReadBufferLead));
}

} // namespace

void InitializeDataBuffer() {
    ArenaClearAll();
    DataReadClearPackCapacities();
    GlobalDataBuffer.used = 0;
    Carve(&WaterData, 10);
    Carve(&ActiveData0, 25000);
    Carve(&ActiveData1, 25000);
}

void SetDataBuffer(CDataAlloc2<1> *arena, int quads) {
    Carve(arena, quads);
}

void SetPacketReadBuffer(int packet_quads, int read_quads) {
    CarveReadBuffer(read_quads);
    MGInitVif1Packet(CarvePackets(packet_quads, 0), CarvePackets(packet_quads, 1));
    CarveWorkBuffer(2048);
}

void BufferAllClear() {
    ArenaClearAll();
    DataReadClearPackCapacities();
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
