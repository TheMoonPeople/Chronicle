#pragma once

#include "common.h"

/**
 * Accumulates elapsed play time and applies it to time-sensitive inventory items.
 */
class CMenuItemStep {
public:
    s32 frame;             /**< Frames accumulated toward the next inventory-volume step. */
    s32 last_slot;         /**< Inventory slot last stepped; reset to -1. */
    s16 last_item;         /**< Item last stepped; reset to -1. */
    s16 dungeon_item_held; /**< Set once the checked character holds a dungeon item. */
    s16 enabled;           /**< Enables periodic inventory-volume accumulation. */
    s16 chara_no;          /**< Character whose held item CheckItemVolume inspects. */
    s16 held_item[4];      /**< Item each character holds, checked for dungeon items. */
    s16 chara_slot[4];     /**< Slot recorded for each character; reset to -1. */
    s16 chara_item[4];     /**< Item recorded for each character; reset to -1. */
    s32 pending_volume;    /**< Inventory-volume steps waiting to be applied. */

    /**
     * Resets item-volume timing and per-character state.
     *
     * @mangled Initialize__13CMenuItemStepFv
     * @address 0x235650
     * @size 0x7C
     */
    void Initialize();

    /**
     * Accumulates one frame toward the next inventory-volume step.
     *
     * @mangled LoopStep__13CMenuItemStepFi
     * @address 0x2356D0
     * @size 0x4C
     */
    void LoopStep(int interval);

    /**
     * Applies pending volume loss to time-sensitive inventory items.
     *
     * @mangled CheckItemVolume__13CMenuItemStepFv
     * @address 0x235720
     * @size 0x2EC
     */
    void CheckItemVolume();
};

STATIC_ASSERT(sizeof(CMenuItemStep) == 0x2C);
