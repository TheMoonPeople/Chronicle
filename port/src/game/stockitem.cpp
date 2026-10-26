#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 459

#include "stockitem.hpp"
#include "types.h"

#include <cstdio>
#include <cstring>

#include "itemdata.hpp"
#include "menu_draw.hpp"

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

void CStockItem::Initialize() {
    memset(this->dungeon_items, 0, sizeof(this->dungeon_items));
    memset(this->dungeon_item_vols, 0, sizeof(this->dungeon_item_vols));
    memset(this->weapons, 0, sizeof(this->weapons));
    memset(this->attachments, 0, sizeof(this->attachments));
}

int CStockItem::SearchSpace(int slot_index, int search_space) {
    int item_no = 0;

    switch (search_space) {
        case SEARCHSPACE_DUNGEON_ITEMS:
            item_no = this->dungeon_items[slot_index];

            if ((item_no >= ITEM_DUNGEON_START) && (item_no <= ITEM_DUNGEON_END)) {
                return item_no;
            }

            item_no = 0;
            break;
        case SEARCHSPACE_WEAPONS:
            if (this->weapons[slot_index].item_no >= ITEM_WEAPON_START) {
                item_no = this->weapons[slot_index].item_no;
            }

            break;
        case SEARCHSPACE_ATTACH:
            item_no = this->attachments[slot_index].item_no;

            if (item_no < ITEM_ATTACH_START) {
                item_no = 0;
                return item_no;
            }

            break;
    }

    return item_no;
}

void CStockItem::SetItemToPos(int slot_index, s16 *item_no, s16 *volume) {
    s16 *item_slot = &this->dungeon_items[slot_index];
    s16  old_item_no = *item_slot;
    *item_slot = *item_no;
    *item_no = old_item_no;

    s16 *volume_slot = &this->dungeon_item_vols[slot_index];
    s16  old_volume = *volume_slot;
    *volume_slot = *volume;
    *volume = old_volume;
}

void CStockItem::GetItemInfo(int slot_index, s16 *item_no, s16 *volume) {
    *item_no = this->dungeon_items[slot_index];
    *volume = this->dungeon_item_vols[slot_index];
}

void CStockItem::SetWepToPos(int slot_index, WEAPON_HAVE *weapon) {
    WEAPON_HAVE *slot = &this->weapons[slot_index];
    WEAPON_HAVE  old_weapon;
    memcpy(&old_weapon, slot, sizeof(WEAPON_HAVE));
    memcpy(slot, weapon, sizeof(WEAPON_HAVE));
    memcpy(weapon, &old_weapon, sizeof(WEAPON_HAVE));
}

void CStockItem::GetWeaponInfo(int slot_index, WEAPON_HAVE *weapon) {
    memcpy(weapon, &this->weapons[slot_index], sizeof(WEAPON_HAVE));
}

void CStockItem::SetAttachToPos(int slot_index, ATTACH_LIST *attachment) {
    ATTACH_LIST *slot = &this->attachments[slot_index];
    ATTACH_LIST  old_attachment;
    memcpy(&old_attachment, slot, sizeof(ATTACH_LIST));
    memcpy(slot, attachment, sizeof(ATTACH_LIST));
    memcpy(attachment, &old_attachment, sizeof(ATTACH_LIST));
}

void CStockItem::GetAttachInfo(int slot_index, ATTACH_LIST *attachment) {
    memcpy(attachment, &this->attachments[slot_index], sizeof(ATTACH_LIST));
}

int CStockItem::SearchItem(int item_no) {
    if (item_no < ITEM_ATTACH_START) {
        return 0;
    }

    int found = false;

    if ((item_no > 0 && item_no < ITEM_ATTACH_START) || item_no >= ITEM_WEAPON_START) {
        for (int slot_index = 0; slot_index < 30; slot_index++) {
            if (this->weapons[slot_index].item_no == item_no) {
                found = true;
                break;
            }
        }
    } else if (item_no >= ITEM_ATTACH_START && item_no < ITEM_DUNGEON_START) {
        for (int slot_index = 0; slot_index < 30; slot_index++) {
            if (this->attachments[slot_index].item_no == item_no) {
                found = true;
                break;
            }
        }
    } else if (item_no >= ITEM_DUNGEON_START && item_no <= ITEM_DUNGEON_END) {
        for (int slot_index = 0; slot_index < 60; slot_index++) {
            if (this->dungeon_items[slot_index] == item_no) {
                found = true;
                break;
            }
        }
    }

    return found;
}

int CompChargeItem(int first_item_no, int second_item_no) {
    ITEM_DATA *first = GetItemData(first_item_no);
    ITEM_DATA *second = GetItemData(second_item_no);

    int first_priority = 0;
    int second_priority = 0;

    if (first != NULL) {
        first_priority = sort_table__3[first->sort_key];
    }

    if (second != NULL) {
        second_priority = sort_table__3[second->sort_key];
    }

    if (first_item_no < ITEM_DUNGEON_START) {
        first_priority = 9;
    }

    if (second_item_no < ITEM_DUNGEON_START) {
        second_priority = 9;
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
    int first_slot;
    int second_slot;
    int moved;

    for (first_slot = 0; first_slot < ITEM_SORT_COUNT; first_slot++) {
        sort_table__3[sort_type] = first_slot;
        sort_type++;

        if (sort_type >= ITEM_SORT_COUNT) {
            sort_type = ITEM_SORT_NONE;
        }
    }

    sort_table__3[ITEM_SORT_NONE] = 9;

    moved = false;

    for (first_slot = 0; first_slot < 59; first_slot++) {
        for (second_slot = first_slot + 1; second_slot < 60; second_slot++) {
            if (CompChargeItem(this->dungeon_items[first_slot], this->dungeon_items[second_slot]) > 0) {
                MenuDataSwap(&this->dungeon_items[first_slot], &this->dungeon_items[second_slot]);
                MenuDataSwap(&this->dungeon_item_vols[first_slot], &this->dungeon_item_vols[second_slot]);
                moved = true;
            }
        }
    }

    return moved;
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

/**
 * Compares two attachments with the current stock sort order.
 */
static int CompChargeAttach(ATTACH_LIST *first, ATTACH_LIST *second) {
    int first_priority = asort_table__3[GetAttachKind(first->item_no)];
    int second_priority = asort_table__3[GetAttachKind(second->item_no)];
    int second_item_no;
    int first_item_no;

    first_item_no = first->item_no;

    if (ITEM_ATTACH_START > first_item_no) {
        first_priority = 5;
    }

    second_item_no = second->item_no;

    if (ITEM_ATTACH_START > second_item_no) {
        second_priority = 5;
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
static int SeitonAttachBoardSub(ATTACH_LIST *attachments) {
    int sort_type = asort_top_type__3;
    int first_slot;
    int second_slot;
    int moved;

    for (first_slot = 0; first_slot < 5; first_slot++) {
        asort_table__3[sort_type] = first_slot;
        sort_type++;

        if (sort_type >= 5) {
            sort_type = 0;
        }
    }

    asort_table__3[0] = 5;
    moved = false;

    for (first_slot = 0; first_slot < 29; first_slot++) {
        for (second_slot = first_slot + 1; second_slot < 30; second_slot++) {
            if (CompChargeAttach(&attachments[first_slot], &attachments[second_slot]) > 0) {
                MenuDataSwap(&attachments[first_slot], &attachments[second_slot]);
                moved = true;
            }
        }
    }

    return moved;
}

void CStockItem::SeitonAttach() {
    for (int attempt = 0; attempt < 5; attempt++) {
        if (SeitonAttachBoardSub(this->attachments) != 0) {
            break;
        }

        asort_top_type__3++;

        if (asort_top_type__3 >= 5) {
            asort_top_type__3 = 0;
        }
    }
}

int CStockItem::GetNowModeSpace(int search_space) {
    int             empty_slot = -1;
    StockSlotCounts slot_counts = LIT_459;

    for (int slot_index = 0; slot_index < slot_counts.values[search_space]; slot_index++) {
        if (SearchSpace(slot_index, search_space) == 0) {
            empty_slot = slot_index;
            break;
        }
    }

    return empty_slot;
}
