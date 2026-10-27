#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

/**
 * @file
 * Declares the light that stands in for a point light.
 */

/**
 * Lights the world about one point.
 */
class CFakePointLight {
public:
    sceVu0FVECTOR pos;         /**< World position that the light comes from. */
    float         colour[4];   /**< Colour that the light gives. */
    float         inner_range; /**< Distance up to which the light gives its whole colour. */
    float         outer_range; /**< Distance past which the light gives nothing. */
    s32           used;        /**< 1 while the slot lights something. */
    float         source_w;    /**< W component of the position the light was set from; -1 while the slot is cleared. */

    /**
     * Makes a light that is white, and that reaches from ten units away to
     * seventy.
     */
    CFakePointLight();
};

STATIC_ASSERT(sizeof(CFakePointLight) == 0x30);

} // namespace dc::game
