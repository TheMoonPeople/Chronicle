#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Steps of a stolen item flying back, as CStealItem::state holds them.
 */
// clang-format off
enum StealItemState {
    STEAL_ITEM_NONE    = -1, /**< Free. */
    STEAL_ITEM_RISE    = 0,  /**< Arcing toward the player. */
    STEAL_ITEM_HOME    = 1,  /**< Flying straight to the player. */
    STEAL_ITEM_ARRIVED = 2,  /**< Arrived. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrame;
class CFrameVu1;

/** Slots the thief carries away at once. */
#define STEAL_ITEM_MAX 8

/**
 * Carries away the items a monster stole. Each slot rises out of the player,
 * turns towards them, and is taken back once it reaches them.
 */
class CStealItem {
public:
    CFrame       *frame; /**< Model that every slot draws with. */
    u8            unk_004[0xC];
    sceVu0FVECTOR pos[STEAL_ITEM_MAX];         /**< Where each slot is. */
    float         base_height[STEAL_ITEM_MAX]; /**< Height each slot bobs about. */
    float         speed[STEAL_ITEM_MAX];       /**< How far each slot travels a step. */
    s32           state[STEAL_ITEM_MAX];       /**< -1 free, 0 rising, 1 closing, 2 arrived. */
    s32           aux_state[STEAL_ITEM_MAX];   /**< Second per-slot state, reset to -1 with state and otherwise unused. */
    float         angle;                       /**< Angle every slot spins at. */
    float         phase[STEAL_ITEM_MAX];       /**< How far through its bob each slot is. */
    s32           item[STEAL_ITEM_MAX];        /**< Item each slot carries. */
    u8            unk_154[0xC];

    /**
     * Frees every slot and gives them the model they draw with.
     *
     * @mangled Initialize__10CStealItemFP9CFrameVu1
     * @address 0x1D74E0
     * @size 0x3C
     */
    void Initialize(CFrameVu1 *model);

    /**
     * Starts one item off at a position, in the first free slot. Nothing
     * happens once every slot is taken.
     *
     * @mangled Set__10CStealItemFPfi
     * @address 0x1D7520
     * @size 0xD4
     */
    void Set(float *position, int item_no);

    /**
     * Moves every taken slot one step towards the player.
     *
     * @mangled Step__10CStealItemFv
     * @address 0x1D7600
     * @size 0x340
     */
    void Step();

    /**
     * Draws every taken slot.
     *
     * @mangled Draw__10CStealItemFv
     * @address 0x1D7940
     * @size 0xA0
     */
    void Draw();

    /**
     * Frees the first slot that has reached the player and returns the item
     * it carried, or -1 while none has.
     *
     * @mangled checkEvent__10CStealItemFv
     * @address 0x1D79E0
     * @size 0x60
     */
    int checkEvent();
};

STATIC_ASSERT(sizeof(CStealItem) == 0x160);
