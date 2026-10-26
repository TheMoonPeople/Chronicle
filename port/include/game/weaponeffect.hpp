#pragma once

#include "common.h"

#include <libvu0.h>

#include "rect.hpp"

/**
 * @file
 * Declares the trail that a swung weapon leaves behind it.
 */

/** Number of steps of the swing that the trail keeps. */
#define WEAPON_EFFECT_STEP_MAX 32

class CFrame;

/**
 * Draws the ribbon a swung weapon leaves behind it, by remembering where the
 * two ends of the blade stood on each of the last few steps and filling the
 * gap between one step and the next.
 */
class CWeaponEffect {
public:
    CFrame *root; /**< Node at the end of the blade nearer the hand. */
    CFrame *tip;  /**< Node at the far end of the blade. */
    u8      unk_008[8];

    /**
     * Where the two ends of the blade stood on each step of the swing: the
     * near end of a step at twice its number, the far end just after it.
     */
    sceVu0FVECTOR edge[WEAPON_EFFECT_STEP_MAX * 2];
    float         alpha[WEAPON_EFFECT_STEP_MAX]; /**< How brightly each step still draws. */
    s32           used[WEAPON_EFFECT_STEP_MAX];  /**< 1 while the step of the slot still draws. */

    /**
     * Colours of the four corners of one piece of the ribbon: the near and far
     * ends of the step before, then of the step after.
     */
    spRGBA colour[4];
    s32    cursor;   /**< Slot the next step is written to. */
    s32    timer;    /**< Steps the swing has run for. */
    s32    hold;     /**< Steps the trail is laid down at full strength for. */
    float  strength; /**< Brightness a new step is laid down at. */
    float  fade_out; /**< Brightness the trail stops laying down each step, once held. */
    float  fade;     /**< Brightness every step already laid down loses each step. */
    u8     unk_538[8];

    /**
     * Points the trail at the two ends of one weapon's blade.
     *
     * @mangled InitSet__13CWeaponEffectFP6CFramePcPc
     * @address 0x1D65C0
     * @size 0x74
     */
    void InitSet(CFrame *frame, char *root_name, char *tip_name);

    /**
     * Starts a swing, and lays down its first step.
     *
     * @mangled Set__13CWeaponEffectFiff
     * @address 0x1D6640
     * @size 0x120
     */
    void Set(int hold, float fade, float fade_out);

    /**
     * Lays down one more step of the swing and fades the ones behind it.
     *
     * @mangled Step__13CWeaponEffectFv
     * @address 0x1D6760
     * @size 0x1D8
     */
    void Step();

    /**
     * Fills the gap between each pair of steps the swing still holds.
     *
     * @mangled Draw__13CWeaponEffectFv
     * @address 0x1D6940
     * @size 0x298
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CWeaponEffect) == 0x540);
