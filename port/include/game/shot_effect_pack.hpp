#pragma once

#include "common.h"

#include "bt_shot_effect.hpp"
#include "dataalloc_fwd.hpp"
#include "shot_effect.hpp"

namespace dc::game {

/**
 * Holds the projectile effects the dungeon can run, and which one is being set up.
 */
class CSHOT_EFFECT_PACK {
public:
    CSHOT_EFFECT effect[5]; /**< The effects the dungeon can run. */

    s32 current_effect; /**< Effect that SetUserID2 and SetDmg apply to. */

    /**
     * Fires one effect from a position in a direction, and selects it.
     */
    void Set(int index, float *position, float *direction) {
        effect[index].Set(position, direction, -1, -1, 0, nullptr, -1);
        current_effect = index;
    }

    /**
     * Loads an effect into a free slot and returns the slot, the slot already holding it, or -1
     * when none is free.
     */
    int Entry(BT_SHOT_EFFECT *description, unsigned int *resource, int texture_block, CDataAlloc2<1> *allocator, int slots);

    /**
     * Sets the secondary user identifier of the selected effect, if one is selected.
     */
    void SetUserID2(int id);

    /**
     * Sets the damage dealt by the selected effect, if one is selected.
     */
    void SetDmg(int damage);
};

STATIC_ASSERT(sizeof(CSHOT_EFFECT_PACK) == 0x326F0);

} // namespace dc::game
