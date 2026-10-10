#include <gtest/gtest.h>

#include "editground.hpp"
#include "editloop.hpp"
#include "mainselect.hpp"
#include "savedata.hpp"
#include "treant_chests_port.hpp"

namespace {

alignas(64) CSaveData g_treant_save;

} // namespace

// Treant's clearing's five chests are opened once for both of its maps (port/src/treant_chests.cpp).
TEST(TreantChests, ChestsOpenedBeforeTheFloodStayOpenAfter) {
    g_treant_save.Initialize();
    SaveData = &g_treant_save;
    const int map = MapNo;

    MapNo = kTreantClearingDry;
    EdSetMapFlag(361, 1);
    MapNo = kTreantClearingFlooded;
    EXPECT_EQ(EdGetMapFlag(361), 1);
    EXPECT_EQ(EdGetMapFlag(360), 0);
    EdSetMapFlag(364, 1);
    MapNo = kTreantClearingDry;
    EXPECT_EQ(EdGetMapFlag(364), 1);

    // A save that opened a chest in one map before this is read as open in the other.
    g_treant_save.SetMapFlag(kTreantClearingDry, 362, 1);
    MapNo = kTreantClearingFlooded;
    EXPECT_EQ(EdGetMapFlag(362), 1);

    // The flooded clearing's own chests, and every other map's flags, stay apart.
    EdSetMapFlag(365, 1);
    MapNo = kTreantClearingDry;
    EXPECT_EQ(EdGetMapFlag(365), 0);
    MapNo = TOWN_MATATAKI;
    EXPECT_EQ(EdGetMapFlag(361), 0);
    EXPECT_FALSE(TreantChestFlag(TOWN_MATATAKI, 361));
    EXPECT_FALSE(TreantChestFlag(kTreantClearingFlooded, 359));
    EXPECT_FALSE(TreantChestFlag(kTreantClearingFlooded, 365));

    MapNo = map;
    SaveData = nullptr;
}
