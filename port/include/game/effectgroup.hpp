#pragma once

#include "common.h"

namespace dc::game {

class CEffect;
class CEffectParam;

/**
 * Owns access to a caller-supplied fixed-capacity pool of effects.
 */
class CEffectGroup {
public:
    CEffect *effect_table; /**< First effect in the caller-supplied pool. */
    int      capacity;     /**< Number of effects in the pool. */

    /**
     * Starts the group with no pool.
     */
    CEffectGroup() { Initialize(nullptr, 0); }

    /**
     * Starts an effect in the first inactive pool slot.
     * Returns 1 when a slot was found and 0 when the pool is full.
     */
    int EnterEffect(CEffectParam *parameters);

    /**
     * Advances every pool slot by one frame.
     */
    void Step(int unused);

    /**
     * Draws every active effect in the pool.
     */
    void Draw();

    /**
     * Resets every effect in the pool to its inactive state.
     */
    void Clear();

    /**
     * Assigns the storage and capacity used by the effect pool.
     */
    void Initialize(CEffect *table, int count);
};

STATIC_ASSERT(sizeof(CEffectGroup) == 0x8);

} // namespace dc::game
