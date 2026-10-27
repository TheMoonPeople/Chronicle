// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "btitem.hpp"
#include "fishing.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "platform/datapacks.hpp"
#include "shop.hpp"
#include "tests/platform_fixture.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::platform;

namespace {

namespace fs = std::filesystem;

template <class T>
void Append(std::vector<unsigned char> &bytes, std::span<const T> table) {
    const auto *begin = reinterpret_cast<const unsigned char *>(table.data());
    bytes.insert(bytes.end(), begin, begin + table.size_bytes());
}

// Every table the documents fill, as bytes.
std::vector<unsigned char> Tables() {
    std::vector<unsigned char> bytes;
    Append<COM_ITEM_INFO>(bytes, ComItemInfo);
    Append<const char *>(bytes, ITEM_NAME_TBL_NEW);
    Append<ITEM_DATA>(bytes, ITEM_LIST);
    Append<ATTACH_DATA>(bytes, AttachList);
    Append<WEAPON_DATA>(bytes, WeaponList);
    Append<EsaInfo>(bytes, esa_info);
    Append<ITEM_MONEY_ENTRY>(bytes, PriceList);
    return bytes;
}

nlohmann::json &Row(nlohmann::json &document, const char *rows, int item) {
    for (auto &row : document[rows]) {
        if (row["item_no"] == item) {
            return row;
        }
    }
    throw std::runtime_error("no such row");
}

// A data folder with a datapacks folder beside it, and the exported documents to start packs from.
class ItemDocuments : public ::testing::Test {
protected:
    fs::path                   directory = fs::canonical(fs::temp_directory_path()) / std::format("dc_items_{}", ProcessId());
    fs::path                   data = directory / "data";
    std::vector<unsigned char> compiled = Tables();

    void SetUp() override {
        fs::remove_all(directory);
        fs::create_directories(data);
        ASSERT_TRUE(item::Export(directory / "exported"));
    }

    void TearDown() override {
        datapacks::Clear();
        item::Load();
        item::Clear();
        fs::remove_all(directory);
    }

    // The exported document at a path under gameplay/items/, to change and put in a pack.
    nlohmann::json Document(const std::string &path) const {
        return nlohmann::json::parse(std::ifstream(directory / "exported/gameplay/items" / path));
    }

    void Pack(const std::string &pack, const std::string &path, const nlohmann::json &document) const {
        const fs::path file = directory / "datapacks" / pack / "gameplay/items" / path;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << document.dump();
    }

    void Order(const std::string &packs) const {
        fs::create_directories(directory / "datapacks");
        std::ofstream(directory / "datapacks/datapacks.json") << R"({"schema_version":1,"load_order":[)" + packs + "]}";
        datapacks::Load(data);
        item::Load();
    }
};

TEST_F(ItemDocuments, TheExportHasEveryRowOfEveryTable) {
    EXPECT_EQ(Document("items.json")["items"].size(), 124u);
    EXPECT_EQ(Document("attachments.json")["attachments"].size(), 50u);
    EXPECT_EQ(Document("weapons.json")["weapons"].size(), 120u);
    EXPECT_EQ(Document("bait.json")["bait"].size(), esa_info.size());
    EXPECT_EQ(Document("item_common.json")["records"].size(), std::size(ComItemInfo));
    EXPECT_EQ(Document("prices.json")["prices"].size(), std::size(PriceList));
}

TEST_F(ItemDocuments, TheExportLoadsBackByteForByte) {
    fs::create_directories(directory / "datapacks");
    fs::rename(directory / "exported", directory / "datapacks/exported");
    Order(R"("exported")");
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(ItemDocuments, APacksChangeReachesTheTableAndLeavesWithIt) {
    auto prices = Document("prices.json");
    Row(prices, "prices", ITEM_PRICKLY)["buy_price"] = 1234;
    auto bait = Document("bait.json");
    Row(bait, "bait", ITEM_PRICKLY)["radius"] = 37.5;
    auto common = Document("item_common.json");
    Row(common, "records", ITEM_PRICKLY)["model"] = "atfire";
    Pack("lures", "prices.json", prices);
    Pack("lures", "bait.json", bait);
    Pack("lures", "item_common.json", common);
    Order(R"("lures")");
    EXPECT_EQ(PriceList[ITEM_PRICKLY - ITEM_ATTACH_START].buy_price, 1234);
    EXPECT_STREQ(ITEM_NAME_TBL_NEW[ITEM_PRICKLY - ITEM_ATTACH_START], "atfire");
    const auto prickly = std::ranges::find(esa_info, static_cast<int>(ITEM_PRICKLY), &EsaInfo::item_no);
    ASSERT_NE(prickly, esa_info.end());
    EXPECT_EQ(prickly->radius, 37.5f);

    Order("");
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(ItemDocuments, ADocumentRetailCannotReadIsRefusedWhole) {
    auto weapons = Document("weapons.json");
    Row(weapons, "weapons", ITEM_WEAPON_START)["owner"] = 3; // decides whose weapon list holds a copy
    Pack("owner", "weapons.json", weapons);
    auto common = Document("item_common.json");
    Row(common, "records", ITEM_PRICKLY)["index"] = 52;
    Pack("index", "item_common.json", common);
    auto prices = Document("prices.json");
    Row(prices, "prices", ITEM_PRICKLY)["sell_price"] = 1;
    Row(prices, "prices", ITEM_PRICKLY + 1)["buy_price"] = 40000; // past an s16
    Pack("range", "prices.json", prices);
    auto items = Document("items.json");
    items["items"].erase(items["items"].begin());
    Pack("missing", "items.json", items);
    auto attachments = Document("attachments.json");
    attachments["attachments"].push_back({
        {"item_no", ITEM_WEAPON_START}
    });
    Pack("extra", "attachments.json", attachments);
    auto bait = Document("bait.json");
    bait["schema_version"] = 3;
    Row(bait, "bait", ITEM_PRICKLY)["radius"] = 1.0;
    Pack("newer", "bait.json", bait);
    auto model = Document("item_common.json");
    Row(model, "records", ITEM_PRICKLY)["model"] = "../escape";
    Pack("model", "item_common.json", model);
    Order(R"("owner","index","range","missing","extra","newer","model")");
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(ItemDocuments, AnUnknownFieldIsReportedButNotRefused) {
    auto items = Document("items.json");
    Row(items, "items", ITEM_PRICKLY)["vol"] = 7;
    Row(items, "items", ITEM_PRICKLY)["sparkle"] = true;
    Pack("newer field", "items.json", items);
    Order(R"("newer field")");
    EXPECT_EQ(GetItemData(ITEM_PRICKLY)->vol, 7);
}

TEST_F(ItemDocuments, EveryVanillaItemIsRegisteredWithTheTraitsItsRowsGive) {
    Order("");
    const auto all = item::All();
    ASSERT_EQ(all.size(), std::size(ComItemInfo));
    std::set<std::string> ids;
    for (const item::Def *def : all) {
        EXPECT_TRUE(item::ValidId(def->id)) << def->id;
        EXPECT_TRUE(ids.insert(def->id).second) << def->id;
        const COM_ITEM_INFO    &info = ComItemInfo[def->number - ITEM_ATTACH_START];
        const ITEM_MONEY_ENTRY &price = PriceList[def->number - ITEM_ATTACH_START];
        ASSERT_NE(def->Get<item::Sellable>(), nullptr);
        EXPECT_EQ(def->Get<item::Sellable>()->buy, price.buy_price);
        EXPECT_EQ(def->Get<item::Sellable>()->sell, price.sell_price);
        if (info.kind == ITEMKIND_ITEM && info.index >= 0 && std::cmp_less(info.index, std::size(ITEM_LIST))) {
            const ITEM_DATA &row = ITEM_LIST[info.index];
            EXPECT_EQ(def->Has<item::Consumable>(), (row.kind_flags & ITEMKINDF_CONSUMABLE) != 0) << def->id;
            EXPECT_EQ(def->Has<item::Throwable>(), (row.kind_flags & ITEMKINDF_THROWABLE) != 0) << def->id;
            EXPECT_EQ(def->Has<item::QuickSlot>(), row.stack_kind != ITEM_STACK_NEVER) << def->id;
            EXPECT_EQ(def->Has<item::Fish>(), row.sort_key == ITEM_SORT_FISH) << def->id;
            EXPECT_EQ(def->Has<item::KeyItem>(), row.sort_key == ITEM_SORT_KEY_ITEM) << def->id;
            EXPECT_EQ(def->Has<item::QuestItem>(), row.sort_key == ITEM_SORT_QUEST) << def->id;
        } else if (info.kind == ITEMKIND_WEAPON && info.index >= 0 && std::cmp_less(info.index, std::size(WeaponList))) {
            ASSERT_NE(def->Get<item::Weapon>(), nullptr) << def->id;
            EXPECT_EQ(def->Get<item::Weapon>()->owner, WeaponList[info.index].owner);
            EXPECT_EQ(def->Get<item::Weapon>()->attack, WeaponList[info.index].attack);
        } else if (info.kind == ITEMKIND_ATTACH && info.index >= 0 && std::cmp_less(info.index, std::size(AttachList))) {
            ASSERT_NE(def->Get<item::Attachment>(), nullptr) << def->id;
            EXPECT_EQ(def->Get<item::Attachment>()->attack, AttachList[info.index].attack);
        }
    }
    EXPECT_EQ(item::With<item::Bait>().size(), esa_info.size());
    EXPECT_EQ(item::Find("vanilla:bread")->number, ITEM_BREAD);
    EXPECT_EQ(item::Find("vanilla:weapon_bone_slingshot")->number, ITEM_WEAPON_BONE_SLINGSHOT);
    EXPECT_EQ(item::Find("vanilla:attach_fire")->number, ITEM_ATTACH_FIRE);
    EXPECT_TRUE(item::Find("vanilla:prickly")->Has<item::Bait>());
    // A number the decomp has not named.
    EXPECT_EQ(item::Find(86)->id, "vanilla:item_86");
}

TEST_F(ItemDocuments, APacksChangesReachTheRegistryAndLeaveWithThem) {
    auto items = Document("items.json");
    Row(items, "items", ITEM_PRICKLY)["stack_kind"] = ITEM_STACK_MULTI; // never, in retail
    auto prices = Document("prices.json");
    Row(prices, "prices", ITEM_BREAD)["buy_price"] = 1;
    Pack("bakery", "items.json", items);
    Pack("bakery", "prices.json", prices);
    Order(R"("bakery")");
    EXPECT_EQ(item::Find("vanilla:bread")->Get<item::Sellable>()->buy, 1);
    EXPECT_TRUE(item::Find("vanilla:prickly")->Has<item::QuickSlot>());

    Order("");
    EXPECT_EQ(item::Find("vanilla:bread")->Get<item::Sellable>()->buy, PriceList[ITEM_BREAD - ITEM_ATTACH_START].buy_price);
    EXPECT_FALSE(item::Find("vanilla:prickly")->Has<item::QuickSlot>());
    EXPECT_EQ(Tables(), compiled);
}

} // namespace

} // namespace dc::test
