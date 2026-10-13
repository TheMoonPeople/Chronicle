#include <gtest/gtest.h>

#include "editground.hpp"
#include "editloop.hpp"
#include "mainselect.hpp"
#include "savedata.hpp"

namespace {

alignas(64) CSaveData g_treant_save;

constexpr int kDry = 33;
constexpr int kFlooded = 13;

} // namespace

TEST(TreantChests, ChestsOpenedBeforeTheFloodStayOpenAfter) {
    g_treant_save.Initialize();
    SaveData = &g_treant_save;
    const int map = MapNo;

    MapNo = kDry;
    EdSetMapFlag(361, 1);
    MapNo = kFlooded;
    EXPECT_EQ(EdGetMapFlag(361), 1);
    EXPECT_EQ(EdGetMapFlag(360), 0);
    EdSetMapFlag(364, 1);
    MapNo = kDry;
    EXPECT_EQ(EdGetMapFlag(364), 1);

    // A save that opened a chest on one map reads it open on the other.
    g_treant_save.SetMapFlag(kDry, 362, 1);
    MapNo = kFlooded;
    EXPECT_EQ(EdGetMapFlag(362), 1);

    // The flooded map's extra chests and other maps' flags stay apart.
    EdSetMapFlag(365, 1);
    MapNo = kDry;
    EXPECT_EQ(EdGetMapFlag(365), 0);
    MapNo = TOWN_MATATAKI;
    EXPECT_EQ(EdGetMapFlag(361), 0);

    MapNo = map;
    SaveData = nullptr;
}
