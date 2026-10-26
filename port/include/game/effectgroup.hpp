#pragma once

#include "common.h"

class CEffect;
class CEffectParam;

/**
 * Owns access to a caller-supplied fixed-capacity pool of effects.
 */
class CEffectGroup {
public:
    CEffect *effect_table; /**< First effect in the caller-supplied pool. */
    int      capacity;     /**< Number of effects in the pool. */

    /** Starts the group with no pool. */
    CEffectGroup() { Initialize(0, 0); }

    /**
     * Starts an effect in the first inactive pool slot.
     *
     * @mangled EnterEffect__12CEffectGroupFP12CEffectParam
     * @address 0x164980
     * @size 0x6C
     */
    int EnterEffect(CEffectParam *parameters);

    /**
     * Advances every pool slot by one frame.
     *
     * @mangled Step__12CEffectGroupFi
     * @address 0x1649F0
     * @size 0x64
     */
    void Step(int unused);

    /**
     * Draws every active effect in the pool.
     *
     * @mangled Draw__12CEffectGroupFv
     * @address 0x164A60
     * @size 0x60
     */
    void Draw();

    /**
     * Resets every effect in the pool to its inactive state.
     *
     * @mangled Clear__12CEffectGroupFv
     * @address 0x164AC0
     * @size 0x60
     */
    void Clear();

    /**
     * Assigns the storage and capacity used by the effect pool.
     *
     * @mangled Initialize__12CEffectGroupFP7CEffecti
     * @address 0x164B20
     * @size 0x10
     */
    void Initialize(CEffect *table, int count);
};

STATIC_ASSERT(sizeof(CEffectGroup) == 0x8);
