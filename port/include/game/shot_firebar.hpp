#pragma once

#include "common.h"

/**
 *
 * Manages the particle state and rendering for Ozumond's fire stream.
 *
 */
class CSHOT_FIREBAR {
public:
    float position[64][4];      /**< World positions for the fire particles. */
    float velocity[64][4];      /**< Per-step movement vectors for the fire particles. */
    float size[64];             /**< Display sizes for the fire particles. */
    float opacity[63];          /**< Current opacity values for the fire particles. */
    s32   init_damage;          /**< Collision damage given when the stream was initialized. */
    s32   damage[63];           /**< Collision damage values for the fire particles. */
    s32   init_element;         /**< Element given when the stream was initialized. */
    s32   particle_element[64]; /**< Weapon element of each fire particle, which picks its texture cell. */
    s32   state[64];            /**< Activity states for the fire particles. */
    s32   start_index;          /**< First particle slot that initialization lays the stream out from. */
    u8    unk_D04[0xC];

    /**
     *
     * Initializes a 24-particle fire stream from a position and direction.
     *
     * @mangled Init__13CSHOT_FIREBARFPfPfii
     * @address 0x1AEB20
     * @size 0x21C
     */
    int Init(float *origin, float *direction, int collision_damage, int element);

    /**
     *
     * Emits a 24-particle fire stream from a position and direction.
     *
     * @mangled Set__13CSHOT_FIREBARFPfPfii
     * @address 0x1AED40
     * @size 0x21C
     */
    int Set(float *origin, float *direction, int collision_damage, int element);

    /**
     *
     * Marks the first 24 fire-particle slots as inactive.
     *
     * @mangled Rset__13CSHOT_FIREBARFv
     * @address 0x1AEF60
     * @size 0x34
     */
    void Rset();

    /**
     *
     * Advances active fire particles and applies their periodic collision damage.
     *
     * @mangled Step__13CSHOT_FIREBARFv
     * @address 0x1AEFA0
     * @size 0x238
     */
    void Step();

    /**
     *
     * Draws the active fire particles with their selected texture cells.
     *
     * @mangled Draw__13CSHOT_FIREBARFv
     * @address 0x1AF1E0
     * @size 0x180
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CSHOT_FIREBAR) == 0xD10);
