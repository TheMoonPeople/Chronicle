#include <gtest/gtest.h>

#include "dungeonparts.hpp"

namespace dc::test {

using namespace dc::game;

TEST(DungeonParts, RejectsInvalidTreasureTableSelectors) {
    EXPECT_EQ(PresetSmallItemNo_Get(-1, 0, 0, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(7, 0, 0, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, -1, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, 2, 0), -1);
}

} // namespace dc::test
