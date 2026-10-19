#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>

#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "options/rows.hpp"
#include "party_pool.hpp"
#include "platform/config.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "shot_freefuncs.hpp"
#include "userstatus.hpp"

/**
 * Checks opt-in companion healing without loading a dungeon or touching player saves.
 */
TEST(PartyPool, ToggleAndPartyBoundaries) {
    const Config previous = ConfigGet();
    const auto   previous_root = PathsSaveRoot();
    const auto   root = std::filesystem::temp_directory_path() / ("dc-pool-" + std::to_string(dc::test::ProcessId()));
    PathsSetSaveRoot(root);
    auto status = std::make_unique<CUserStatus>();
    status->party_size = 4;
    status->cur_chara = 0;
    for (int member = 0; member < 6; ++member) {
        status->max_hp[member] = 100;
        status->hp[member] = member == 2 ? 0 : 20;
        status->water_max[member] = 80;
        status->water_now[member] = 10;
    }
    Config config = previous;
    config.party_pool_healing = false;
    EXPECT_TRUE(ConfigChange(config));
    RestorePoolCompanions(*status);
    EXPECT_EQ(status->hp[1], 20);
    config.party_pool_healing = true;
    EXPECT_TRUE(ConfigChange(config));
    RestorePoolCompanions(*status);
    EXPECT_EQ(status->hp[0], 20);
    EXPECT_EQ(status->hp[1], 100);
    EXPECT_FLOAT_EQ(status->water_now[1], 80);
    EXPECT_EQ(status->hp[2], 0);
    EXPECT_EQ(status->hp[3], 100);
    EXPECT_EQ(status->hp[4], 20);
    EXPECT_TRUE(ConfigParse(ConfigSerialize(config)).party_pool_healing);
    EXPECT_FALSE(ConfigParse("{}").party_pool_healing);
    ConfigChange(previous);
    PathsSetSaveRoot(previous_root);
    std::filesystem::remove_all(root);
}

/**
 * Checks that the QOL menu exposes an independent, default-off pool setting.
 */
TEST(PartyPool, QolMenuToggleAndReset) {
    Config     config;
    const auto pages = options::Pages();
    const auto page = std::find_if(pages.begin(), pages.end(),
                                   [](const options::Page &entry) { return std::string_view(entry.name) == "QOL"; });
    ASSERT_NE(page, pages.end());
    const auto row = std::find_if(page->rows.begin(), page->rows.end(),
                                  [](const options::Row &entry) { return std::string_view(entry.key) == "qol.party_pool_healing"; });
    ASSERT_NE(row, page->rows.end());
    EXPECT_EQ(options::RowValue(*row, config), "Off");
    EXPECT_TRUE(options::StepRow(*row, config, 1, false));
    EXPECT_TRUE(config.party_pool_healing);
    EXPECT_FALSE(config.feather_toggle);
    EXPECT_FALSE(config.inventory_auto_repair);
    EXPECT_FALSE(config.skip_atla_cutscene);
    EXPECT_FALSE(config.skip_small_chest_cutscene);
    options::ResetPage(*page, config);
    EXPECT_FALSE(config.party_pool_healing);
}

/** Ticks until the next pool healing pulse. */
extern int healingSpeed;

/**
 * Checks the game's water detection and healing hook against a bounded pool fixture.
 */
TEST(PartyPool, WaterDetectionAndPulse) {
    const Config previous = ConfigGet();
    const auto   previous_root = PathsSaveRoot();
    const auto   root = std::filesystem::temp_directory_path() / ("dc-pool-hook-" + std::to_string(dc::test::ProcessId()));
    PathsSetSaveRoot(root);
    auto      status = std::make_unique<CUserStatus>();
    auto      map = std::make_unique<CDungeonMap>();
    auto     *previous_status = UserStatus;
    auto     *previous_map = NowDngMap;
    const int previous_speed = healingSpeed;
    float     previous_position[4];
    std::copy_n(CharaMain.pos, 4, previous_position);
    UserStatus = status.get();
    NowDngMap = map.get();
    std::fill_n(CharaMain.pos, 4, 0.0f);
    for (auto &cell : map->cells) {
        cell.parts_no = -1;
    }
    map->cells[0].parts_no = 0;
    auto &water = map->parts[0].water;
    water.used = 1;
    water.vertex[0][0] = -40.0f;
    water.vertex[0][1] = 10.0f;
    water.vertex[0][2] = -40.0f;
    water.vertex[3][0] = 40.0f;
    water.vertex[3][2] = 40.0f;
    status->party_size = 2;
    status->cur_chara = 0;
    status->max_hp[0] = status->hp[0] = 100;
    status->max_hp[1] = 100;
    status->hp[1] = 20;
    status->water_max[0] = status->water_now[0] = 80;
    status->water_max[1] = 80;
    status->water_now[1] = 10;
    Config config = previous;
    config.party_pool_healing = false;
    EXPECT_TRUE(ConfigChange(config));
    healingSpeed = 0;
    HealingWater();
    EXPECT_EQ(status->hp[1], 20);
    config.party_pool_healing = true;
    EXPECT_TRUE(ConfigChange(config));
    healingSpeed = 1;
    HealingWater();
    EXPECT_EQ(status->hp[1], 20);
    HealingWater();
    EXPECT_EQ(status->hp[1], 100);
    EXPECT_FLOAT_EQ(status->water_now[1], 80);
    status->hp[1] = 20;
    status->water_now[1] = 10;
    CharaMain.pos[0] = 60.0f;
    healingSpeed = 0;
    HealingWater();
    EXPECT_EQ(status->hp[1], 20);
    EXPECT_FLOAT_EQ(status->water_now[1], 10);
    UserStatus = previous_status;
    NowDngMap = previous_map;
    healingSpeed = previous_speed;
    std::copy_n(previous_position, 4, CharaMain.pos);
    ConfigChange(previous);
    PathsSetSaveRoot(previous_root);
    std::filesystem::remove_all(root);
}
