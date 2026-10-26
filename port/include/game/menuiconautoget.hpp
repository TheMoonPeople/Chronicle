#pragma once

#include "common.h"

/**
 * Describes one item icon flying from where the party picked it up to the
 * inventory slot it lands in.
 */
struct MENU_AUTO_GET_ICON {
    float x;      /**< Screen x the icon draws at, eased toward the slot. */
    float y;      /**< Screen y the icon draws at, eased toward the slot. */
    s16   item;   /**< Item the icon stands for; not positive while the slot is free. */
    s16   slot;   /**< Quick item slot the icon is flying to. */
    s16   volume; /**< Item volume the slot takes when the icon lands. */
    u8    unk_0E[2];
};

STATIC_ASSERT(sizeof(MENU_AUTO_GET_ICON) == 0x10);

/**
 * Flies the icons of newly collected items into the inventory.
 */
class CMenuIconAutoGet {
public:
    MENU_AUTO_GET_ICON icon[3]; /**< Icons that can be in flight at once. */

    /**
     * Reports whether an icon is still flying to its slot.
     *
     * @mangled IsMoveIcon__16CMenuIconAutoGetFv
     * @address 0x2380B0
     * @size 0x40
     */
    int IsMoveIcon();

    /**
     * Counts the icons already flying with one item.
     *
     * @mangled IsSameItem__16CMenuIconAutoGetFi
     * @address 0x2380F0
     * @size 0x40
     */
    int IsSameItem(int item);

    /**
     * Counts the icons flying to one slot with one item.
     *
     * @mangled GetMoveIconGole__16CMenuIconAutoGetFii
     * @address 0x238130
     * @size 0x4C
     */
    int GetMoveIconGole(int slot, int item);

    /**
     * Gives a free slot among the three flying icons, or -1.
     *
     * @mangled GetSpace__16CMenuIconAutoGetFv
     * @address 0x238180
     * @size 0x48
     */
    int GetSpace();

    /**
     * Advances the flying icons, puts what lands into the inventory, and reports activity.
     *
     * @mangled IconAutoMove__16CMenuIconAutoGetFv
     * @address 0x2381D0
     * @size 0x18C
     */
    int IconAutoMove();

    /**
     * Starts one icon flying from a position to a slot.
     *
     * @mangled IconMoveTarSet__16CMenuIconAutoGetFiiiiff
     * @address 0x238360
     * @size 0x24
     */
    void IconMoveTarSet(int index, int slot, int item, int count, float x, float y);

    /**
     * Draws the icons part-way through their flight.
     *
     * @mangled IconAutoMoveDraw__16CMenuIconAutoGetFv
     * @address 0x238390
     * @size 0xBC
     */
    void IconAutoMoveDraw();
};
