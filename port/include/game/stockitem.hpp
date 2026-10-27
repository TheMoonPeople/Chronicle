#pragma once

#include "common.h"

#include "itemdata.hpp"

namespace dc::game {

/**
 * @file
 * Declares the stock inventory records and operations.
 */

/**
 * Identifies a section of the stock inventory.
 */
enum class SearchSpace : int {
    DungeonItems = 0, /**< Identifies the dungeon-item section. */
    Weapons = 1,      /**< Identifies the weapon section. */
    Attach = 2,       /**< Identifies the attachment section. */
};

/**
 * Stores dungeon items, weapons, and attachments.
 */
class CStockItem {
public:
    s16         dungeon_items[60];     /**< Contains the dungeon-item identifiers. */
    s16         dungeon_item_vols[60]; /**< Contains the volume for each dungeon-item slot. */
    WEAPON_HAVE weapons[30];           /**< Contains the weapon data. */
    ATTACH_LIST attachments[30];       /**< Contains the attachment data. */

    /**
     * Sets all stock inventory slots to zero.
     */
    void Initialize();

    /**
     * Returns an item identifier from a slot of a SearchSpace section, or zero
     * if the identifier is not valid for that section.
     */
    int SearchSpace(int slot_index, int search_space);

    /**
     * Exchanges an item identifier and volume with a dungeon-item slot.
     */
    void SetItemToPos(int slot_index, s16 *item_no, s16 *volume);

    /**
     * Copies an item identifier and volume from a dungeon-item slot.
     */
    void GetItemInfo(int slot_index, s16 *item_no, s16 *volume);

    /**
     * Exchanges weapon data with a weapon slot.
     */
    void SetWepToPos(int slot_index, WEAPON_HAVE *weapon);

    /**
     * Copies weapon data from a weapon slot.
     */
    void GetWeaponInfo(int slot_index, WEAPON_HAVE *weapon);

    /**
     * Exchanges attachment data with an attachment slot.
     */
    void SetAttachToPos(int slot_index, ATTACH_LIST *attachment);

    /**
     * Copies attachment data from an attachment slot.
     */
    void GetAttachInfo(int slot_index, ATTACH_LIST *attachment);

    /**
     * Returns 1 if the section matching the item's identifier range holds the
     * item, or 0 otherwise.
     */
    int SearchItem(int item_no);

    /**
     * Sorts dungeon-item slots with the current priority and returns 1 if it
     * moves a slot.
     */
    int SeitonChargeItemBoardSub();

    /**
     * Tries item priorities until a sort operation moves a dungeon-item slot.
     */
    void SeitonItem();

    /**
     * Tries attachment priorities until a sort operation moves an attachment
     * slot.
     */
    void SeitonAttach();

    /**
     * Returns the first empty slot in a SearchSpace section, or -1 if the
     * section is full.
     */
    int GetNowModeSpace(int search_space);
};

static_assert(sizeof(CStockItem) == 0x21C0);

/**
 * Compares two dungeon-item identifiers for the current stock sort order,
 * returning 1, -1 or 0 like a three-way comparison.
 */
int CompChargeItem(int first_item_no, int second_item_no);

} // namespace dc::game
