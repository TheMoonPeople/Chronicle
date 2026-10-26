#pragma once

#include "common.h"

#include <libvu0.h>

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
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
     * Draws the bomb's blast and its shock wave.
     *
     * @mangled Draw__15CItemBombEffectFP7CCamera
     * @address 0x1D5B30
     * @size 0x374
     */
    void Draw(CCamera *camera);

    /**
     * Advances the bomb effect by a frame.
     *
     * @mangled Step__15CItemBombEffectFv
     * @address 0x1D5EB0
     * @size 0x1F0
     */
    void Step();

    /**
     * Places the bomb's five blast puffs around a position.
     *
     * @mangled SetBomb__15CItemBombEffectFPff
     * @address 0x1D60A0
     * @size 0xBC
     */
    void SetBomb(float *position, float scale);

    /**
     * Reports whether the bomb effect is still running.
     *
     * @mangled CheckBomb__15CItemBombEffectFv
     * @address 0x1D6160
     * @size 0x48
     */
    int CheckBomb();

    /**
     * Clears the bomb effect.
     *
     * @mangled Initialize__15CItemBombEffectFv
     * @address 0x1D61B0
     * @size 0x30
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
     * Draws the expanding shock-wave ring.
     *
     * @mangled Draw__10CShockWaveFP7CCamera
     * @address 0x1D61E0
     * @size 0x2F8
     */
    void Draw(CCamera *camera);

    /**
     * Expands and fades the shock-wave ring by a frame.
     *
     * @mangled Step__10CShockWaveFv
     * @address 0x1D64E0
     * @size 0xD8
     */
    void Step();
};

STATIC_ASSERT(sizeof(CShockWave) == 0x30);

/**
 * Spends one use of a running item.
 *
 * @mangled usedActiveItem__FP11CUserStatusi
 * @address 0x1D56F0
 * @size 0x248
 */
void usedActiveItem(CUserStatus *status, int item);

/**
 * Reports whether one running item is still in use.
 *
 * @mangled checkItemUsed__Fi
 * @address 0x1D5580
 * @size 0x16C
 */
int checkItemUsed(int slot);

/**
 * Starts a bomb effect at a position and returns its collision slot.
 *
 * @mangled SetBombEffect__FPfiif
 * @address 0x1D5940
 * @size 0x1F0
 */
int SetBombEffect(float *position, int owner, int damage, float scale);

STATIC_ASSERT(sizeof(CItemBombEffect) == 0xC0);
