#pragma once

#include "common.h"

// WEAPON_HAVE and ATTACH_LIST live with the definition tables they are copied
// from; CStockItem stores both by value, so it needs the complete types.
#include "itemdata.hpp"

/**
 * @file
 * Declares the stock inventory records and operations.
 */

/**
 * Identifies a section of the stock inventory.
 */
// clang-format off
enum SearchSpace {
    SEARCHSPACE_DUNGEON_ITEMS = 0, /**< Identifies the dungeon-item section. */
    SEARCHSPACE_WEAPONS       = 1, /**< Identifies the weapon section. */
    SEARCHSPACE_ATTACH        = 2, /**< Identifies the attachment section. */
};

// clang-format on

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
     *
     * @mangled Initialize__10CStockItemFv
     * @address 0x23F590
     * @size 0x70
     */
    void Initialize();

    /**
     * Returns an item identifier from a slot or zero if the identifier is not valid for the selected section.
     *
     * @mangled SearchSpace__10CStockItemFii
     * @address 0x23F600
     * @size 0xBC
     */
    int SearchSpace(int slot_index, int search_space);

    /**
     * Exchanges an item identifier and volume with a dungeon-item slot.
     *
     * @mangled SetItemToPos__10CStockItemFiPsPs
     * @address 0x23F6C0
     * @size 0x30
     */
    void SetItemToPos(int slot_index, s16 *item_no, s16 *volume);

    /**
     * Copies an item identifier and volume from a dungeon-item slot.
     *
     * @mangled GetItemInfo__10CStockItemFiPsPs
     * @address 0x23F6F0
     * @size 0x20
     */
    void GetItemInfo(int slot_index, s16 *item_no, s16 *volume);

    /**
     * Exchanges weapon data with a weapon slot.
     *
     * @mangled SetWepToPos__10CStockItemFiP11WEAPON_HAVE
     * @address 0x23F710
     * @size 0x7C
     */
    void SetWepToPos(int slot_index, WEAPON_HAVE *weapon);

    /**
     * Copies weapon data from a weapon slot.
     *
     * @mangled GetWeaponInfo__10CStockItemFiP11WEAPON_HAVE
     * @address 0x23F790
     * @size 0x3C
     */
    void GetWeaponInfo(int slot_index, WEAPON_HAVE *weapon);

    /**
     * Exchanges attachment data with an attachment slot.
     *
     * @mangled SetAttachToPos__10CStockItemFiP11ATTACH_LIST
     * @address 0x23F7D0
     * @size 0x74
     */
    void SetAttachToPos(int slot_index, ATTACH_LIST *attachment);

    /**
     * Copies attachment data from an attachment slot.
     *
     * @mangled GetAttachInfo__10CStockItemFiP11ATTACH_LIST
     * @address 0x23F850
     * @size 0x34
     */
    void GetAttachInfo(int slot_index, ATTACH_LIST *attachment);

    /**
     * Searches the stock inventory for an item identifier and prints the result.
     *
     * @mangled SearchItem__10CStockItemFi
     * @address 0x23F890
     * @size 0x18C
     */
    int SearchItem(int item_no);

    /**
     * Sorts dungeon-item slots with the current priority and returns 1 if it moves a slot.
     *
     * @mangled SeitonChargeItemBoardSub__10CStockItemFv
     * @address 0x23FB40
     * @size 0x12C
     */
    int SeitonChargeItemBoardSub();

    /**
     * Tries item priorities until a sort operation moves a dungeon-item slot.
     *
     * @mangled SeitonItem__10CStockItemFv
     * @address 0x23FC70
     * @size 0x80
     */
    void SeitonItem();

    /**
     * Tries attachment priorities until a sort operation moves an attachment slot.
     *
     * @mangled SeitonAttach__10CStockItemFv
     * @address 0x23FF10
     * @size 0x7C
     */
    void SeitonAttach();

    /**
     * Returns the first empty slot in an inventory section or minus one if the section is full.
     *
     * @mangled GetNowModeSpace__10CStockItemFi
     * @address 0x23FF90
     * @size 0xB4
     */
    int GetNowModeSpace(int search_space);
};

STATIC_ASSERT(sizeof(CStockItem) == 0x21C0);

/**
 * Compares two dungeon-item identifiers for the current stock sort order.
 *
 * @mangled CompChargeItem__Fii
 * @address 0x23FA20
 * @size 0x11C
 */
int CompChargeItem(int first_item_no, int second_item_no);
