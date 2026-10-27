#include "common.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <print>
#include <span>
#include <vector>

#include "dungeonparts.hpp"
#include "editmenu.hpp"
#include "itemdata.hpp"

extern s16 *ItemSetRateTbl[7];
extern int  floorNum[7];

namespace {

constexpr int kRateCount = 385;
constexpr int kMaxListItems = 128;
constexpr int kMaxDrawTries = 0xFFFF;

// Only the first normal list has four rows.
constexpr int kItemPutListCounts[14] = {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};

/* Draws candidates until one passes its turn-down chance, or returns -1 once the tries run out. */
int DrawCandidate(const std::vector<int> &candidates, std::span<const s16> rates) {
    const int count = static_cast<int>(candidates.size());

    for (int tries = 0;;) {
        const float roll = (static_cast<float>(count) * static_cast<float>(rand())) / 2.1474836e9f;
        int         pick = static_cast<int>(roll);

        if (roll - static_cast<float>(pick) > 0.0f) {
            pick++;
        }

        if (pick < 0 || pick >= count) {
            pick = 0;
        }

        const int chance = static_cast<int>((100.0f * static_cast<float>(rand())) / 2.1474836e9f);
        const int item_no = candidates[pick];

        if (item_no <= 0 || item_no > kRateCount) {
            return -1;
        }
        if (rates[item_no - 1] < chance) {
            return item_no;
        }

        tries++;

        if (tries >= kMaxDrawTries) {
            return -1;
        }
    }
}

} // namespace

/* The chance out of a hundred that each item is turned down when a treasure box draws it comes
   from the dungeon's rate list, the list GetPieroItem also reads for the clown's boxes. */
int PresetSmallItemNo_Get(int map_no, int floor_no, int special, int small) {
    if (map_no < 0 || map_no >= 7 || special < 0 || special > 1) {
        return -1;
    }

    const ITEM_PUT_SET *list = ItemPutListPtr[map_no + special * 7];
    const int           list_count = kItemPutListCounts[map_no + special * 7];

    if (list == nullptr || ItemSetRateTbl[map_no] == nullptr) {
        return -1;
    }

    auto rates = std::make_unique<s16[]>(kRateCount);
    std::copy_n(ItemSetRateTbl[map_no], kRateCount, rates.get());

    static_assert(ITEM_WEAPON_START + 0x7B <= kRateCount + 1);

    // Weapons the player already holds come up less often.
    for (int item_no = ITEM_WEAPON_START; item_no < ITEM_WEAPON_START + 0x7B; item_no++) {
        const int held = GetNumHowManyItemsHave(item_no);
        s16      &rate = rates[item_no - 1];

        if (held > 0) {
            if (held == 1) {
                rate -= 10;
            }

            if (held >= 2) {
                rate -= 20;
            }

            rate = std::max<s16>(rate, 0);
        }
    }

    int wanted = -1;
    int scan = 0;

    for (; scan < list_count; scan++) {
        const int floor = list[scan].floor;

        if (floor == -1) {
            break;
        }

        if (floor_no + 1 == floor) {
            wanted = floor_no + 1;
        }
    }

    if (scan == list_count) {
        return -1;
    }

    if (wanted == -1) {
        wanted = floor_no < floorNum[map_no] ? 0x100 : 0xFF;
    }

    int list_no = 0;

    while (wanted != list[list_no].floor) {
        list_no++;

        if (list_no >= list_count) {
            std::println("err itembox list ");
            return -1;
        }
    }

    const ITEM_PUT_SET &set = list[list_no];

    if (set.item_count < 0 || set.item_count > kMaxListItems) {
        return -1;
    }

    std::vector<int> candidates;
    candidates.reserve(kMaxListItems);

    for (const int item_no : std::span(set.item, set.item_count)) {
        const bool is_attachment = item_no >= ITEM_ATTACH_START && item_no < ITEM_WEAPON_START;
        const bool is_weapon = item_no >= ITEM_WEAPON_START;

        if (small != 0 ? is_attachment : is_weapon) {
            candidates.push_back(item_no);
        }
    }

    if (candidates.empty()) {
        return -1;
    }

    return DrawCandidate(candidates, std::span<const s16>(rates.get(), kRateCount));
}
