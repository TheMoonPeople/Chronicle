#include "pickup_qol.hpp"

#include <iterator>

#include "btitem.hpp"
#include "btmisc.hpp"
#include "clsmes.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "dungeonmap.hpp"
#include "memcard.hpp"
#include "platform/config.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "userstatus.hpp"

extern int iventMarker;
int createAttachVolume(int item_no, int dungeon);

namespace {
bool ActiveEvent(int kind) {
    return UserStatus && NowDngMap && iventActive >= 0 &&
           iventActive < static_cast<int>(std::size(NowDngMap->events)) &&
           NowDngMap->events[iventActive].kind == kind;
}

void FinishPickup() {
    ResetMovePower();
    iventActive = -1;
    iventInfo = DNG_EVENT_NONE;
    iventMarker = 0;
}
}

bool QolSkipSmallChest(int dungeon) {
    if (!ConfigGet().skip_small_chest_cutscene || !ActiveEvent(DNG_EVENT_TREASURE_BOX)) return false;
    auto &event = NowDngMap->events[iventActive];
    if (event.index < 0 || event.index >= static_cast<int>(std::size(NowDngMap->boxes))) return false;
    auto &box = NowDngMap->boxes[event.index];
    if (box.kind != TREASURE_BOX_SMALL || !box.closed || box.trap_no != 0) return false;
    int item = box.item_no;
    if (item < ITEM_ATTACH_START || item >= ITEM_WEAPON_START) return false;
    // Keep the normal full-inventory refusal sequence and leave the chest available.
    if (reinterpret_cast<CDngStatusData *>(UserStatus)->CheckItemGet(item) != 0) return false;
    // Match the retail small-chest fallback and attachment roll exactly.
    if (ITEM_NAME_TBL_NEW[item - ITEM_ATTACH_START] == nullptr) item = ITEM_ATTACH_PERIDOT;
    const int volume = createAttachVolume(item, dungeon);
    if (item == ITEM_MAP) BtEquipMap = 1;
    else if (item == ITEM_MAGICAL_CRYSTAL) BtEquipMasuisyou = 1;
    else reinterpret_cast<CDngStatusData *>(UserStatus)->GetItem(item, volume);
    box.closed = 0;
    box.lid_angle = -30.0f;
    event.kind = DNG_EVENT_NONE;
    FinishPickup();
    SndSePlay(SE_CHEST_OPEN_SMALL, -1, 0);
    ItemGetMes(item, volume != 0 ? volume : -1, 90, 0);
    return true;
}

bool QolSkipAtla(int dungeon, int floor) {
    if (!ConfigGet().skip_atla_cutscene || !ActiveEvent(DNG_EVENT_ATRA) || !SaveData || UserStatus->cur_chara != CHARA_TOAN) return false;
    auto &event = NowDngMap->events[iventActive];
    if (event.index < 0 || event.index >= static_cast<int>(std::size(NowDngMap->atra))) return false;
    auto &atla = NowDngMap->atra[event.index];
    if (!atla.used || dungeon < 0 || dungeon >= 6 || floor < 0 || floor >= 40 ||
        atla.atra_no < 0 || atla.atra_no >= 100) return false;
    const int id = UserStatus->atra_registry[dungeon][atla.atra_no].id;
    if (id < 0 || id >= 100) return false;
    // This is the retail award path: it grants Georama parts/chips and updates floor progress.
    getAtraToSaveData(id, atla.atra_no, SaveData, dungeon, floor);
    atla.used = 0;
    event.kind = DNG_EVENT_NONE;
    FinishPickup();
    int args[4] = {GetAtraMsgNo(dungeon, id), -1, -1, -1};
    SetSystemMes(40, 90, MES_POS_BOTTOM, 0, args, nullptr);
    return true;
}
