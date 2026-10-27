#pragma once

#include "common.h"

namespace dc::game {

/**
 * Tracks a linearly changing 0-to-128 fade level.
 */
class Fader {
public:
    int value; /**< Current fade level. */

    /**
     * Advances the fade level toward full intensity in steps of four,
     * returning 1 once it had already reached 128.
     */
    int In();

    /**
     * Advances the fade level toward zero in steps of four,
     * returning 1 once it had already reached zero.
     */
    int Out();

    /**
     * Scales a requested maximum by the current fade fraction.
     */
    int Get(int maximum);
};

STATIC_ASSERT(sizeof(Fader) == 0x4);

} // namespace dc::game
