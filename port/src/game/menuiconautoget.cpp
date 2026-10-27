#include "menuiconautoget.hpp"

#include <array>
#include <cstring>

#include "menu_draw.hpp"
#include "savedata.hpp"

namespace dc::game {

int CMenuIconAutoGet::IsMoveIcon() {
    int moving = 0;

    for (const MENU_AUTO_GET_ICON &entry : icon) {
        if (entry.item > 0) {
            moving = 1;
        }
    }

    return moving;
}

int CMenuIconAutoGet::IsSameItem(int item) {
    int found = 0;

    for (const MENU_AUTO_GET_ICON &entry : icon) {
        if (item == entry.item) {
            found++;
        }
    }

    return found;
}

int CMenuIconAutoGet::GetMoveIconGole(int slot, int item) {
    int found = 0;

    for (const MENU_AUTO_GET_ICON &entry : icon) {
        if (slot == entry.slot && item == entry.item) {
            found++;
        }
    }

    return found;
}

int CMenuIconAutoGet::GetSpace() {
    for (int i = 0; i < 3; i++) {
        if (icon[i].item < ITEM_ATTACH_START) {
            return i;
        }
    }

    return -1;
}

int CMenuIconAutoGet::IconAutoMove() {
    constexpr std::array<int, 3> destination_x = {92, 156, 220};

    int             moving = 0;
    CDngStatusData *dungeon_status = SaveData->GetDngStatus();
    ITEM_PACK      &pack = dungeon_status->item_pack;

    for (MENU_AUTO_GET_ICON &entry : icon) {
        if (entry.item > 0) {
            moving = 1;
            entry.x += (destination_x[entry.slot] - entry.x) / 4.0f;
            entry.y += (105.0f - entry.y) / 4.0f;

            if (entry.x - destination_x[entry.slot] < 4.0f) {
                int slot = entry.slot;

                if (pack.quick_item_slot[slot] <= 0) {
                    pack.quick_item_slot[slot] = entry.item;
                    pack.quick_item_qty[slot] = 1;
                    dungeon_status->active_item_vol[slot] = entry.volume;
                } else {
                    pack.quick_item_qty[slot]++;
                }

                // A cleared icon reads back as item -1, which marks the slot free.
                std::memset(&entry, -1, sizeof(entry));
            }
        }
    }

    return moving;
}

void CMenuIconAutoGet::IconMoveTarSet(int index, int slot, int item, int count, float x, float y) {
    icon[index].slot = static_cast<s16>(slot);
    icon[index].item = static_cast<s16>(item);
    icon[index].volume = static_cast<s16>(count);
    icon[index].x = x;
    icon[index].y = y;
}

void CMenuIconAutoGet::IconAutoMoveDraw() {
    for (const MENU_AUTO_GET_ICON &entry : icon) {
        if (entry.item >= ITEM_DUNGEON_START) {
            DrawIconParts(entry.item, static_cast<int>(entry.x), static_cast<int>(entry.y), 0, 640, 128, 0);
        }
    }
}

} // namespace dc::game
