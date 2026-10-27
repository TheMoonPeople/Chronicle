#pragma once

#include "common.h"

namespace dc::game {

/**
 * Tracks the title screen's signed scene-fade value.
 */
class CScFader {
public:
    int value = 0; /**< The fade itself, between -4 and 128. */

    /**
     * Advances the fade-in by 2, returning nonzero once it has reached 128.
     */
    int In();

    /**
     * Advances the fade-in by 8, returning nonzero once it has reached 128.
     */
    int In2();

    /**
     * Advances the fade-out by 2, returning nonzero once it has reached -4.
     */
    int Out();

    /**
     * Returns the fade clamped to the range zero to limit.
     */
    int Get(int limit);

    /**
     * Forces the fade to its fully faded-in state.
     */
    void Skip();
};

} // namespace dc::game
