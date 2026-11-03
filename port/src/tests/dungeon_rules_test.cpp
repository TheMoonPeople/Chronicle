// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#include "dun/gameloop.hpp"
#include "dungeon/doors.hpp"
#include "dungeon/rules.hpp"
#include "dungeoneventdata.hpp"
#include "dungeonparts.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "menu_save.hpp"
#include "platform/datapacks.hpp"
#include "savedata.hpp"
#include "tests/platform_fixture.hpp"
#include "userstatus.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

// A data folder with a datapacks folder beside it.
class DungeonRules : public ::testing::Test {
protected:
    fs::path directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_dungeon_{}", ProcessId());
    fs::path data = directory / "data";

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
    }

    void TearDown() override {
        datapacks::Clear();
        item::Load();
        dungeon::LoadRules();
        item::Clear();
        fs::remove_all(directory);
    }

    void Pack(const std::string &pack, const std::string &path, const nlohmann::json &document) const {
        const fs::path file = directory / "datapacks" / pack / "gameplay" / path;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << document.dump();
    }

    void Order(const std::string &packs) const {
        fs::create_directories(directory / "datapacks");
        std::ofstream(directory / "datapacks/datapacks.json") << R"({"schema_version":1,"load_order":[)" + packs + "]}";
        datapacks::Load(data);
        item::Load();
        dungeon::LoadRules();
    }

    static nlohmann::json RulesOn() {
        return {
            {"schema_version",         2              },
            {"kind",                   "dungeon_rules"},
            {"key_item_filter",        true           },
            {"auto_open_locked_doors", true           },
        };
    }

    static const item::DungeonKey *KeyOf(int number) {
        const item::Def *def = item::Find(number);
        return def != nullptr ? def->Get<item::DungeonKey>() : nullptr;
    }
};

TEST_F(DungeonRules, WithoutADocumentEveryChangeIsOffAsRetailPlays) {
    Order("");
    EXPECT_FALSE(dungeon::GetRules().key_item_filter);
    EXPECT_FALSE(dungeon::GetRules().auto_open_locked_doors);
}

TEST_F(DungeonRules, APackTurnsThemOn) {
    Pack("rules", "dungeon/rules.json", RulesOn());
    Order(R"("rules")");
    EXPECT_TRUE(dungeon::GetRules().key_item_filter);
    EXPECT_TRUE(dungeon::GetRules().auto_open_locked_doors);
}

TEST_F(DungeonRules, TheExportIsEveryChangeOff) {
    ASSERT_TRUE(dungeon::ExportRules(directory / "exported"));
    const auto document = nlohmann::json::parse(std::ifstream(directory / "exported/gameplay/dungeon/rules.json"));
    EXPECT_EQ(document["kind"], "dungeon_rules");
    EXPECT_EQ(document["key_item_filter"], false);
    EXPECT_EQ(document["auto_open_locked_doors"], false);
}

TEST_F(DungeonRules, EachDungeonsKeysHaveTheirTrait) {
    Order("");
    ASSERT_NE(KeyOf(ITEM_BONE_KEY), nullptr);
    EXPECT_EQ(KeyOf(ITEM_BONE_KEY)->opens, item::DungeonKey::Opens::LockedDoor);
    EXPECT_EQ(KeyOf(ITEM_BONE_KEY)->dungeon, 0);
    ASSERT_NE(KeyOf(ITEM_SILVER_KEY), nullptr);
    EXPECT_EQ(KeyOf(ITEM_SILVER_KEY)->dungeon, 6);
    ASSERT_NE(KeyOf(ITEM_RED_BERRY), nullptr);
    EXPECT_EQ(KeyOf(ITEM_RED_BERRY)->opens, item::DungeonKey::Opens::FloorExit);
    EXPECT_EQ(KeyOf(ITEM_RED_BERRY)->dungeon, 1);
    ASSERT_NE(KeyOf(ITEM_ROTTEN_FISH), nullptr);
    EXPECT_EQ(KeyOf(ITEM_ROTTEN_FISH)->opens, item::DungeonKey::Opens::BackFloor);
    EXPECT_EQ(KeyOf(ITEM_ROTTEN_FISH)->dungeon, 2);
    EXPECT_EQ(KeyOf(ITEM_BREAD), nullptr);
    EXPECT_EQ(item::With<item::DungeonKey>().size(), 7u + 9u + 8u);
}

TEST_F(DungeonRules, APacksKeysReplaceTheDungeonsAndAnUnknownItemIsLeftOut) {
    Pack("keys", "items/dungeon_keys.json", {
                                                {"schema_version", 2                                                                            },
                                                {"kind",           "dungeon_keys"                                                               },
                                                {"dungeons",       {{{"dungeon", 0}, {"locked_door", {"vanilla:treasure_key", "nobody:ghost"}}}}},
    });
    Order(R"("keys")");
    ASSERT_NE(KeyOf(ITEM_TREASURE_KEY), nullptr);
    EXPECT_EQ(KeyOf(ITEM_TREASURE_KEY)->opens, item::DungeonKey::Opens::LockedDoor);
    EXPECT_EQ(KeyOf(ITEM_BONE_KEY), nullptr);
    EXPECT_EQ(item::With<item::DungeonKey>().size(), 1u);
}

TEST_F(DungeonRules, AKeysDocumentRetailCannotReadLeavesTheDungeonsAsCompiled) {
    Pack("seventh", "items/dungeon_keys.json", {
                                                   {"schema_version", 2                                                            },
                                                   {"kind",           "dungeon_keys"                                               },
                                                   {"dungeons",       {{{"dungeon", 7}, {"locked_door", {"vanilla:treasure_key"}}}}},
    });
    Pack("twice", "items/dungeon_keys.json", {
                                                 {"schema_version", 2                                                                                              },
                                                 {"kind",           "dungeon_keys"                                                                                 },
                                                 {"dungeons",       {{{"dungeon", 0}, {"locked_door", {"vanilla:bone_key"}}, {"floor_exit", {"vanilla:bone_key"}}}}},
    });
    Order(R"("seventh")");
    EXPECT_NE(KeyOf(ITEM_SILVER_KEY), nullptr);
    Order(R"("twice")");
    EXPECT_NE(KeyOf(ITEM_SILVER_KEY), nullptr);
}

// The first dungeon's floor with a locked room door, whose event the door's map part placed, and Toan's bag.
class DungeonDoor : public DungeonRules {
protected:
    alignas(64) static inline CSaveData save;
    CSaveData        *saved_data = nullptr;
    CUserStatus      *saved_status = nullptr;
    s32               saved_dungeon = 0;
    CDungeonEvent     door_event{};
    CDungeonEventData door{};
    int               usable[2] = {ITEM_BONE_KEY, -1};

    void SetUp() override {
        DungeonRules::SetUp();
        saved_data = SaveData;
        saved_status = UserStatus;
        saved_dungeon = selectMapNo;
        save.Initialize();
        SaveData = &save;
        UserStatus = save.GetDngStatus();
        UserStatus->item_pack.num = 10;
        selectMapNo = 0;
        door_event.parts_id = MAP_PARTS_ROOM_GATE_SOUTH;
        door.event = &door_event;
        door.hold = 1;
    }

    void TearDown() override {
        SaveData = saved_data;
        UserStatus = saved_status;
        selectMapNo = saved_dungeon;
        DungeonRules::TearDown();
    }
};

TEST_F(DungeonDoor, CrossAtALockedDoorUsesItsKeyOnlyWhenTurnedOn) {
    UserStatus->item_pack.item[3] = ITEM_BONE_KEY;
    int key = -1;

    Order("");
    EXPECT_FALSE(dungeon::AutoOpenLockedDoor(door));
    EXPECT_FALSE(dungeon::TakeAutoKey(usable, true, key));

    Pack("rules", "dungeon/rules.json", RulesOn());
    Order(R"("rules")");
    ASSERT_TRUE(dungeon::AutoOpenLockedDoor(door));
    ASSERT_TRUE(dungeon::TakeAutoKey(usable, true, key));
    EXPECT_EQ(key, ITEM_BONE_KEY);
    // The door's window uses its key up, as choosing it does.
    EXPECT_EQ(UserStatus->item_pack.item[3], 0);
    // Set aside once: the next window asks again.
    EXPECT_FALSE(dungeon::TakeAutoKey(usable, true, key));
}

TEST_F(DungeonDoor, WithoutTheKeyOrAtAnotherEventCrossDoesAsRetail) {
    Pack("rules", "dungeon/rules.json", RulesOn());
    Order(R"("rules")");
    EXPECT_FALSE(dungeon::AutoOpenLockedDoor(door));

    UserStatus->item_pack.item[0] = ITEM_BONE_KEY;
    // A floor's exit and a back floor's entrance take items too, but they are not doors.
    CDungeonEvent exit_event = door_event;
    exit_event.parts_id = 31;
    CDungeonEventData exit = door;
    exit.event = &exit_event;
    EXPECT_FALSE(dungeon::AutoOpenLockedDoor(exit));
    CDungeonEventData free_slot = door;
    free_slot.hold = 0;
    EXPECT_FALSE(dungeon::AutoOpenLockedDoor(free_slot));
    // Another dungeon's key opens nothing here.
    UserStatus->item_pack.item[0] = ITEM_MOUSTACHE_KEY;
    EXPECT_FALSE(dungeon::AutoOpenLockedDoor(door));
}

TEST_F(DungeonDoor, TheKeyGoesOnlyToAWindowTakingIt) {
    UserStatus->item_pack.item[0] = ITEM_BONE_KEY;
    Pack("rules", "dungeon/rules.json", RulesOn());
    Order(R"("rules")");
    int       key = -1;
    const int gate[2] = {ITEM_DRAN_S_CREST, -1};
    ASSERT_TRUE(dungeon::AutoOpenLockedDoor(door));
    EXPECT_FALSE(dungeon::TakeAutoKey(gate, true, key));
    EXPECT_EQ(UserStatus->item_pack.item[0], ITEM_BONE_KEY);
}

TEST_F(DungeonRules, AnEventsWindowCanListOnlyWhatItTakes) {
    int       usable[3] = {ITEM_BONE_KEY, ITEM_DRAN_S_CREST, -1};
    ITEM_PACK pack{};
    InitEventItemSelect(0x18, usable, &pack, 0xB4, 0xD2, 1, EVENT_ITEM_MODE_USABLE);
    EXPECT_TRUE(EventItemListed(ITEM_BONE_KEY, EVENT_ITEM_MODE_USABLE));
    EXPECT_TRUE(EventItemListed(ITEM_DRAN_S_CREST, EVENT_ITEM_MODE_USABLE));
    EXPECT_FALSE(EventItemListed(ITEM_BREAD, EVENT_ITEM_MODE_USABLE));
    EXPECT_TRUE(EventItemListed(ITEM_BREAD, EVENT_ITEM_MODE_EVENT));
}

} // namespace

} // namespace dc::test
