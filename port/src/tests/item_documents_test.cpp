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
#include "btmisc.hpp"
#include "clsmes.hpp"
#include "data/data_read.hpp"
#include "data_fixture.hpp"
#include "dngstatusdata.hpp"
#include "fishing.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "platform/datapacks.hpp"
#include "platform/paths.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "tests/platform_fixture.hpp"
#include "text/gametext.hpp"

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

TEST_F(ItemDocuments, TheTrackedDocumentsHoldTheCompiledTables) {
    // port/gameplay/items, which the build seals into its gameplay seed and dcdata writes into the data.
    const fs::path        tracked = fs::path(__FILE__).parent_path().parent_path().parent_path() / "gameplay/items";
    std::set<std::string> exported;
    for (const fs::directory_entry &entry : fs::directory_iterator(directory / "exported/gameplay/items")) {
        exported.insert(entry.path().filename().string());
    }
    std::set<std::string> files;
    for (const fs::directory_entry &entry : fs::directory_iterator(tracked)) {
        files.insert(entry.path().filename().string());
    }
    EXPECT_EQ(files, exported);
    for (const std::string &name : exported) {
        EXPECT_EQ(nlohmann::json::parse(std::ifstream(tracked / name)), Document(name)) << name;
    }
}

TEST_F(ItemDocuments, TheDatasOwnDocumentIsReadUnderThePacks) {
    const datafix::Disc disc = datafix::MakeDisc({
        {"dun/pack/maindat.pac", datafix::Pattern(64, 1)}
    });
    datafix::WriteBytes(data / "data.hd2", disc.hd2);
    datafix::WriteBytes(data / "dun/pack/maindat.pac", disc.files[0].data);
    auto prices = Document("prices.json");
    Row(prices, "prices", ITEM_PRICKLY)["buy_price"] = 20;
    fs::create_directories(data / "gameplay/items");
    std::ofstream(data / "gameplay/items/prices.json") << prices.dump();
    PathsSetDataRoot(data);
    PathsSetSaveRoot(directory / "save");
    data::DataReadIndex();
    Order("");
    EXPECT_EQ(PriceList[ITEM_PRICKLY - ITEM_ATTACH_START].buy_price, 20);

    Row(prices, "prices", ITEM_PRICKLY)["buy_price"] = 30;
    Pack("bakery", "prices.json", prices);
    Order(R"("bakery")");
    EXPECT_EQ(PriceList[ITEM_PRICKLY - ITEM_ATTACH_START].buy_price, 30);
    data::DataReadReset();
}

// A custom bag item's document, as a pack ships it.
nlohmann::json CustomItem(const std::string &id) {
    return {
        {"schema_version", 2                                                                                                                       },
        {"kind",           "item"                                                                                                                  },
        {"id",             id                                                                                                                      },
        {"sort_key",       ITEM_SORT_GOODS                                                                                                         },
        {"stack_kind",     ITEM_STACK_SINGLE                                                                                                       },
        {"buy_price",      120                                                                                                                     },
        {"sell_price",     60                                                                                                                      },
        {"icon",           56                                                                                                                      },
        {"model",          "turisao"                                                                                                               },
        {"text",           {{"en_gb", {{"name", "Rod"}, {"description", "A rod."}}}, {"fr_fr", {{"name", "Canne"}, {"description", "Une canne."}}}}}
    };
}

TEST_F(ItemDocuments, ACustomItemGetsANumberAboveRetailsAndRowsOfItsOwn) {
    Pack("rods", "custom/chronicle.rod_2.json", CustomItem("chronicle:rod_2"));
    Order(R"("rods")");
    const item::Def *rod = item::Find("chronicle:rod_2");
    ASSERT_NE(rod, nullptr);
    EXPECT_EQ(rod->number, item::kCustomFirst);
    EXPECT_EQ(item::Find(item::kCustomFirst), rod);
    ASSERT_NE(rod->Get<item::Sellable>(), nullptr);
    EXPECT_EQ(rod->Get<item::Sellable>()->buy, 120);
    ASSERT_NE(rod->Get<item::QuickSlot>(), nullptr);

    const COM_ITEM_INFO *info = GetCommonItemInfo(rod->number);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->kind, ITEMKIND_ITEM);
    EXPECT_EQ(info->icon_index, 56);
    EXPECT_EQ(info->msg, rod->number);
    ASSERT_NE(GetItemData(rod->number), nullptr);
    EXPECT_EQ(GetItemData(rod->number)->sort_key, ITEM_SORT_GOODS);
    EXPECT_STREQ(item::CustomModel(rod->number), "turisao");
    EXPECT_EQ(item::CustomText(rod->number, "fr_fr")->name, "Canne");
    EXPECT_EQ(item::CustomText(rod->number, "de_de")->name, "Rod");
    EXPECT_EQ(GetCommonItemInfo(rod->number + 1), nullptr);

    Order("");
    EXPECT_EQ(item::Find("chronicle:rod_2"), nullptr);
    EXPECT_EQ(GetCommonItemInfo(item::kCustomFirst), nullptr);
    EXPECT_EQ(Tables(), compiled);
}

TEST_F(ItemDocuments, ACustomItemsNameAndDescriptionAnswerTheGamesLookups) {
    Pack("rods", "custom/chronicle.rod_2.json", CustomItem("chronicle:rod_2"));
    Order(R"("rods")");
    const int number = item::kCustomFirst;
    const int language = LanguageCode;
    LanguageCode = 3; // fr_fr

    // Names come from the system messages, descriptions from the menus' own; the files' own messages win.
    text::GameTextFile system;
    system.Set(1, "System");
    text::GameTextFile menu;
    menu.Set(number + 500, "Retail's");
    ClsMes mes;
    mes.SetBuff(menu.Data());
    mes.SetBuff_system(system.Data());
    EXPECT_EQ(text::GameTextDecode(mes.GetTextLineDataTop_system(number + 100)), "Canne");
    EXPECT_EQ(text::GameTextDecode(mes.GetTextLineDataTop(number + 500)), "Retail's");
    menu.Set(1, "Menu");
    mes.SetBuff(menu.Data());
    EXPECT_EQ(mes.GetTextLineDataTop(number + 500 + 1), nullptr);
    EXPECT_EQ(text::GameTextDecode(item::CustomMessage(number + 500, false)), "Une canne.");
    LanguageCode = 4; // de_de, which the item has no text for
    EXPECT_EQ(text::GameTextDecode(mes.GetTextLineDataTop_system(number + 100)), "Rod");
    EXPECT_EQ(mes.GetTextLineDataTop_system(number + 101), nullptr);
    LanguageCode = language;

    char model[64] = {};
    char texture[64] = {};
    BtGetItemNamePath(model, texture, number);
    EXPECT_STREQ(model, "dun/item/main_data/turisao.mds");
    EXPECT_STREQ(item::ModelName(ITEM_PRICKLY), ITEM_NAME_TBL_NEW[ITEM_PRICKLY - ITEM_ATTACH_START]);
    EXPECT_EQ(item::ModelName(ITEM_WEAPON_START + 1), nullptr);
}

alignas(64) CSaveData g_custom_save;

TEST_F(ItemDocuments, ACustomItemIsABagItemWhereRetailTellsKindsByNumber) {
    Pack("rods", "custom/chronicle.rod_2.json", CustomItem("chronicle:rod_2"));
    Order(R"("rods")");
    const int number = item::kCustomFirst;
    EXPECT_EQ(item::KindOf(number), item::Kind::Item);
    EXPECT_EQ(item::KindOf(number + 1), item::Kind::None);
    EXPECT_EQ(item::KindOf(0), item::Kind::None);
    EXPECT_EQ(item::KindOf(ITEM_ATTACH_PERIDOT), item::Kind::Attachment);
    EXPECT_EQ(item::KindOf(ITEM_PRICKLY), item::Kind::Item);
    EXPECT_EQ(item::KindOf(ITEM_WEAPON_START), item::Kind::Weapon);
    // Retail takes anything from ITEM_WEAPON_START up for a weapon; below the custom numbers it still does.
    EXPECT_EQ(item::KindOf(ITEM_WEAPON_END), item::Kind::Weapon);

    // Picked up in a dungeon, it goes into the bag; retail took it for a weapon and read past WeaponList.
    g_custom_save.Initialize();
    CDngStatusData &status = *g_custom_save.GetDngStatus();
    const int       slot = status.GetItem(number, 0);
    ASSERT_GE(slot, 0);
    EXPECT_EQ(status.item_pack.item[slot], number);
    EXPECT_EQ(status.SearchItemIndexNo(number), slot);
}

nlohmann::json KeptFish(const std::string &id) {
    nlohmann::json fish = CustomItem(id);
    fish["state"] = {
        {"size", 31.5}
    };
    fish["state_version"] = 1;
    return fish;
}

TEST_F(ItemDocuments, ACopyWithStateTravelsUnderAHandleThatAnswersAsItsItem) {
    item::ClearCopies();
    Pack("fish", "custom/chronicle.rod_2.json", CustomItem("chronicle:rod_2"));
    Pack("fish", "custom/chronicle.bass.json", KeptFish("chronicle:bass"));
    Order(R"("fish")");
    const item::Def *bass = item::Find("chronicle:bass");
    const item::Def *rod = item::Find("chronicle:rod_2");
    ASSERT_NE(bass, nullptr);
    ASSERT_NE(rod, nullptr);
    EXPECT_EQ(bass->state_version, 1);
    EXPECT_EQ(item::NewCopy(*rod), -1);

    const int first = item::NewCopy(*bass);
    const int second = item::NewCopy(*bass);
    EXPECT_EQ(first, item::kHandleFirst);
    EXPECT_EQ(second, item::kHandleFirst + 1);
    ASSERT_NE(item::FindCopy(first), nullptr);
    EXPECT_EQ(item::FindCopy(first)->state.at("size"), 31.5);
    EXPECT_NE(item::FindCopy(first)->instance, item::FindCopy(second)->instance);
    item::FindCopy(second)->state["size"] = 40.0;
    EXPECT_EQ(item::FindCopy(first)->state.at("size"), 31.5);

    // Retail's lookups see the item behind the handle.
    EXPECT_EQ(item::Definition(first), bass->number);
    EXPECT_EQ(item::KindOf(first), item::Kind::Item);
    EXPECT_EQ(GetCommonItemInfo(first), GetCommonItemInfo(bass->number));
    EXPECT_EQ(GetItemData(first), GetItemData(bass->number));
    EXPECT_EQ(text::GameTextDecode(item::CustomMessage(first + 100, true)), "Rod");
    EXPECT_EQ(item::Definition(first + 2), -1);
    EXPECT_EQ(item::KindOf(first + 2), item::Kind::None);

    // A copy that left every slot gives its handle back, to be used again.
    g_custom_save.Initialize();
    SaveData = &g_custom_save;
    CDngStatusData &status = *g_custom_save.GetDngStatus();
    ASSERT_GE(status.GetItem(second, 0), 0);
    item::ReclaimHeldCopies();
    EXPECT_EQ(item::FindCopy(first), nullptr);
    ASSERT_NE(item::FindCopy(second), nullptr);
    EXPECT_EQ(item::NewCopy(*bass), first);
    EXPECT_EQ(item::CopyCount(), 2);

    // A copy whose item is gone keeps its ID and state, and is nothing to retail's code.
    Order("");
    EXPECT_EQ(item::Definition(second), -1);
    EXPECT_EQ(item::KindOf(second), item::Kind::None);
    EXPECT_EQ(GetCommonItemInfo(second), nullptr);
    EXPECT_EQ(item::FindCopy(second)->id, "chronicle:bass");
    Order(R"("fish")");
    EXPECT_EQ(item::Definition(second), item::Find("chronicle:bass")->number);
    EXPECT_EQ(item::FindCopy(second)->state.at("size"), 40.0);
    SaveData = nullptr;
}

TEST_F(ItemDocuments, ACopyKeptInASaveComesBackUnderAFreeHandle) {
    item::ClearCopies(7);
    const int handle = item::RestoreCopy("chronicle:bass", 3, {{"size", 12.0}}, 1);
    EXPECT_EQ(handle, item::kHandleFirst);
    EXPECT_EQ(item::FindCopy(handle)->instance, 3U);
    EXPECT_EQ(item::FindCopy(handle)->number, -1);
    EXPECT_EQ(item::NextInstance(), 7U);
    Pack("fish", "custom/chronicle.bass.json", KeptFish("chronicle:bass"));
    Order(R"("fish")");
    EXPECT_EQ(item::Definition(handle), item::Find("chronicle:bass")->number);
    const int made = item::NewCopy(*item::Find("chronicle:bass"));
    EXPECT_EQ(item::FindCopy(made)->instance, 7U);
    EXPECT_EQ(item::NextInstance(), 8U);
}

TEST_F(ItemDocuments, StateThatIsNotNumbersIsRefused) {
    nlohmann::json fish = KeptFish("chronicle:bass");
    fish["state"] = {
        {"species", "bass"}
    };
    Pack("fish", "custom/chronicle.bass.json", fish);
    Order(R"("fish")");
    EXPECT_EQ(item::Find("chronicle:bass"), nullptr);
}

TEST_F(ItemDocuments, CustomItemsFromEveryPackAreHeardAndNumberedByID) {
    Pack("first", "custom/chronicle.rod_3.json", CustomItem("chronicle:rod_3"));
    Pack("second", "custom/chronicle.rod_2.json", CustomItem("chronicle:rod_2"));
    auto changed = CustomItem("chronicle:rod_3");
    changed["buy_price"] = 999;
    Pack("second", "custom/chronicle.rod_3.json", changed);
    Order(R"("first", "second")");
    ASSERT_NE(item::Find("chronicle:rod_2"), nullptr);
    ASSERT_NE(item::Find("chronicle:rod_3"), nullptr);
    EXPECT_EQ(item::Find("chronicle:rod_2")->number, item::kCustomFirst);
    EXPECT_EQ(item::Find("chronicle:rod_3")->number, item::kCustomFirst + 1);
    EXPECT_EQ(item::Find("chronicle:rod_3")->Get<item::Sellable>()->buy, 999);
}

TEST_F(ItemDocuments, ACustomItemThatCannotBeReadIsLeftOutAndTold) {
    Pack("bad", "custom/chronicle.wrong_name.json", CustomItem("chronicle:rod_2"));
    Pack("bad", "custom/vanilla.bread.json", CustomItem("vanilla:bread"));
    auto price = CustomItem("chronicle:pricey");
    price["buy_price"] = 1 << 20;
    Pack("bad", "custom/chronicle.pricey.json", price);
    testing::internal::CaptureStderr();
    Order(R"("bad")");
    const std::string told = testing::internal::GetCapturedStderr();
    EXPECT_EQ(item::Find("chronicle:rod_2"), nullptr);
    EXPECT_EQ(item::Find("chronicle:pricey"), nullptr);
    EXPECT_EQ(item::Find("vanilla:bread")->number, ITEM_BREAD);
    EXPECT_NE(told.find("chronicle.wrong_name.json"), std::string::npos) << told;
    EXPECT_NE(told.find("vanilla namespace"), std::string::npos) << told;
    EXPECT_NE(told.find("buy_price"), std::string::npos) << told;
}

TEST_F(ItemDocuments, ACustomItemsBaitAndFieldsItDoesNotReadAreTold) {
    auto lure = CustomItem("chronicle:lure");
    lure["bait"] = {
        {"radius",           40.0},
        {"lure",             true},
        {"default_affinity", 0.5 },
        {"uses",             0   }
    };
    lure["colour"] = "red";
    lure["rod"] = {
        {"spring", 77.0},
        {"lure",   20.0},
        {"reach",  3.0 }
    };
    Pack("lures", "custom/chronicle.lure.json", lure);
    testing::internal::CaptureStderr();
    Order(R"("lures")");
    const std::string told = testing::internal::GetCapturedStderr();
    const item::Bait *bait = item::Find("chronicle:lure")->Get<item::Bait>();
    ASSERT_NE(bait, nullptr);
    EXPECT_TRUE(bait->lure);
    EXPECT_EQ(bait->default_affinity, 0.5f);
    const item::Rod *rod = item::Find("chronicle:lure")->Get<item::Rod>();
    ASSERT_NE(rod, nullptr);
    EXPECT_EQ(rod->spring, 77.0f);
    EXPECT_EQ(rod->lure, 20.0f);
    EXPECT_EQ(rod->heft, item::Rod{}.heft); // left out: retail's
    EXPECT_NE(told.find("fields this version does not read: bait.uses, colour, rod.reach"), std::string::npos) << told;
}

TEST_F(ItemDocuments, ACustomItemInTheDataIsReadUnderThePacks) {
    const datafix::Disc disc = datafix::MakeDisc({
        {"dun/pack/maindat.pac", datafix::Pattern(64, 1)}
    });
    datafix::WriteBytes(data / "data.hd2", disc.hd2);
    datafix::WriteBytes(data / "dun/pack/maindat.pac", disc.files[0].data);
    fs::create_directories(data / "gameplay/items/custom");
    std::ofstream(data / "gameplay/items/custom/chronicle.rod_2.json") << CustomItem("chronicle:rod_2").dump();
    PathsSetDataRoot(data);
    PathsSetSaveRoot(directory / "save");
    data::DataReadIndex();
    Order("");
    ASSERT_NE(item::Find("chronicle:rod_2"), nullptr);
    EXPECT_EQ(item::Find("chronicle:rod_2")->Get<item::Sellable>()->buy, 120);

    auto changed = CustomItem("chronicle:rod_2");
    changed["buy_price"] = 7;
    Pack("cheap", "custom/chronicle.rod_2.json", changed);
    Order(R"("cheap")");
    EXPECT_EQ(item::Find("chronicle:rod_2")->Get<item::Sellable>()->buy, 7);
    data::DataReadReset();
}

} // namespace

} // namespace dc::test
