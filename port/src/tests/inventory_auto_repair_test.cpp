#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include "menu_save.hpp"
#include "dngmessageman.hpp"
#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "mainitemmodel.hpp"
#include "userstatus.hpp"
#include "options/rows.hpp"
#include "platform/config.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"

class InventoryAutoRepair : public testing::Test {
protected:
    std::unique_ptr<CUserStatus> status = std::make_unique<CUserStatus>();
    CUserStatus *before_status = UserStatus;
    WEAPON_HAVE *before_weapon = NowWeaponHave;
    Config before_config = ConfigGet();
    std::filesystem::path before_root = PathsSaveRoot();
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("dc-auto-repair-" + std::to_string(dc::test::ProcessId()));

    void SetUp() override {
        PathsSetSaveRoot(root);
        UserStatus = status.get();
        NowWeaponHave = &status->chara_weapons[0][0];
        NowWeaponHave->item_no = ITEM_WEAPON_BASELARD;
        NowWeaponHave->durability = 100;
        NowWeaponHave->durability_f = 0.5f;
        status->item_pack.num = 10;
        std::fill_n(status->item_pack.item, 103, -1);
        std::fill_n(status->item_pack.quick_item_slot, 3, -1);
        for (int i = 1; i <= 3; ++i) activeItem.model[i] = -1;
        SetEnabled(false);
    }
    void SetEnabled(bool value) {
        Config config = ConfigGet();
        config.inventory_auto_repair = value;
        ASSERT_TRUE(ConfigChange(config));
    }
    void TearDown() override {
        ConfigChange(before_config);
        PathsSetSaveRoot(before_root);
        UserStatus = before_status;
        NowWeaponHave = before_weapon;
        std::error_code error;
        std::filesystem::remove(root / "config.json", error);
        std::filesystem::remove(root, error);
    }
};

TEST_F(InventoryAutoRepair, DisabledLeavesBagPowderAndWeaponReachesZero) {
    status->item_pack.item[0] = ITEM_AUTO_REPAIR_POWDER;
    BattleSubWeaponDmg(1.0f, 0);
    EXPECT_FLOAT_EQ(NowWeaponHave->durability_f, 0.0f);
    EXPECT_EQ(status->item_pack.item[0], ITEM_AUTO_REPAIR_POWDER);
}

TEST_F(InventoryAutoRepair, EnabledConsumesExactlyOneAndKeepsRepairFeedback) {
    SetEnabled(true);
    status->item_pack.item[0] = ITEM_AUTO_REPAIR_POWDER;
    status->item_pack.item[7] = ITEM_AUTO_REPAIR_POWDER;
    BattleSubWeaponDmg(1.0f, 0);
    EXPECT_FLOAT_EQ(NowWeaponHave->durability_f, 100.0f);
    EXPECT_EQ(status->item_pack.item[0], -1);
    EXPECT_EQ(status->item_pack.item[7], ITEM_AUTO_REPAIR_POWDER);
    EXPECT_EQ(DngMessMan.message, 0xBC);
    EXPECT_EQ(DngMessMan.timer, 0xF0);
}

TEST_F(InventoryAutoRepair, QuickSlotsStillWorkInBothModesAndTakePriority) {
    for (bool enabled : {false, true}) {
        SetEnabled(enabled);
        for (int slot = 0; slot < 3; ++slot) {
            NowWeaponHave->durability_f = 0.5f;
            status->item_pack.item[0] = ITEM_AUTO_REPAIR_POWDER;
            status->item_pack.quick_item_slot[slot] = ITEM_AUTO_REPAIR_POWDER;
            status->item_pack.quick_item_qty[slot] = 1;
            BattleSubWeaponDmg(1.0f, 0);
            EXPECT_FLOAT_EQ(NowWeaponHave->durability_f, 100.0f);
            EXPECT_EQ(status->item_pack.quick_item_slot[slot], -1);
            EXPECT_EQ(status->item_pack.quick_item_qty[slot], 0);
            EXPECT_EQ(status->item_pack.item[0], ITEM_AUTO_REPAIR_POWDER);
        }
    }
}

TEST_F(InventoryAutoRepair, OrdinaryPowderDoesNotPreventBreak) {
    SetEnabled(true);
    status->item_pack.item[0] = ITEM_REPAIR_POWDER;
    BattleSubWeaponDmg(1.0f, 0);
    EXPECT_FLOAT_EQ(NowWeaponHave->durability_f, 0.0f);
    EXPECT_EQ(status->item_pack.item[0], ITEM_REPAIR_POWDER);
}

TEST_F(InventoryAutoRepair, NonBreakingWearDoesNotConsumePowder) {
    SetEnabled(true);
    status->item_pack.item[0] = ITEM_AUTO_REPAIR_POWDER;
    NowWeaponHave->durability_f = 100.0f;
    BattleSubWeaponDmg(1.0f, 0);
    EXPECT_FLOAT_EQ(NowWeaponHave->durability_f, 98.5f);
    EXPECT_EQ(status->item_pack.item[0], ITEM_AUTO_REPAIR_POWDER);
}

TEST(InventoryAutoRepairConfig, ToggleRoundTripsAndPageResetRestoresRetail) {
    Config config;
    EXPECT_FALSE(config.inventory_auto_repair);
    const auto pages = options::Pages();
    const auto page = std::find_if(pages.begin(), pages.end(),
        [](const options::Page &p) { return std::string_view(p.name) == "QOL"; });
    ASSERT_NE(page, pages.end());
    const auto row = std::find_if(page->rows.begin(), page->rows.end(),
        [](const options::Row &r) { return std::string_view(r.key) == "qol.inventory_auto_repair"; });
    ASSERT_NE(row, page->rows.end());
    EXPECT_EQ(options::RowValue(*row, config), "Off");
    ASSERT_TRUE(options::StepRow(*row, config, 1, false));
    EXPECT_TRUE(config.inventory_auto_repair);
    EXPECT_TRUE(ConfigParse(ConfigSerialize(config)).inventory_auto_repair);
    EXPECT_FALSE(ConfigParse(R"({"qol":{"inventory_auto_repair":"bad"}})").inventory_auto_repair);
    options::ResetPage(*page, config);
    EXPECT_FALSE(config.inventory_auto_repair);
}
