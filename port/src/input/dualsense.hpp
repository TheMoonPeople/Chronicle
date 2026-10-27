#pragma once

#include <cstdint>

namespace dc::input {

/**
 * An RGB colour for a pad's lightbar.
 */
struct LightbarColour {
    std::uint8_t red = 0;   /**< Red intensity. */
    std::uint8_t green = 0; /**< Green intensity. */
    std::uint8_t blue = 0;  /**< Blue intensity. */

    bool operator==(const LightbarColour &) const = default;
};

/**
 * The lightbar colour for a party member's life: green when whole, through yellow to red when low.
 */
LightbarColour LightbarForLife(int life, int max_life);

/**
 * The colour a pad rests at where there is no party member's life to show, such as the title or towns.
 */
LightbarColour LightbarIdle();

/**
 * Sets pad 0's lightbar to the active character's life in a dungeon; called once a tick after the pad read.
 */
void DualSenseUpdate();

} // namespace dc::input
