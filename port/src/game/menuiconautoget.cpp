#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#include "menuiconautoget.hpp"

#include <cstring>

#include "menu_draw.hpp"
#include "savedata.hpp"

int CMenuIconAutoGet::IsMoveIcon() {
    int moving = 0;

    for (int i = 0; i < 3; i++) {
        if (icon[i].item > 0) {
            moving = 1;
        }
    }

    return moving;
}

int CMenuIconAutoGet::IsSameItem(int item) {
    int found = 0;

    for (int i = 0; i < 3; i++) {
        if (item == icon[i].item) {
            found++;
        }
    }

    return found;
}

int CMenuIconAutoGet::GetMoveIconGole(int slot, int item) {
    int found = 0;

    for (int i = 0; i < 3; i++) {
        if (slot == icon[i].slot && item == icon[i].item) {
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
    int             moving = 0;
    CDngStatusData *dungeon_status = SaveData->GetDngStatus();
    int             i;
    ITEM_PACK      *pack = &dungeon_status->item_pack;
    int             destination_x[3] = {92, 156, 220};

    for (i = 0; i < 3; i++) {
        if (icon[i].item > 0) {
            moving = 1;
            icon[i].x += (destination_x[icon[i].slot] - icon[i].x) / 4.0f;
            icon[i].y += (105.0f - icon[i].y) / 4.0f;

            if (icon[i].x - destination_x[icon[i].slot] < 4.0f) {
                int slot = icon[i].slot;

                if (pack->quick_item_slot[slot] <= 0) {
                    pack->quick_item_slot[slot] = icon[i].item;
                    pack->quick_item_qty[slot] = 1;
                    dungeon_status->active_item_vol[slot] = icon[i].volume;
                } else {
                    pack->quick_item_qty[slot]++;
                }

                memset(&icon[i], -1, sizeof(icon[i]));
            }
        }
    }

    return moving;
}

void CMenuIconAutoGet::IconMoveTarSet(int index, int slot, int item, int count, float x, float y) {
    icon[index].slot = slot;
    icon[index].item = item;
    icon[index].volume = count;
    icon[index].x = x;
    icon[index].y = y;
}

void CMenuIconAutoGet::IconAutoMoveDraw() {
    for (int i = 0; i < 3; i++) {
        if (icon[i].item >= ITEM_DUNGEON_START) {
            DrawIconParts(icon[i].item, (int) icon[i].x, (int) icon[i].y, 0, 640, 128, 0);
        }
    }
}
