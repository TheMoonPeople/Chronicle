#pragma once

#include "common.h"

namespace dc::game {

/**
 * Draws the short muzzle-hit flashes produced by the machine gun.
 */
class CHIT_MACHINGUN_EFFECT {
public:
    float position[16][4]; /**< World positions of the active flashes. */
    s32   timer[16];       /**< Remaining frame counts of the flashes. */

    /**
     * Starts one flash at the specified world position.
     */
    void Set(float *position);

    /**
     * Draws each active machine-gun hit flash.
     */
    void Draw();

    /**
     * Advances the lifetime of each active flash.
     */
    void Step();
};

STATIC_ASSERT(sizeof(CHIT_MACHINGUN_EFFECT) == 0x140);

} // namespace dc::game
