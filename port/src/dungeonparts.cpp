#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dungeonparts.hpp"
#include "editmenu.hpp"
#include "itemdata.hpp"

extern s16 *ItemSetRateTbl[7];
extern int  floorNum[7];

/* The chance out of a hundred that each item is turned down when a treasure box draws it, one
   list per dungeon. Retail copies 380 bytes from the address of the dungeon's slot in
   ItemSetRateTbl, the table of pointers to these lists, rather than from the list the slot points
   to, so a box reads the words that follow the pointer table as rates and leaves the rest of the
   array, every weapon's rate among them, as whatever the stack held. What a box then turns down
   depends on the executable's layout and the floor, and differs between the NTSC and PAL discs.
   The port reads the dungeon's list, as GetPieroItem does for the clown's boxes. */
PORT_OVERRIDE int PresetSmallItemNo_Get(int map_no, int floor_no, int special, int small) {
    int           candidate[144];
    s16           rate[400];
    ITEM_PUT_SET *list = ItemPutListPtr[map_no + special * 7];
    int           held;
    int           count;
    int           item_no;
    float         roll;
    int           scan;
    int           wanted;
    int           i;
    s16          *table;
    int           tries;
    int           pick;
    int           list_no;
    int           chance;

    memset(rate, 0, sizeof(rate));
    memcpy(rate, ItemSetRateTbl[map_no], 385 * sizeof(s16));

    // Weapons the player already holds come up less often.
    for (i = ITEM_WEAPON_START; i < ITEM_WEAPON_START + 0x7B; i++) {
        held = GetNumHowManyItemsHave(i);

        if (held > 0) {
            if (held == 1) {
                rate[i - 1] -= 10;
            }

            if (held >= 2) {
                rate[i - 1] -= 20;
            }

            if (rate[i - 1] <= 0) {
                rate[i - 1] = 0;
            }
        }
    }

    scan = 0;
    wanted = -1;

    for (;; scan++) {
        item_no = list[scan].floor;

        if (item_no == -1) {
            break;
        }

        if (floor_no + 1 == item_no) {
            wanted = floor_no + 1;
        }
    }

    if (wanted == -1) {
        if (floor_no < floorNum[map_no]) {
            wanted = 0x100;
        } else {
            wanted = 0xFF;
        }
    }

    list_no = 0;

    do {
        if (wanted == list[list_no].floor) {
            break;
        }

        list_no++;

        if (list_no >= 128) {
            printf("err itembox list \n");
            return -1;
        }
    } while (1);

    if (small != 0) {
        int j = 0;
        count = 0;

        for (; list[list_no].item[j] != -1; j++) {
            item_no = list[list_no].item[j];

            if (item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START) {
                candidate[count++] = item_no;
            }
        }

        table = rate;
        tries = 0;

        do {
            roll = ((float) count * (float) rand()) / 2.1474836e9f;
            pick = (int) roll;

            if (roll - (float) pick > 0.0f) {
                pick++;
            }

            if (pick < 0 || pick >= count) {
                pick = 0;
            }

            chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);
            item_no = candidate[pick];

            if (table[item_no - 1] < chance) {
                break;
            }

            tries++;

            if (tries >= 0xFFFF) {
                item_no = -1;
                break;
            }
        } while (1);

        return item_no;
    }

    int j = 0;
    count = 0;

    for (; list[list_no].item[j] != -1; j++) {
        item_no = list[list_no].item[j];

        if (item_no >= ITEM_WEAPON_START) {
            candidate[count++] = item_no;
        }
    }

    if (count == 0) {
        return -1;
    }

    table = rate;
    tries = 0;

    do {
        roll = ((float) count * (float) rand()) / 2.1474836e9f;
        pick = (int) roll;

        if (roll - (float) pick > 0.0f) {
            pick++;
        }

        if (pick < 0 || pick >= count) {
            pick = 0;
        }

        chance = (int) ((100.0f * (float) rand()) / 2.1474836e9f);
        item_no = candidate[pick];

        if (table[item_no - 1] < chance) {
            break;
        }

        tries++;

        if (tries >= 0xFFFF) {
            item_no = -1;
            break;
        }
    } while (1);

    return item_no;
}
