#include "menuitemstep.hpp"

#include <algorithm>

#include "dngstatusdata.hpp"
#include "itemdata.hpp"
#include "savedata.hpp"

namespace dc::game {

void CMenuItemStep::Initialize() {
    frame = 0;
    last_item = -1;
    last_slot = -1;
    enabled = true;
    pending_volume = 0;

    for (s16 &item : held_item) {
        item = 0;
    }

    for (int i = 0; i < 4; i++) {
        chara_slot[i] = -1;
        chara_item[i] = -1;
    }
}

void CMenuItemStep::LoopStep(int interval) {
    if (enabled != 0) {
        // A negative interval asks for the default of one second.
        if (interval < 0) {
            interval = 60;
        }

        frame++;

        if (frame >= interval) {
            pending_volume++;
            frame = 0;
        }
    }
}

void CMenuItemStep::CheckItemVolume() {
    const int elapsed = pending_volume;

    if (elapsed > 0) {
        int        preservation[100]{};
        ITEM_PACK *pack = &SaveData->GetDngStatus()->item_pack;
        chara_no = 0;

        for (int slot = 0; slot < pack->num; slot++) {
            if (pack->item[slot] == ITEM_ICE_BLOCK || pack->item[slot] == ITEM_SMALL_ICE || pack->item[slot] == ITEM_TINY_ICE) {
                const int row = slot / 5;

                if (row != 0) {
                    preservation[slot - 5] += 100;
                }

                if (row != pack->num / 5) {
                    preservation[slot + 5] += 100;
                }

                const int column = slot % 5;

                if (column != 0) {
                    preservation[slot - 1] += 100;
                }

                if (column != 4) {
                    preservation[slot + 1] += 100;
                }

                pack->item_vol[slot] -= elapsed;

                if (pack->item_vol[slot] <= 0) {
                    pack->item[slot]++;

                    if (pack->item[slot] >= ITEM_TINY_ICE + 1) {
                        pack->item[slot] = -1;
                    } else {
                        pack->item_vol[slot] = GetItemData(pack->item[slot])->vol;
                    }
                }
            }
        }

        for (int slot = 0; slot < pack->num; slot++) {
            if (pack->item[slot] == ITEM_FLAPPING_FISH) {
                preservation[slot] = std::min(preservation[slot], 100);

                const int loss = static_cast<int>(static_cast<float>(elapsed) * (100.0f - static_cast<float>(preservation[slot])) / 100.0f);
                pack->item_vol[slot] = pack->item_vol[slot] - loss;

                if (pack->item_vol[slot] <= 0) {
                    pack->item[slot]++;
                }
            }
        }

        if (held_item[chara_no] >= ITEM_DUNGEON_START) {
            dungeon_item_held = 1;
        }

        pending_volume = 0;
        frame = 0;
    }
}

} // namespace dc::game
