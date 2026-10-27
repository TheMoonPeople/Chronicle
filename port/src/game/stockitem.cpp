#include "stockitem.hpp"
#include "types.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <utility>

#include "itemdata.hpp"
#include "menu_draw.hpp"

namespace dc::game {

/** Selects the first item type for the next stock sort operation. */
int sort_top_type__3 = ITEM_SORT_FOOD;

/** Selects the first attachment type for the next stock sort operation. */
int asort_top_type__3 = 1;

/** Maps each dungeon-item sort key to its current stock sort priority. */
int sort_table__3[9] = {9, 0, 1, 2, 3, 4, 5, 6, 7};

/** Maps each AttachKind to its current stock sort priority. */
int asort_table__3[5] = {5, 1, 2, 3, 4};

/**
 * Contains the slot limits for each stock inventory section.
 */
struct StockSlotCounts {
    int values[3]; /**< Contains the slot limit for each inventory section. */
};

/** Defines the slot limits for the stock inventory sections. */
StockSlotCounts LIT_459 = {
    {60, 30, 30}
};

namespace {

constexpr int kUnsortedItemPriority = 9;
constexpr int kAttachSortTypeCount = 5;
constexpr int kUnsortedAttachPriority = 5;

/**
 * Compares two attachments with the current stock sort order.
 */
int CompChargeAttach(const ATTACH_LIST &first, const ATTACH_LIST &second) {
    // ATTACHKIND_OTHER (5) indexes one past asort_table__3, into whatever global follows it.
    int first_priority = asort_table__3[GetAttachKind(first.item_no)];
    int second_priority = asort_table__3[GetAttachKind(second.item_no)];

    const int first_item_no = first.item_no;

    if (first_item_no < ITEM_ATTACH_START) {
        first_priority = kUnsortedAttachPriority;
    }

    const int second_item_no = second.item_no;

    if (second_item_no < ITEM_ATTACH_START) {
        second_priority = kUnsortedAttachPriority;
    }

    if (second_priority < first_priority) {
        return 1;
    }

    if (first_priority < second_priority) {
        return -1;
    }

    if (first_item_no > second_item_no) {
        return 1;
    }

    if (first_item_no < second_item_no) {
        return -1;
    }

    return 0;
}

/**
 * Sorts attachment slots with the current priority and returns 1 if it moves a slot.
 */
int SeitonAttachBoardSub(ATTACH_LIST (&attachments)[30]) {
    int sort_type = asort_top_type__3;

    for (int priority = 0; priority < kAttachSortTypeCount; priority++) {
        asort_table__3[sort_type] = priority;
        sort_type++;

        if (sort_type >= kAttachSortTypeCount) {
            sort_type = 0;
        }
    }

    asort_table__3[0] = kUnsortedAttachPriority;

    bool      moved = false;
    const int slot_count = static_cast<int>(std::size(attachments));

    for (int first_slot = 0; first_slot < slot_count - 1; first_slot++) {
        for (int second_slot = first_slot + 1; second_slot < slot_count; second_slot++) {
            if (CompChargeAttach(attachments[first_slot], attachments[second_slot]) > 0) {
                MenuDataSwap(&attachments[first_slot], &attachments[second_slot]);
                moved = true;
            }
        }
    }

    return static_cast<int>(moved);
}

} // namespace

void CStockItem::Initialize() {
    std::ranges::fill(this->dungeon_items, 0);
    std::ranges::fill(this->dungeon_item_vols, 0);
    std::memset(this->weapons, 0, sizeof(this->weapons));
    std::memset(this->attachments, 0, sizeof(this->attachments));
}

int CStockItem::SearchSpace(int slot_index, int search_space) {
    switch (static_cast<game::SearchSpace>(search_space)) {
        case game::SearchSpace::DungeonItems: {
            const int item_no = this->dungeon_items[slot_index];

            if (item_no >= ITEM_DUNGEON_START && item_no <= ITEM_DUNGEON_END) {
                return item_no;
            }

            return 0;
        }
        case game::SearchSpace::Weapons:
            if (this->weapons[slot_index].item_no >= ITEM_WEAPON_START) {
                return this->weapons[slot_index].item_no;
            }

            return 0;
        case game::SearchSpace::Attach: {
            const int item_no = this->attachments[slot_index].item_no;

            if (item_no < ITEM_ATTACH_START) {
                return 0;
            }

            return item_no;
        }
    }

    return 0;
}

void CStockItem::SetItemToPos(int slot_index, s16 *item_no, s16 *volume) {
    std::swap(this->dungeon_items[slot_index], *item_no);
    std::swap(this->dungeon_item_vols[slot_index], *volume);
}

void CStockItem::GetItemInfo(int slot_index, s16 *item_no, s16 *volume) {
    *item_no = this->dungeon_items[slot_index];
    *volume = this->dungeon_item_vols[slot_index];
}

void CStockItem::SetWepToPos(int slot_index, WEAPON_HAVE *weapon) {
    std::swap(this->weapons[slot_index], *weapon);
}

void CStockItem::GetWeaponInfo(int slot_index, WEAPON_HAVE *weapon) {
    *weapon = this->weapons[slot_index];
}

void CStockItem::SetAttachToPos(int slot_index, ATTACH_LIST *attachment) {
    std::swap(this->attachments[slot_index], *attachment);
}

void CStockItem::GetAttachInfo(int slot_index, ATTACH_LIST *attachment) {
    *attachment = this->attachments[slot_index];
}

int CStockItem::SearchItem(int item_no) {
    bool found = false;

    if (item_no >= ITEM_WEAPON_START) {
        found = std::ranges::contains(this->weapons, item_no, &WEAPON_HAVE::item_no);
    } else if (item_no >= ITEM_ATTACH_START && item_no < ITEM_DUNGEON_START) {
        found = std::ranges::contains(this->attachments, item_no, &ATTACH_LIST::item_no);
    } else if (item_no >= ITEM_DUNGEON_START && item_no <= ITEM_DUNGEON_END) {
        found = std::ranges::contains(this->dungeon_items, item_no);
    }

    return static_cast<int>(found);
}

int CompChargeItem(int first_item_no, int second_item_no) {
    const ITEM_DATA *first = GetItemData(first_item_no);
    const ITEM_DATA *second = GetItemData(second_item_no);

    int first_priority = 0;
    int second_priority = 0;

    if (first != nullptr) {
        first_priority = sort_table__3[first->sort_key];
    }

    if (second != nullptr) {
        second_priority = sort_table__3[second->sort_key];
    }

    if (first_item_no < ITEM_DUNGEON_START) {
        first_priority = kUnsortedItemPriority;
    }

    if (second_item_no < ITEM_DUNGEON_START) {
        second_priority = kUnsortedItemPriority;
    }

    if (second_priority < first_priority) {
        return 1;
    }

    if (first_priority < second_priority) {
        return -1;
    }

    if (second_item_no < first_item_no) {
        return 1;
    }

    if (first_item_no < second_item_no) {
        return -1;
    }

    return 0;
}

int CStockItem::SeitonChargeItemBoardSub() {
    int sort_type = sort_top_type__3;

    for (int priority = 0; priority < ITEM_SORT_COUNT; priority++) {
        sort_table__3[sort_type] = priority;
        sort_type++;

        if (sort_type >= ITEM_SORT_COUNT) {
            sort_type = ITEM_SORT_NONE;
        }
    }

    sort_table__3[ITEM_SORT_NONE] = kUnsortedItemPriority;

    bool      moved = false;
    const int slot_count = static_cast<int>(std::size(this->dungeon_items));

    for (int first_slot = 0; first_slot < slot_count - 1; first_slot++) {
        for (int second_slot = first_slot + 1; second_slot < slot_count; second_slot++) {
            if (CompChargeItem(this->dungeon_items[first_slot], this->dungeon_items[second_slot]) > 0) {
                MenuDataSwap(&this->dungeon_items[first_slot], &this->dungeon_items[second_slot]);
                MenuDataSwap(&this->dungeon_item_vols[first_slot], &this->dungeon_item_vols[second_slot]);
                moved = true;
            }
        }
    }

    return static_cast<int>(moved);
}

void CStockItem::SeitonItem() {
    for (int attempt = 0; attempt < ITEM_SORT_COUNT; attempt++) {
        if (SeitonChargeItemBoardSub() != 0) {
            break;
        }

        sort_top_type__3++;

        if (sort_top_type__3 >= ITEM_SORT_COUNT) {
            sort_top_type__3 = ITEM_SORT_FOOD;
        }
    }
}

void CStockItem::SeitonAttach() {
    for (int attempt = 0; attempt < kAttachSortTypeCount; attempt++) {
        if (SeitonAttachBoardSub(this->attachments) != 0) {
            break;
        }

        asort_top_type__3++;

        if (asort_top_type__3 >= kAttachSortTypeCount) {
            asort_top_type__3 = 0;
        }
    }
}

int CStockItem::GetNowModeSpace(int search_space) {
    for (int slot_index = 0; slot_index < LIT_459.values[search_space]; slot_index++) {
        if (SearchSpace(slot_index, search_space) == 0) {
            return slot_index;
        }
    }

    return -1;
}

} // namespace dc::game
