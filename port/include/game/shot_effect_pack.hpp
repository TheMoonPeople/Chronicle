#pragma once

#include "common.h"

#include "bt_shot_effect.hpp"
#include "dataalloc_fwd.hpp"
#include "shot_effect.hpp"

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
        effect[index].Set(position, direction, -1, -1, 0, NULL, -1);
        current_effect = index;
    }

    /**
     * Loads an effect into a free slot, and returns the slot, or -1 when none is free.
     *
     * @mangled Entry__17CSHOT_EFFECT_PACKFP14BT_SHOT_EFFECTPUiiP14CDataAlloc2_1_i
     * @address 0x1AE4C0
     * @size 0x100
     */
    int Entry(BT_SHOT_EFFECT *description, unsigned int *resource, int texture_block, CDataAlloc2<1> *allocator, int slots);

    /**
     * @mangled SetUserID2__17CSHOT_EFFECT_PACKFi
     * @address 0x1AE5C0
     * @size 0x44
     * @unknownret
     */
    void SetUserID2(int id);

    /**
     * @mangled SetDmg__17CSHOT_EFFECT_PACKFi
     * @address 0x1AE610
     * @size 0x44
     * @unknownret
     */
    void SetDmg(int damage);
};

STATIC_ASSERT(sizeof(CSHOT_EFFECT_PACK) == 0x326F0);
