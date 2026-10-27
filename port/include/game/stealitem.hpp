#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

/**
 * Steps of a stolen item flying back, as CStealItem::state holds them.
 */
// clang-format off
enum class StealItemState : s32 {
    None    = -1, /**< Free. */
    Rise    = 0,  /**< Arcing toward the player. */
    Home    = 1,  /**< Flying straight to the player. */
    Arrived = 2,  /**< Arrived. */
};

// clang-format on

class CFrame;
class CFrameVu1;

/** Slots the thief carries away at once. */
inline constexpr int STEAL_ITEM_MAX = 8;

/**
 * Carries away the items a monster stole. Each slot rises out of the player,
 * turns towards them, and is taken back once it reaches them.
 */
class CStealItem {
public:
    CFrame        *frame; /**< Model that every slot draws with. */
    u8             unk_004[0xC];
    sceVu0FVECTOR  pos[STEAL_ITEM_MAX];         /**< Where each slot is. */
    float          base_height[STEAL_ITEM_MAX]; /**< Height each slot bobs about. */
    float          speed[STEAL_ITEM_MAX];       /**< How far each slot travels a step. */
    StealItemState state[STEAL_ITEM_MAX];       /**< Step each slot is at. */
    s32            aux_state[STEAL_ITEM_MAX];   /**< Second per-slot state, reset to -1 with state and otherwise unused. */
    float          angle;                       /**< Angle every slot spins at. */
    float          phase[STEAL_ITEM_MAX];       /**< How far through its bob each slot is. */
    s32            item[STEAL_ITEM_MAX];        /**< Item each slot carries. */
    u8             unk_154[0xC];

    /**
     * Frees every slot and gives them the model they draw with.
     */
    void Initialize(CFrameVu1 *model);

    /**
     * Starts one item off at a position, in the first free slot. Nothing
     * happens once every slot is taken.
     */
    void Set(float *position, int item_no);

    /**
     * Moves every taken slot one step towards the player.
     */
    void Step();

    /**
     * Draws every taken slot.
     */
    void Draw();

    /**
     * Frees the first slot that has reached the player and returns the item
     * it carried, or -1 while none has.
     */
    int checkEvent();
};

STATIC_ASSERT(sizeof(CStealItem) == 0x160);

} // namespace dc::game
