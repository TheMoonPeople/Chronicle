#pragma once

#include "common.h"

/**
 * Animates the ring of healing particles around the player character.
 */
class CHealEffect {
public:
    float position[4];            /**< World-space centre of the effect. */
    float particle_offset[32][4]; /**< Rotated offset of each healing particle. */
    float angle[32];              /**< Rotation angle of each particle around the centre. */
    float angular_velocity[32];   /**< Per-step angular advance of each particle. */
    float radius[32];             /**< Horizontal orbit radius of each particle. */
    float alpha[32];              /**< Current opacity of each particle. */
    float phase[32];              /**< Lifetime phase controlling height and opacity. */
    float size[32];               /**< Draw size of each particle. */
    s32   active;                 /**< Indicates that the particle effect is advancing. */
    u8    unk_514[0xC];

    /**
     * Starts a healing-particle effect at the supplied world position.
     *
     * @mangled Set__11CHealEffectFPf
     * @address 0x1B2900
     * @size 0x1F8
     */
    void Set(float *world);

    /**
     * Advances every active healing particle and ends the effect when all expire.
     *
     * @mangled Step__11CHealEffectFv
     * @address 0x1B2B00
     * @size 0x244
     */
    void Step();

    /**
     * Draws every live healing particle around the player character.
     *
     * @mangled Draw__11CHealEffectFv
     * @address 0x1B2D50
     * @size 0x13C
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CHealEffect) == 0x520);
