#pragma once

#include "common.h"

#include <array>

namespace dc::game {

/**
 * The spark that streaks across the title screen, trailing a tail of fading points.
 */
class CSprite {
public:
    std::array<float, 12> x;       /**< Horizontal position of each trail point, newest first. */
    std::array<float, 12> y;       /**< Vertical position of each trail point, newest first. */
    float                 angle;   /**< Heading the head of the trail is turning towards. */
    int                   started; /**< Whether the trail has been let go. */

    /**
     * Parks the trail off the left of the screen, not yet started.
     */
    CSprite();

    /**
     * Resets the trail to its constructed state.
     */
    void Init();

    /**
     * Shifts the trail along and steers its head towards the next waypoint.
     */
    void Move();

    /**
     * Draws the head, its sparks and the fading tail.
     */
    void Draw();

    /**
     * Returns nonzero once Move has been called since the last reset.
     */
    int Se();
};

} // namespace dc::game
