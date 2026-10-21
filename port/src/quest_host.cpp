#include "quest_host.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "custom_shop.hpp"
#include "dialog_framework.hpp"
#include "dialog_npc.hpp"
#include "dngstatusdata.hpp"
#include "edit.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "itemdata.hpp"
#include "localize.hpp"
#include "main.hpp"
#include "menu_save.hpp"
#include "platform/files.hpp"
#include "quest_framework.hpp"
#include "quests/quests.hpp"
#include "runscript_port.hpp"
#include "savedata.hpp"
#include "stockitem.hpp"

namespace {

namespace fs = std::filesystem;

constexpr const char *kFileName = "quests.json";
constexpr int         kMostMoney = 0xFFFF;

CDngStatusData *Status() {
    return SaveData != nullptr ? SaveData->GetDngStatus() : nullptr;
}

bool InPack(int item) {
    return item >= ITEM_DUNGEON_START && item < ITEM_WEAPON_START;
}

bool IsWeapon(int item) {
    return item >= ITEM_WEAPON_START && item < ITEM_WEAPON_END;
}

// As CDngStatusData::GetItem counts a weapon's owner's: fewer than ten weapons in their first ten slots
// (an eleventh is the overflow the game makes them throw one away from).
bool WeaponHasRoom(const CDngStatusData &status, int owner) {
    int count = 0;
    for (int i = 0; i < 10; ++i) {
        count += status.chara_weapons[owner][i].item_no >= ITEM_WEAPON_START ? 1 : 0;
    }
    return count < 10;
}

// As CDngStatusData::GetItem counts: the items in the pack's slots and in its quick-use slots.
bool PackHasRoom(const CDngStatusData &status) {
    int count = 0;
    for (s16 item : status.item_pack.item) {
        count += item >= ITEM_DUNGEON_START ? 1 : 0;
    }
    for (int i = 0; i < 3; ++i) {
        if (status.item_pack.quick_item_slot[i] != -1) {
            count += status.item_pack.quick_item_qty[i];
        }
    }
    return count + 1 <= status.item_pack.num;
}

// A town's georama parts as they stand: the live ones for the town the player is in (EditPartsInfo),
// any other's read from its save into a copy kept for the question. Null for no such town.
const CEditPartsInfo *TownParts(int town) {
    if (SaveData == nullptr || town < 0 || town >= TOWN_COUNT) {
        return nullptr;
    }
    if (town == NowEditMap) {
        return &EditPartsInfo;
    }
    static CEditPartsInfo saved;
    saved.Load(town, SaveData, 1);
    return &saved;
}

const EDITPARTS_INFO *TownPart(int town, int part) {
    const CEditPartsInfo *parts = TownParts(town);
    return parts != nullptr && part >= 0 && part < 24 ? &parts->parts[part] : nullptr;
}

// Retail's PlayerAllItemCheck: whether the player has the item in the pack or stored.
bool HasAnywhere(int item) {
    if (SaveData == nullptr) {
        return false;
    }
    return SaveData->GetDngStatus()->SearchItemIndexNo(item) >= 0 || SaveData->GetStockItem()->SearchItem(item) != 0;
}

// The Mayor's talk script hands out his supplies while this holds: from the conversation starting,
// as the script runs, until the conversation is up (QuestHostTalkStarting, QuestHostTalkOpened).
bool g_hold_mayor_supplies = false;

// The save data in play, as the quests see it.
class SaveGame final : public Quest::IGame {
public:
    int ItemCount(int item) const override {
        const CDngStatusData *status = Status();
        if (status == nullptr) {
            return 0;
        }
        // What the pack carries, which TakeItem takes from; the game counts other kinds its own way.
        if (InPack(item)) {
            return static_cast<int>(std::count(std::begin(status->item_pack.item), std::end(status->item_pack.item), item));
        }
        return GetNumHowManyItemsHave(item);
    }

    bool GiveItem(int item) override {
        CDngStatusData *status = Status();
        if (status == nullptr) {
            return false;
        }
        if (InPack(item)) {
            if (!PackHasRoom(*status)) {
                return false;
            }
            status->GetItem(item, 1);
            return true;
        }
        if (IsWeapon(item)) {
            const WEAPON_DATA *weapon = GetWeaponData(item);
            if (weapon == nullptr || weapon->owner >= std::size(status->chara_weapons) ||
                !WeaponHasRoom(*status, weapon->owner)) {
                return false;
            }
            status->GetItem(item, 0);
            return true;
        }
        return false;
    }

    bool TakeItem(int item) override {
        CDngStatusData *status = Status();
        return status != nullptr && InPack(item) && status->LostItem(item) >= 0;
    }

    int Money() const override {
        const CDngStatusData *status = Status();
        return status != nullptr ? status->money : 0;
    }

    bool SpendMoney(int amount) override {
        CDngStatusData *status = Status();
        if (status == nullptr || amount < 0 || status->money < amount) {
            return false;
        }
        status->money = static_cast<u16>(status->money - amount);
        return true;
    }

    void EarnMoney(int amount) override {
        CDngStatusData *status = Status();
        if (status != nullptr && amount > 0) {
            status->money = static_cast<u16>(std::min(kMostMoney, status->money + amount));
        }
    }

    int DeepestFloor(int dungeon) const override {
        const CDngStatusData *status = Status();
        if (status == nullptr || dungeon < 0 || dungeon >= static_cast<int>(std::size(status->floor_reached))) {
            return -1;
        }
        return status->floor_reached[dungeon];
    }

    bool HasItemAnywhere(int item) const override {
        return HasAnywhere(item);
    }

    int DungeonCount(int dungeon) const override {
        return SaveData != nullptr ? SaveData->QuestDungeon(dungeon, 0) : 0;
    }

    bool GameFlag(int flag) const override {
        return SaveData != nullptr && SaveData->GetGameFlag(flag) != 0;
    }

    void SetGameFlag(int flag, bool on) override {
        if (SaveData != nullptr) {
            SaveData->SetGameFlag(flag, on ? 1 : 0);
        }
    }

    int GameNumber(int slot) const override {
        return SaveData != nullptr && slot >= 0 ? SaveData->GetGameIntFlag(slot) : 0;
    }

    void SetGameNumber(int slot, int value) override {
        if (SaveData != nullptr && slot >= 0) {
            SaveData->SetGameIntFlag(slot, value);
        }
    }

    // The scripts' map flags are the save's turned over (_SGET_MAP_FLAG, _SSET_MAP_FLAG); the map the
    // player is on goes through EdGetMapFlag, which shares Treant's chest flags between its two maps.
    bool MapFlag(int map, int flag) const override {
        if (SaveData == nullptr || flag <= 0) {
            return false;
        }
        const int stored = map == MapNo ? EdGetMapFlag(flag) : SaveData->GetMapFlag(map, flag);
        return stored == 0;
    }

    void SetMapFlag(int map, int flag, bool on) override {
        if (SaveData == nullptr || flag <= 0) {
            return;
        }
        if (map == MapNo) {
            EdSetMapFlag(flag, on ? 0 : 1);
        } else {
            SaveData->SetMapFlag(map, flag, on ? 0 : 1);
        }
    }

    bool PartObtained(int town, int part) const override {
        const EDITPARTS_INFO *info = TownPart(town, part);
        return info != nullptr && info->obtained != 0;
    }

    int PartPlaced(int town, int part) const override {
        const EDITPARTS_INFO *info = TownPart(town, part);
        return info != nullptr ? info->placed : 0;
    }

    bool PartComplete(int town, int part) const override {
        const CEditPartsInfo *parts = TownParts(town);
        return parts != nullptr && part >= 0 && part < 24 && const_cast<CEditPartsInfo *>(parts)->CheckComplete(part) != 0;
    }

    bool PartEventDone(int town, int part) const override {
        const EDITPARTS_INFO *info = TownPart(town, part);
        return info != nullptr && (info->completion_flags & 1) != 0;
    }

    bool RequestMet(int town, int request) const override {
        const CEditPartsInfo *parts = TownParts(town);
        return parts != nullptr && request >= 0 && request < 24 && parts->request[request] != 0;
    }

    bool VillagerHome(int town, int villager) const override {
        // As _CHECK_VILLAGER asks it, of the sixteen villagers a town's script knows (GetVillagerInfo).
        if (town != NowEditMap || EdEventInfo.edit_ground == nullptr || EdEventInfo.villagers == nullptr ||
            villager < 0 || villager >= 16) {
            return false;
        }
        return EdCheckVillager(villager, &EdEventInfo.villagers[villager], EdEventInfo.edit_ground) != 0;
    }

    int PartySize() const override {
        const CDngStatusData *status = Status();
        return status != nullptr ? status->party_size : 0;
    }

    std::string TalkLine(int message) const override {
        return DialogNpcTalkLine(message);
    }

    void OpenShop(Quest::ShopSpec shop) override;
    bool ShowItemReceived(int item) override;
};

// A quest's shop, from the choice that opened it until the conversation has ended.
std::optional<Quest::ShopSpec> g_pending_shop;
// An item a quest shows the player receiving, likewise; -1 for none.
int g_pending_item_shown = -1;

CustomShopLine ToCustomLine(Quest::ShopLine line) {
    switch (line) {
        case Quest::ShopLine::Greeting:
            return CustomShopLine::Greeting;
        case Quest::ShopLine::Browsing:
            return CustomShopLine::Browsing;
        case Quest::ShopLine::Selling:
            return CustomShopLine::Selling;
        case Quest::ShopLine::NotEnoughMoney:
            return CustomShopLine::NotEnoughMoney;
        case Quest::ShopLine::NoRoom:
            return CustomShopLine::NoRoom;
        case Quest::ShopLine::Confirm:
            return CustomShopLine::Confirm;
        case Quest::ShopLine::Thanks:
            return CustomShopLine::Thanks;
        case Quest::ShopLine::Goodbye:
            return CustomShopLine::Goodbye;
    }
    return CustomShopLine::Greeting;
}

void SaveGame::OpenShop(Quest::ShopSpec shop) {
    g_pending_shop = std::move(shop);
}

bool SaveGame::ShowItemReceived(int item) {
    if (!EventScriptHasItemGet()) {
        return false;
    }
    g_pending_item_shown = item;
    return true;
}

} // namespace

void QuestHostSetup() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    static SaveGame game;
    RegisterAllQuests();
    Quest::SetGame(&game);
    Dialog::RegisterOptionProvider(
        [](const Dialog::NpcContext &npc, std::vector<Dialog::ChoiceItem> &out) { Quest::CollectOptions(npc, out); });
}

void QuestHostTalkStarting(int map_no, int npc_id) {
    QuestHostSetup();
    g_pending_item_shown = -1;
    Dialog::NpcContext npc;
    npc.map_no = map_no;
    npc.npc_id = npc_id;
    g_hold_mayor_supplies = Quest::TakesOver(npc, Quest::Retail::MayorSupplies);
}

void QuestHostTalkOpened() {
    g_hold_mayor_supplies = false;
}

// The talk script asks this whether the player has an item anywhere (_SITEM_CHECK_ALL) before the Mayor
// hands it over. While a quest has his supplies, his own count as had, so the script hands nothing out.
int QuestHostPlayerAllItemCheck(int item) {
    if (g_hold_mayor_supplies && std::find(std::begin(kNoruneMayorSupplies), std::end(kNoruneMayorSupplies), item) !=
                                     std::end(kNoruneMayorSupplies)) {
        return 1;
    }
    return HasAnywhere(item) ? 1 : 0;
}

void QuestHostNewGame() {
    QuestHostSetup();
    g_hold_mayor_supplies = false;
    Quest::ResetProgress();
    g_pending_shop.reset();
    CustomShopEnd();
}

bool QuestHostSave(const fs::path &save_directory) {
    QuestHostSetup();
    const std::string text = Quest::SaveJson();
    if (FilesWrite(save_directory / kFileName, text.data(), text.size(), true) != FilesResult::kWritten) {
        std::fprintf(stderr, "quests: could not write %s\n", (save_directory / kFileName).string().c_str());
        return false;
    }
    return true;
}

void QuestHostLoad(const fs::path &save_directory) {
    QuestHostSetup();
    g_pending_shop.reset();
    CustomShopEnd();

    std::ifstream stream(save_directory / kFileName, std::ios::binary);
    if (!stream) {
        Quest::ResetProgress();
        return;
    }
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (!Quest::LoadJson(text)) {
        std::fprintf(stderr, "quests: %s is not a quest file; every quest starts again\n",
                     (save_directory / kFileName).string().c_str());
    }
}

void QuestHostDelete(const fs::path &save_directory) {
    std::error_code error;
    fs::remove(save_directory / kFileName, error);
}

bool QuestHostTakeItemShown(int *event_no) {
    if (g_pending_item_shown < 0) {
        return false;
    }
    EventScriptSetItemGet(g_pending_item_shown);
    g_pending_item_shown = -1;
    *event_no = kEventItemGet;
    return true;
}

bool QuestHostTakeShop(const char *speaker_model, int *shop_no) {
    if (!g_pending_shop) {
        return false;
    }
    Quest::ShopSpec spec = std::move(*g_pending_shop);
    g_pending_shop.reset();

    CustomShop shop;
    shop.look = spec.look_of_shop;
    for (const Quest::ShopGood &good : spec.goods) {
        shop.goods.emplace_back(good.item, good.price);
    }
    if (spec.keeper == Quest::Shopkeeper::Speaker && speaker_model != nullptr) {
        shop.keeper = speaker_model;
    }
    // DC_DIALOG_TEST=1 traces who stands behind the counter, beside the dialog framework's test choice.
    if (std::getenv("DC_DIALOG_TEST") != nullptr) {
        std::fprintf(stderr, "quests: shop %d, speaker model %s\n", spec.look_of_shop,
                     speaker_model != nullptr ? speaker_model : "(none)");
    }
    for (const auto &[line, text] : spec.lines) {
        shop.lines[ToCustomLine(line)] = LocalizeText(text.key, text.english);
    }
    *shop_no = CustomShopBegin(std::move(shop));
    return true;
}
