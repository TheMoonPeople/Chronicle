#pragma once

#include "common.h"

/**
 * Tracks a linearly changing 0-to-128 fade level.
 */
class Fader {
public:
    int value; /**< Current fade level. */

    /**
     * Advances the fade level toward full intensity in steps of four.
     *
     * @mangled In__5FaderFv
     * @address 0x2451F0
     * @size 0x48
     */
    int In();

    /**
     * Advances the fade level toward zero in steps of four.
     *
     * @mangled Out__5FaderFv
     * @address 0x245240
     * @size 0x2C
     */
    int Out();

    /**
     * Scales a requested maximum by the current fade fraction.
     *
     * @mangled Get__5FaderFi
     * @address 0x245270
     * @size 0x48
     */
    int Get(int maximum);
};

STATIC_ASSERT(sizeof(Fader) == 0x4);
