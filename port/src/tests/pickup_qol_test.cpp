#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include "pickup_qol.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "savedata.hpp"
#include "userstatus.hpp"
#include "sysmes.hpp"
#include "platform/config.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"

class PickupQol : public testing::Test {
protected:
    std::unique_ptr<CUserStatus> status = std::make_unique<CUserStatus>();
    std::unique_ptr<CDungeonMap> map = std::make_unique<CDungeonMap>();
    std::unique_ptr<CSaveData> save = std::make_unique<CSaveData>();
    CUserStatus *old_status = UserStatus;
    CDungeonMap *old_map = NowDngMap;
    CSaveData *old_save = SaveData;
    int old_event = iventActive;
    Config old_config = ConfigGet();
    std::filesystem::path old_root = PathsSaveRoot();
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("dc-pickup-qol-" + std::to_string(dc::test::ProcessId()));

    void SetUp() override {
        PathsSetSaveRoot(root);
        UserStatus = status.get();
        NowDngMap = map.get();
        SaveData = save.get();
        status->cur_chara = CHARA_TOAN;
        status->item_pack.num = 10;
        std::fill_n(status->item_pack.item, 103, -1);
        std::fill_n(status->item_pack.quick_item_slot, 3, -1);
        for (auto &item : status->consumable_items) item.id = -1;
        SetOptions(false, false);
    }
    void SetOptions(bool atla, bool chest) {
        Config config = ConfigGet();
        config.skip_atla_cutscene = atla;
        config.skip_small_chest_cutscene = chest;
        ASSERT_TRUE(ConfigChange(config));
    }
    void Chest(int item = ITEM_REPAIR_POWDER) {
        iventActive = 0;
        map->events[0].kind = DNG_EVENT_TREASURE_BOX;
        map->events[0].index = 0;
        map->boxes[0].kind = TREASURE_BOX_SMALL;
        map->boxes[0].closed = 1;
        map->boxes[0].trap_no = 0;
        map->boxes[0].item_no = item;
    }
    void Atla() {
        iventActive = 1;
        map->events[1].kind = DNG_EVENT_ATRA;
        map->events[1].index = 0;
        map->atra[0].used = 1;
        map->atra[0].atra_no = 0;
        status->atra_registry[0][0].id = 40;
        status->atra_registry[0][0].refcount = 1;
        std::fill_n(status->atra_grid[0][0], 8, -1);
        status->atra_grid[0][0][0] = 0;
        std::fill_n(save->GetElemData(0), 128, -1);
    }
    void TearDown() override {
        ConfigChange(old_config);
        PathsSetSaveRoot(old_root);
        UserStatus = old_status;
        NowDngMap = old_map;
        SaveData = old_save;
        iventActive = old_event;
        std::error_code error;
        std::filesystem::remove(root / "config.json", error);
        std::filesystem::remove(root, error);
    }
};

TEST(PickupConfig, IndependentTogglesSurviveRoundTrip) {
    for (bool atla : {false, true}) for (bool chest : {false, true}) {
        Config config;
        config.skip_atla_cutscene = atla;
        config.skip_small_chest_cutscene = chest;
        const auto loaded = ConfigParse(ConfigSerialize(config));
        EXPECT_EQ(loaded.skip_atla_cutscene, atla);
        EXPECT_EQ(loaded.skip_small_chest_cutscene, chest);
    }
}

TEST_F(PickupQol, DisabledOrOtherToggleLeavesRetailSequenceIntact) {
    for (bool enabled : {false, true}) {
        SetOptions(enabled, false);
        Chest();
        EXPECT_FALSE(QolSkipSmallChest(0));
        EXPECT_EQ(map->boxes[0].closed, 1);
        EXPECT_EQ(status->item_pack.item[0], -1);
        SetOptions(false, enabled);
        Atla();
        EXPECT_FALSE(QolSkipAtla(0, 0));
        EXPECT_EQ(map->atra[0].used, 1);
        EXPECT_EQ(save->GetElemData(0)[0], -1);
    }
}

TEST_F(PickupQol, SmallChestAwardsOnceAndRemovesEvent) {
    SetOptions(false, true);
    Chest();
    ASSERT_TRUE(QolSkipSmallChest(0));
    EXPECT_EQ(status->item_pack.item[0], ITEM_REPAIR_POWDER);
    EXPECT_EQ(map->boxes[0].closed, 0);
    EXPECT_EQ(map->events[0].kind, DNG_EVENT_NONE);
    EXPECT_EQ(iventActive, -1);
    EXPECT_EQ(SystemMesInputKey, 0);
    EXPECT_EQ(SystemMesCount, 90);
    EXPECT_FALSE(QolSkipSmallChest(0));
    EXPECT_EQ(status->item_pack.item[1], -1);
}

TEST_F(PickupQol, FullBagAndTrappedChestsKeepRetailSequence) {
    SetOptions(false, true);
    Chest();
    status->item_pack.num = 0;
    EXPECT_FALSE(QolSkipSmallChest(0));
    EXPECT_EQ(map->boxes[0].closed, 1);
    status->item_pack.num = 10;
    map->boxes[0].trap_no = 1;
    EXPECT_FALSE(QolSkipSmallChest(0));
    EXPECT_EQ(map->events[0].kind, DNG_EVENT_TREASURE_BOX);
    map->boxes[0].trap_no = 0;
    map->boxes[0].kind = TREASURE_BOX_LARGE;
    EXPECT_FALSE(QolSkipSmallChest(0));
}

TEST_F(PickupQol, FullAttachmentsRetainChestAndMapBypassesFullBag) {
    SetOptions(false, true);
    Chest(ITEM_ATTACH_PERIDOT);
    for (auto &item : status->consumable_items) item.id = ITEM_ATTACH_PERIDOT;
    EXPECT_FALSE(QolSkipSmallChest(0));
    EXPECT_EQ(map->boxes[0].closed, 1);
    const int old_map_flag = BtEquipMap;
    const int old_crystal_flag = BtEquipMasuisyou;
    status->item_pack.num = 0;
    BtEquipMap = 0;
    Chest(ITEM_MAP);
    EXPECT_TRUE(QolSkipSmallChest(0));
    EXPECT_EQ(BtEquipMap, 1);
    BtEquipMasuisyou = 0;
    Chest(ITEM_MAGICAL_CRYSTAL);
    EXPECT_TRUE(QolSkipSmallChest(0));
    EXPECT_EQ(BtEquipMasuisyou, 1);
    EXPECT_EQ(status->item_pack.item[0], -1);
    BtEquipMap = old_map_flag;
    BtEquipMasuisyou = old_crystal_flag;
}

TEST_F(PickupQol, AtlaAwardsChipAndUpdatesFloorExactlyOnce) {
    SetOptions(true, false);
    Atla();
    ASSERT_TRUE(QolSkipAtla(0, 0));
    EXPECT_EQ(save->GetElemData(0)[0], 0);
    EXPECT_EQ(status->atra_grid[0][0][0], -3);
    EXPECT_EQ(map->atra[0].used, 0);
    EXPECT_EQ(map->events[1].kind, DNG_EVENT_NONE);
    EXPECT_EQ(SystemMesInputKey, 0);
    EXPECT_EQ(SystemMesCount, 90);
    EXPECT_FALSE(QolSkipAtla(0, 0));
    EXPECT_EQ(save->GetElemData(0)[1], -1);
}

TEST_F(PickupQol, AtlaStillRequiresToanAndValidEvent) {
    SetOptions(true, true);
    Atla();
    status->cur_chara = CHARA_XIAO;
    EXPECT_FALSE(QolSkipAtla(0, 0));
    EXPECT_EQ(map->atra[0].used, 1);
    status->cur_chara = CHARA_TOAN;
    EXPECT_FALSE(QolSkipAtla(-1, 0));
    iventActive = -1;
    EXPECT_FALSE(QolSkipSmallChest(0));
    EXPECT_FALSE(QolSkipAtla(0, 0));
}
