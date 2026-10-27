#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

class CCamera;
class CUserStatus;

/**
 * Animates the blast billboards created by one bomb.
 */
class CItemBombEffect {
public:
    sceVu0FVECTOR positions[5]; /**< World positions of the blast billboards. */
    s32           phases[5];    /**< Envelope phase run by each billboard. */
    s32           counters[5];  /**< Frames elapsed in each billboard's phase; negative while its start is delayed. */
    float         sizes[5];     /**< World-space widths of the billboards. */
    float         alphas[5];    /**< Alpha values used to draw the billboards. */
    s32           active[5];    /**< Nonzero while each billboard is animated. */
    float         scale;        /**< Scale applied to every billboard. */
    u8            unk_B8[0x8];

    /**
     * Draws the bomb's blast billboards facing the camera.
     */
    void Draw(CCamera *camera);

    /**
     * Advances every blast billboard's envelope by a frame.
     */
    void Step();

    /**
     * Places the bomb's five blast puffs at a position, staggering their starts.
     */
    void SetBomb(float *position, float scale);

    /**
     * Returns 1 while any blast billboard is still animated, otherwise 0.
     */
    int CheckBomb();

    /**
     * Deactivates every blast billboard.
     */
    void Initialize();
};

/**
 * Expands and fades the ring of a large bomb's shock wave.
 */
class CShockWave {
public:
    sceVu0FVECTOR position;      /**< World position at the centre of the ring. */
    float         expand_radius; /**< Radius the ring grows by at the peak of its expansion. */
    float         base_radius;   /**< Ring radius before its expansion is applied. */
    float         radius;        /**< Current world-space radius of the ring. */
    float         expand_steps;  /**< Number of frames taken to complete expansion. */
    float         phase;         /**< Sine-envelope phase of the expansion. */
    float         alpha;         /**< Alpha value used to draw the ring. */
    s32           active;        /**< Nonzero while the ring expands and draws. */
    u8            unk_2C[0x4];

    /**
     * Draws the expanding shock-wave ring flat around its centre.
     */
    void Draw(CCamera *camera);

    /**
     * Expands and fades the shock-wave ring by a frame.
     */
    void Step();
};

STATIC_ASSERT(sizeof(CShockWave) == 0x30);

/**
 * Applies the effects of using an item to the current character.
 */
void usedActiveItem(CUserStatus *status, int item);

/**
 * Returns 1 if the item in a quick-use slot would have an effect if used now, otherwise 0.
 */
int checkItemUsed(int slot);

/**
 * Starts a bomb effect at a position and returns its collision slot, or -1.
 */
int SetBombEffect(float *position, int owner, int damage, float scale);

STATIC_ASSERT(sizeof(CItemBombEffect) == 0xC0);

} // namespace dc::game
