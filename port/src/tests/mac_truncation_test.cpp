#include <gtest/gtest.h>

#include <cstdint>

#include "menu_draw.hpp"
#include "platform/memory.hpp"
#include "texture.hpp"

namespace dc::test {

using namespace dc::game;

TEST(MacTruncation, MenuAlignmentKeepsHighAddresses) {
    auto *high = reinterpret_cast<u_long128 *>(std::uintptr_t{0x7f1234567810});
    ASSERT_TRUE(reinterpret_cast<std::uintptr_t>(MenuCalcBufAlignment(high)) == 0x7f1234567840);
    auto *aligned = reinterpret_cast<u_long128 *>(std::uintptr_t{0x7f1234567840});
    ASSERT_TRUE(MenuCalcBufAlignment(aligned) == aligned);
}

TEST(MacTruncation, TextureBufferInHighMemory) {
    ArenaMemory memory = ArenaMemoryMap(1 << 20);
    auto       *buffer = reinterpret_cast<u_long128 *>(memory.base + 16);
    TexManager.SetBuffer(buffer, 1000);
    ASSERT_TRUE(reinterpret_cast<std::uintptr_t>(TexManager.buffer) % 128 == 0);
    ASSERT_TRUE(TexManager.buffer == buffer + 7);
    ASSERT_TRUE(TexManager.buffer_size == 993);
    ArenaMemoryUnmap(memory);
}

} // namespace dc::test
