// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "dun/gameloop.hpp"
#include "dungeon/loot.hpp"
#include "dungeonparts.hpp"
#include "editmenu.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "platform/datapacks.hpp"
#include "savedata.hpp"
#include "tests/platform_fixture.hpp"
#include "userstatus.hpp"

namespace dc::game {
extern int floorNum[7]; // dungeonparts.cpp: the floor dividing each dungeon's lower lists from its upper
} // namespace dc::game

namespace dc::test {

using namespace dc::game;
using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

bool Same(const dungeon::DungeonLoot &a, const dungeon::DungeonLoot &b) {
    const auto same_clown = [](const dungeon::ClownLists &x, const dungeon::ClownLists &y) {
        return x.lower == y.lower && x.upper == y.upper;
    };
    for (int back = 0; back < 2; back++) {
        if (a.chests[back].size() != b.chests[back].size() || !same_clown(a.clown[back], b.clown[back])) {
            return false;
        }
        for (std::size_t i = 0; i < a.chests[back].size(); i++) {
            if (a.chests[back][i].floor != b.chests[back][i].floor || a.chests[back][i].items != b.chests[back][i].items) {
                return false;
            }
        }
    }
    return a.turned_down == b.turned_down;
}

// Retail's draws as dungeonparts.cpp had them before the loot documents, on the compiled tables: the
// documents' draws must match them, random number for random number.
namespace retail {

float ScaledRand(float range) {
    return (range * static_cast<float>(std::rand())) / 2.1474836e9f;
}

int RandBelow(float range) {
    return static_cast<int>(ScaledRand(range));
}

int PickPieroItem(const PIERO_ITEM_SET &set, int list_no, const s16 *rate) {
    const int count = set.count[list_no];
    int       pick = -1;
    while (pick == -1) {
        pick = RandBelow(static_cast<float>(count));
        if (pick >= count) {
            pick = 0;
        }
        const int chance = RandBelow(100.0f);
        if (rate[set.item[list_no][pick] - 1] >= chance) {
            pick = -1;
        }
    }
    return set.item[list_no][pick];
}

void GetPieroItem(int map_no, int ura_dungeon, int *item0, int *item1) {
    const PIERO_ITEM_SET *list = PieroItemListPtr[map_no + ura_dungeon * 7];
    const s16            *rate = ItemSetRateTbl[map_no];
    const PIERO_ITEM_SET &set = UserStatus->cur_floor < floorNum[map_no] ? list[0] : list[1];
    *item0 = PickPieroItem(set, 0, rate);
    *item1 = PickPieroItem(set, 1, rate);
}

int DrawCandidate(const std::vector<int> &candidates, std::span<const s16> rates) {
    const int count = static_cast<int>(candidates.size());
    for (int tries = 0;;) {
        const float roll = (static_cast<float>(count) * static_cast<float>(rand())) / 2.1474836e9f;
        int         pick = static_cast<int>(roll);
        if (roll - static_cast<float>(pick) > 0.0f) {
            pick++;
        }
        if (pick < 0 || pick >= count) {
            pick = 0;
        }
        const int chance = static_cast<int>((100.0f * static_cast<float>(rand())) / 2.1474836e9f);
        const int item_no = candidates[pick];
        if (item_no <= 0 || item_no > 385) {
            return -1;
        }
        if (rates[item_no - 1] < chance) {
            return item_no;
        }
        if (++tries >= 0xFFFF) {
            return -1;
        }
    }
}

int PresetSmallItemNo_Get(int map_no, int floor_no, int special, int small) {
    constexpr int       kCounts[14] = {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
    const ITEM_PUT_SET *list = ItemPutListPtr[map_no + special * 7];
    const int           list_count = kCounts[map_no + special * 7];
    std::vector<s16>    rates(ItemSetRateTbl[map_no], ItemSetRateTbl[map_no] + 385);
    for (int item_no = ITEM_WEAPON_START; item_no < ITEM_WEAPON_START + 0x7B; item_no++) {
        const int held = GetNumHowManyItemsHave(item_no);
        s16      &rate = rates[item_no - 1];
        if (held > 0) {
            if (held == 1) {
                rate -= 10;
            }
            if (held >= 2) {
                rate -= 20;
            }
            rate = std::max<s16>(rate, 0);
        }
    }
    int wanted = -1;
    int scan = 0;
    for (; scan < list_count; scan++) {
        if (list[scan].floor == -1) {
            break;
        }
        if (floor_no + 1 == list[scan].floor) {
            wanted = floor_no + 1;
        }
    }
    if (scan == list_count) {
        return -1;
    }
    if (wanted == -1) {
        wanted = floor_no < floorNum[map_no] ? 0x100 : 0xFF;
    }
    int list_no = 0;
    while (wanted != list[list_no].floor) {
        if (++list_no >= list_count) {
            return -1;
        }
    }
    std::vector<int> candidates;
    for (const int item_no : std::span(list[list_no].item, list[list_no].item_count)) {
        const bool is_attachment = item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START;
        const bool is_weapon = item_no >= ITEM_WEAPON_START;
        if (small != 0 ? is_attachment : is_weapon) {
            candidates.push_back(item_no);
        }
    }
    if (candidates.empty()) {
        return -1;
    }
    return DrawCandidate(candidates, rates);
}

} // namespace retail

// A data folder with a datapacks folder beside it.
class DungeonLoot : public ::testing::Test {
protected:
    alignas(64) static inline CSaveData save;
    fs::path     directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_loot_{}", ProcessId());
    fs::path     data = directory / "data";
    CSaveData   *saved_data = nullptr;
    CUserStatus *saved_status = nullptr;

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
        saved_data = SaveData;
        saved_status = UserStatus;
        save.Initialize();
        SaveData = &save;
        UserStatus = save.GetDngStatus();
    }

    void TearDown() override {
        SaveData = saved_data;
        UserStatus = saved_status;
        datapacks::Clear();
        item::Load();
        dungeon::LoadLoot();
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
        dungeon::LoadLoot();
    }

    static nlohmann::json Patch(const nlohmann::json &rows) {
        return {
            {"schema_version", 2                   },
            {"kind",           "dungeon_loot_patch"},
            {"dungeons",       rows                }
        };
    }
};

TEST_F(DungeonLoot, TheCompiledTablesAreRetails) {
    Order("");
    const dungeon::DungeonLoot &first = dungeon::Loot(0);
    ASSERT_EQ(first.chests[0].size(), 3u);
    EXPECT_EQ(first.chests[0][0].floor, dungeon::kLowerFloors);
    EXPECT_EQ(first.chests[0][1].floor, dungeon::kUpperFloors);
    EXPECT_EQ(first.chests[0][2].floor, 11);
    EXPECT_EQ(first.chests[0][0].items.size(), 63u);
    EXPECT_EQ(first.chests[1].size(), 2u);
    for (int dungeon = 0; dungeon < 7; dungeon++) {
        for (const int item : dungeon::Loot(dungeon).chests[0][0].items) {
            EXPECT_EQ(dungeon::TurnedDown(dungeon, item), ItemSetRateTbl[dungeon][item - 1]) << dungeon << " " << item;
        }
    }
}

TEST_F(DungeonLoot, WithoutPacksBoxesAndTheClownDrawAsRetailDid) {
    Order("");
    // A held Dagger makes it rarer, as retail's draw has it.
    UserStatus->chara_weapons[0][0].item_no = ITEM_WEAPON_DAGGER;
    for (int dungeon = 0; dungeon < 7; dungeon++) {
        for (int floor = 0; floor < 20; floor++) {
            for (int special = 0; special < 2; special++) {
                for (int small = 0; small < 2; small++) {
                    for (unsigned seed = 1; seed <= 8; seed++) {
                        std::srand(seed);
                        const int expected = retail::PresetSmallItemNo_Get(dungeon, floor, special, small);
                        std::srand(seed);
                        EXPECT_EQ(PresetSmallItemNo_Get(dungeon, floor, special, small), expected)
                            << dungeon << " floor " << floor << " special " << special << " small " << small;
                    }
                }
            }
            UserStatus->cur_floor = static_cast<s16>(floor);
            for (int back = 0; back < 2; back++) {
                std::srand(dungeon * 100 + floor);
                int expected[2];
                retail::GetPieroItem(dungeon, back, &expected[0], &expected[1]);
                std::srand(dungeon * 100 + floor);
                int got[2];
                GetPieroItem(dungeon, back, &got[0], &got[1]);
                EXPECT_EQ(got[0], expected[0]) << dungeon << " floor " << floor;
                EXPECT_EQ(got[1], expected[1]) << dungeon << " floor " << floor;
            }
        }
    }
}

TEST_F(DungeonLoot, TheExportLoadsBackAsCompiled) {
    Order("");
    std::array<dungeon::DungeonLoot, 7> compiled;
    for (int dungeon = 0; dungeon < 7; dungeon++) {
        compiled[dungeon] = dungeon::Loot(dungeon);
    }
    ASSERT_TRUE(dungeon::ExportLoot(directory / "datapacks/exported"));
    Order(R"("exported")");
    for (int dungeon = 0; dungeon < 7; dungeon++) {
        EXPECT_TRUE(Same(dungeon::Loot(dungeon), compiled[dungeon])) << dungeon;
    }
}

TEST_F(DungeonLoot, TheTrackedDocumentIsTheExport) {
    ASSERT_TRUE(dungeon::ExportLoot(directory / "exported"));
    const fs::path tracked = fs::path(__FILE__).parent_path().parent_path().parent_path() / "gameplay/dungeon/loot.json";
    EXPECT_EQ(nlohmann::json::parse(std::ifstream(tracked)), nlohmann::json::parse(std::ifstream(directory / "exported/gameplay/dungeon/loot.json")));
}

TEST_F(DungeonLoot, APatchReplacesAFieldAndAddsChancesAndPacksCombine) {
    Pack("bread", "dungeon/loot.patch.json",
         Patch({
             {{"dungeon", 0}, {"chests", {{{"floor", "lower"}, {"items", {"vanilla:bread"}}}}}, {"turned_down", {{"vanilla:bread", 30}}}}
    }));
    Pack("clown", "dungeon/loot.patch.json",
         Patch({
             {{"dungeon", 3}, {"clown", {{"lower", {{"vanilla:cheese"}, {"vanilla:bread"}}}, {"upper", {{"vanilla:cheese"}, {"vanilla:bread"}}}}}}
    }));
    Order("");
    const int kept_item = dungeon::Loot(0).chests[1][0].items[0];
    const int kept_chance = dungeon::TurnedDown(0, kept_item);
    Order(R"("bread","clown")");
    ASSERT_EQ(dungeon::Loot(0).chests[0][0].items, std::vector<int>{ITEM_BREAD});
    EXPECT_EQ(dungeon::TurnedDown(0, ITEM_BREAD), 30);
    // A list replaces the field it is in; the dungeon's other fields and chances are as they were.
    EXPECT_EQ(dungeon::Loot(0).chests[0].size(), 1u);
    EXPECT_EQ(dungeon::Loot(0).chests[1].size(), 2u);
    EXPECT_EQ(dungeon::TurnedDown(0, kept_item), kept_chance);
    EXPECT_EQ(dungeon::Loot(3).clown[0].lower[0], std::vector<int>{ITEM_CHEESE});
}

TEST_F(DungeonLoot, ABoxAndTheClownDrawFromTheDocuments) {
    Pack("one", "dungeon/loot.patch.json",
         Patch({
             {{"dungeon", 0},
              {"chests", {{{"floor", "lower"}, {"items", {"vanilla:bread", "vanilla:weapon_dagger"}}}}},
              {"clown", {{"lower", {{"vanilla:cheese"}, {"vanilla:bread"}}}, {"upper", {{"vanilla:cheese"}, {"vanilla:bread"}}}}},
              {"turned_down", {{"vanilla:bread", 0}, {"vanilla:cheese", 0}, {"vanilla:weapon_dagger", 0}}}}
    }));
    Order(R"("one")");
    std::srand(7);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, 0, 1), ITEM_BREAD);
    EXPECT_EQ(PresetSmallItemNo_Get(0, 0, 0, 0), ITEM_WEAPON_DAGGER);
    UserStatus->cur_floor = 0;
    int first = 0;
    int second = 0;
    GetPieroItem(0, 0, &first, &second);
    EXPECT_EQ(first, ITEM_CHEESE);
    EXPECT_EQ(second, ITEM_BREAD);
}

TEST_F(DungeonLoot, AnItemNoPackDefinesIsLeftOutAndABadPatchIsRefusedWhole) {
    Pack("ghost", "dungeon/loot.patch.json",
         Patch({
             {{"dungeon", 1}, {"chests", {{{"floor", "lower"}, {"items", {"vanilla:bread", "nobody:ghost"}}}}}}
    }));
    Pack("bad", "dungeon/loot.patch.json",
         Patch({
             {{"dungeon", 2}, {"chests", {{{"floor", "lower"}, {"items", {"vanilla:bread"}}}}}},
             {{"dungeon", 2}, {"turned_down", {{"vanilla:bread", 101}}}                       }
    }));
    Order("");
    const std::vector<int> retail = dungeon::Loot(2).chests[0][0].items;
    Order(R"("ghost","bad")");
    EXPECT_EQ(dungeon::Loot(1).chests[0][0].items, std::vector<int>{ITEM_BREAD});
    EXPECT_EQ(dungeon::Loot(2).chests[0][0].items, retail);
}

} // namespace

} // namespace dc::test
