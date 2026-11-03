#include <gtest/gtest.h>

#include <algorithm>

#include "dngstatusdata.hpp"
#include "dungeon/limits.hpp"
#include "savedata.hpp"
#include "userstatus.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

alignas(64) CSaveData g_save;

// A new game's status with no limit rolled, so a test rolls its own.
CDngStatusData &StatusWithoutRolls() {
    g_save.Initialize();
    CDngStatusData &status = *g_save.GetDngStatus();
    for (auto &zones : status.res_limit_zone_id) {
        std::ranges::fill(zones, static_cast<char>(RES_LIMIT_ZONE_NONE));
    }
    return status;
}

} // namespace

TEST(DungeonLimits, TheFloorsTheScriptsAlwaysLimitNeedNoRoll) {
    const CDngStatusData &status = StatusWithoutRolls();
    EXPECT_EQ(dungeon::FloorLimit(status, 0, 6), RES_LIMIT_ZONE_NO_WEAPON_CHANGE);
    EXPECT_EQ(dungeon::FloorLimit(status, 0, 10), RES_LIMIT_ZONE_XIAO);
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 3), RES_LIMIT_ZONE_XIAO);
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 11), RES_LIMIT_ZONE_GORO);
    EXPECT_EQ(dungeon::FloorLimit(status, 3, 4), RES_LIMIT_ZONE_GORO);
    EXPECT_EQ(dungeon::FloorLimit(status, 3, 12), RES_LIMIT_ZONE_UNGAGA);
    EXPECT_EQ(dungeon::FloorLimit(status, 0, 0), RES_LIMIT_ZONE_NONE);
    EXPECT_EQ(dungeon::FloorLimit(status, 2, 5), RES_LIMIT_ZONE_NONE);
}

TEST(DungeonLimits, ARolledLimitShowsWhereTheFloorsScriptAppliesIt) {
    CDngStatusData &status = StatusWithoutRolls();
    status.res_limit_zone_id[1][5] = RES_LIMIT_ZONE_THIRST;
    status.res_limit_zone_id[4][2] = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 5), RES_LIMIT_ZONE_THIRST);
    EXPECT_EQ(dungeon::FloorLimit(status, 4, 2), RES_LIMIT_ZONE_NO_WEAPON_CHANGE);

    // A fixed map's floor and the boss's never reach _SET_RES_LIMMIT_ZONE.
    status.res_limit_zone_id[1][8] = RES_LIMIT_ZONE_THIRST;
    status.res_limit_zone_id[2][17] = RES_LIMIT_ZONE_THIRST;
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 8), RES_LIMIT_ZONE_NONE);
    EXPECT_EQ(dungeon::FloorLimit(status, 2, 17), RES_LIMIT_ZONE_NONE);

    // The script's own limit is set after the rolled one, and wins.
    status.res_limit_zone_id[1][3] = RES_LIMIT_ZONE_THIRST;
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 3), RES_LIMIT_ZONE_XIAO);

    EXPECT_EQ(dungeon::FloorLimit(status, 6, 0), RES_LIMIT_ZONE_NONE) << "the Demon Shaft has no limits";
    EXPECT_EQ(dungeon::FloorLimit(status, 1, -1), RES_LIMIT_ZONE_NONE);
    EXPECT_EQ(dungeon::FloorLimit(status, 1, 30), RES_LIMIT_ZONE_NONE);
}

TEST(DungeonLimits, EachLimitIsToldByTheMessageRetailShowsOnArrival) {
    int insert = 0;
    EXPECT_EQ(dungeon::FloorLimitMessage(RES_LIMIT_ZONE_XIAO, insert), 0xB);
    EXPECT_EQ(insert, 0x33) << "Xiao's name";
    EXPECT_EQ(dungeon::FloorLimitMessage(RES_LIMIT_ZONE_NO_WEAPON_CHANGE, insert), 0xD);
    EXPECT_EQ(insert, -1);
    EXPECT_EQ(dungeon::FloorLimitMessage(RES_LIMIT_ZONE_THIRST, insert), 0xE);
    EXPECT_EQ(dungeon::FloorLimitMessage(RES_LIMIT_ZONE_NONE, insert), -1);
}

} // namespace dc::test
