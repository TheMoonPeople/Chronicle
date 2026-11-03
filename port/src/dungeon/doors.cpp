#include "dungeon/doors.hpp"

#include <utility>

#include "dun/gameloop.hpp"
#include "dungeon/rules.hpp"
#include "dungeoneventdata.hpp"
#include "dungeonparts.hpp"
#include "item/framework.hpp"
#include "userstatus.hpp"

namespace dc::dungeon {

using namespace dc::game;

namespace {

// The most items the event item window takes, as _ITEM_USE_WINDOW gives them.
constexpr int kUsableMost = 13;

int g_pending_key = -1;

int BagSlot(int item) {
    const ITEM_PACK &pack = UserStatus->item_pack;
    for (int slot = 0; slot < pack.num; slot++) {
        if (pack.item[slot] == item) {
            return slot;
        }
    }
    return -1;
}

// A key the bag holds that opens the dungeon's locked doors, or -1.
int CarriedDoorKey() {
    for (const item::Def *def : item::With<item::DungeonKey>()) {
        const item::DungeonKey *key = def->Get<item::DungeonKey>();
        if (key->opens == item::DungeonKey::Opens::LockedDoor && key->dungeon == selectMapNo && BagSlot(def->number) >= 0) {
            return def->number;
        }
    }
    return -1;
}

// Whether slot is a locked room door's event: the map part that placed it is the door.
bool LockedDoorEvent(const CDungeonEventData &slot) {
    return slot.event != nullptr && slot.event->parts_id >= MAP_PARTS_ROOM_GATE_NORTH && slot.event->parts_id <= MAP_PARTS_ROOM_GATE_WEST;
}

} // namespace

bool AutoOpenLockedDoor(const CDungeonEventData &slot) {
    g_pending_key = -1;
    if (!GetRules().auto_open_locked_doors || slot.hold == 0 || UserStatus == nullptr || !LockedDoorEvent(slot)) {
        return false;
    }
    g_pending_key = CarriedDoorKey();
    return g_pending_key >= 0;
}

bool TakeAutoKey(const int *usable, bool vanish, int &item) {
    const int key = std::exchange(g_pending_key, -1);
    if (key < 0 || usable == nullptr || UserStatus == nullptr) {
        return false;
    }
    bool taken = false;
    for (int i = 0; i < kUsableMost && usable[i] >= 0; i++) {
        taken = taken || usable[i] == key;
    }
    const int slot = taken ? BagSlot(key) : -1;
    if (slot < 0) {
        return false;
    }
    if (vanish) {
        UserStatus->item_pack.item[slot] = 0;
    }
    item = key;
    return true;
}

} // namespace dc::dungeon
