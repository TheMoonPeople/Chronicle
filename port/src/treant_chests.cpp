#include "common.h"
#include "port.h"

#include <libvu0.h>

#include "edit.hpp"
#include "editloop.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "objanime.hpp"
#include "savedata.hpp"
#include "treant_chests_port.hpp"

namespace {

// Where the flooded clearing's three more chests stand. The clearing's chests hang off a frame
// placed at the origin, so these are world positions; y is the ground there. A chest of yaw r faces
// (sin r, cos r) in x and z; the clearing's own Gourd, Fruit of Eden and Grass Cake face kChestYaw.
struct TreantChest {
    int   item;
    float position[3];
    float yaw;
    int   flag;
};

constexpr float kChestYaw = -1.862f;
constexpr float kPi = 3.14159265f;

constexpr TreantChest kExtraChests[] = {
    // By the new pond, facing straight away from its bank.
    {ITEM_GOURD,               {-20.0f, 0.0f, 195.0f}, 1.937f,                 365},
    {ITEM_FRUIT_OF_EDEN,       {125.0f, 0.1f, 95.0f},  kChestYaw + kPi / 6.0f, 366},
    {ITEM_ATTACH_BEAST_BUSTER, {60.0f, 0.6f, 165.0f},  kChestYaw + kPi / 2.0f, 367},
};

constexpr int kExtraChestProgress = 9;

} // namespace

bool TreantChestFlag(int map_no, int flag_no) {
    return (map_no == kTreantClearingDry || map_no == kTreantClearingFlooded) && flag_no >= kTreantChestFirstFlag &&
           flag_no <= kTreantChestLastFlag;
}

PC_OVERRIDE int EdGetMapFlag(int flag_no) {
    if (flag_no <= 0) {
        return 0;
    }

    if (TreantChestFlag(MapNo, flag_no)) {
        return SaveData->GetMapFlag(kTreantClearingDry, flag_no) || SaveData->GetMapFlag(kTreantClearingFlooded, flag_no);
    }

    return SaveData->GetMapFlag(MapNo, flag_no);
}

PC_OVERRIDE int EdSetMapFlag(int flag_no, int value) {
    if (flag_no <= 0) {
        return 0;
    }

    if (TreantChestFlag(MapNo, flag_no)) {
        SaveData->SetMapFlag(kTreantClearingDry, flag_no, value);
        return SaveData->SetMapFlag(kTreantClearingFlooded, flag_no, value);
    }

    return SaveData->SetMapFlag(MapNo, flag_no, value);
}

int AddTreantChests(CMapParts *parts, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points) {
    if (MapNo != kTreantClearingFlooded) {
        return 0;
    }

    const EPARTS_FUNC_DATA *gourd = nullptr;

    for (int i = 0; i < function_count; i++) {
        if (functions[i].kind == EPARTS_FUNC_ITEM_BOX && functions[i].completion_flag == kTreantChestFirstFlag) {
            gourd = &functions[i];
            break;
        }
    }

    if (gourd == nullptr) {
        return 0;
    }

    constexpr int    count = sizeof(kExtraChests) / sizeof(kExtraChests[0]);
    EPARTS_FUNC_DATA extra[count];

    for (int i = 0; i < count; i++) {
        const TreantChest &chest = kExtraChests[i];
        extra[i] = *gourd;
        extra[i].completion_flag = chest.flag;
        extra[i].position[0] = chest.position[0];
        extra[i].position[1] = chest.position[1];
        extra[i].position[2] = chest.position[2];
        extra[i].rotation[1] = chest.yaw;
        extra[i].values[0] = static_cast<float>(chest.item);
        extra[i].values[2] = static_cast<float>(kExtraChestProgress);

        // As LoadPTS starts every part's flags: the first time the map loads, closed.
        if (SaveData->GetMapInitFlag(MapNo, chest.flag) == 0) {
            SaveData->SetMapInitFlag(MapNo, chest.flag, 1);
            SaveData->SetMapFlag(MapNo, chest.flag, !(char) extra[i].unk_28[0]);
        }
    }

    return EdInitEventPoint(parts, NULL, extra, count, points, 0x100);
}
