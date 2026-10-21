#include <gtest/gtest.h>

#include "dungeonparts.hpp"

TEST(DungeonParts, RejectsInvalidTreasureTableSelectors) {
    EXPECT_EQ(PresetSmallItemNo_Get(-1, 0, 0, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(7, 0, 0, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, -1, 0), -1);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, 2, 0), -1);
}

TEST(DungeonParts, RejectsMissingTreasureTable) {
    ITEM_PUT_SET *saved = ItemPutListPtr[0];
    ItemPutListPtr[0] = nullptr;
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, 0, 0), -1);
    ItemPutListPtr[0] = saved;
}
