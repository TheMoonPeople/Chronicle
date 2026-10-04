#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <cstring>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "platform/memory.hpp"
#include "platform_fixture.hpp"

extern CDataAlloc<1, 6000> SystemMesBuffer;

namespace {

bool Aligned64(const void *pointer) {
    return (reinterpret_cast<std::uintptr_t>(pointer) & 63) == 0;
}

} // namespace

TEST(PlatformArena, CarvesWithHeadroom) {
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 1000);
    ASSERT_TRUE(Aligned64(VisualData.base));
    ASSERT_TRUE(VisualData.limit == 4000);
    ASSERT_TRUE(VisualData.used == 0);

    // Past what retail's 1000 quadwords would hold.
    u_char *first = VisualData.Alloc(1500);
    ASSERT_TRUE(first == VisualData.base);
    std::memset(first, 0x5A, 1500 * 16);
    VisualData.Alloc(1);
    u_char *aligned = VisualData.Alloc64(1000);
    ASSERT_TRUE(Aligned64(aligned));
    ASSERT_TRUE(aligned == VisualData.base + 1504 * 16);
    std::memset(VisualData.base, 0x33, static_cast<std::size_t>(VisualData.limit) * 16);

    SetDataBuffer(&MotionData, 10);
    ASSERT_TRUE(MotionData.base != VisualData.base);
    ASSERT_TRUE(MotionData.limit == 40);
    ASSERT_TRUE(Aligned64(MotionData.Alloc64(3)));
    ASSERT_TRUE(Aligned64(MotionData.Alloc64(1)));
    ASSERT_TRUE(MotionData.used == 5);
}

TEST(PlatformArena, ReusesAndZeroes) {
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 2000);
    u_char *base = TextureData.base;
    std::memset(base, 0xFF, 2000 * 16);
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 1000);
    ASSERT_TRUE(TextureData.base == base);
    ASSERT_TRUE(TextureData.limit == 4000);
    for (int i = 0; i < 2000 * 16; ++i) {
        ASSERT_TRUE(base[i] == 0);
    }

    SetDataBuffer(&TextureData, 100000);
    ASSERT_TRUE(TextureData.base != base);
    ASSERT_TRUE(base[0] == 0);
    TextureData.base[100000 * 4 * 16 - 1] = 1;
}

TEST(PlatformArena, OverflowAborts) {
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 100);
    DC_ASSERT_ABORT([] { VisualData.Alloc(401); });
    DC_ASSERT_ABORT([] { VisualData.Alloc64(400); });
    DC_ASSERT_EXIT([] { VisualData.Alloc(400); }, 0);
    // An overrun that skips the allocator runs into the guard page behind the block.
    DC_ASSERT_FAULT([] { VisualData.base[VisualData.limit * 16] = 1; });
    DC_ASSERT_ABORT([] { SystemMesBuffer.Alloc(6001); });
}

TEST(PlatformArena, ModeBuffers) {
    BufferAllClear();
    ASSERT_TRUE(VisualData.limit == 600000 * 4);
    ASSERT_TRUE(ActiveData0.limit == 25000 * 4 && ActiveData1.limit == 25000 * 4);
    ASSERT_TRUE(ActiveData0.base != ActiveData1.base);
    ASSERT_TRUE(WorkBuffer == &workbuffer);
    ASSERT_TRUE(workbuffer.limit == 4096 * 4);
    ASSERT_TRUE(Aligned64(read_buffer));
    read_buffer[100000 * 4 * 4 - 1] = 1;

    InitializeDataBuffer();
    ASSERT_TRUE(WaterData.limit == 40 && ActiveData0.limit == 100000);
    SetPacketReadBuffer(30000, 140000);
    ASSERT_TRUE(workbuffer.limit == 2048 * 4 && workbuffer.used == 0);
    // EdMenuBuffer starts 0x180000 bytes before read_buffer (InitWorkBuffer).
    u_char *menu = reinterpret_cast<u_char *>(read_buffer) - 0x180000;
    std::memset(menu, 1, 0x3A2E0 * 16);

    u_char *global = GlobalDataBuffer.Alloc64(10);
    ASSERT_TRUE(Aligned64(global));
    GlobalDataBuffer.Alloc(1);
    ASSERT_TRUE(Aligned64(GlobalDataBuffer.Alloc64(1)));

    SystemMesBuffer.used = 1;
    SystemMesBuffer.Align64();
    ASSERT_TRUE(Aligned64(&SystemMesBuffer.block[SystemMesBuffer.used]));
}
