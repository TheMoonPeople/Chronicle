#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * The wind one scene blows over its cloth. It holds a direction and a steady
 * speed, and wanders a gust strength between nothing and that speed so that
 * the cloth it drives never settles into a fixed shape.
 */
class CWind {
public:
    float         gust;     /**< Share of the speed the wind blows at now, from zero to one. */
    sceVu0FVECTOR dir;      /**< Unit vector the wind blows along. */
    float         velocity; /**< Speed the wind blows at when the gust is at full strength. */

    /**
     * Stills the wind and starts the gust strength somewhere in its range, so
     * that two scenes built in the same frame do not gust together.
     *
     * @mangled __ct__5CWindFv
     * @address 0x13B5E0
     * @size 0x54
     */
    CWind();

    /**
     * Points the wind along a direction, which need not already be a unit
     * vector.
     *
     * @mangled SetDir__5CWindFPf
     * @address 0x13B480
     * @size 0x24
     * @unknownret
     */
    void SetDir(float *direction);

    /**
     * Sets the speed the wind blows at when it is gusting at full strength.
     *
     * @mangled SetVelocity__5CWindFf
     * @address 0x13B4B0
     * @size 0xC
     * @unknownret
     */
    void SetVelocity(float speed);

    /**
     * Gives the wind at its steady speed, ignoring the gust.
     *
     * @mangled GetWind__5CWindFPf
     * @address 0x13B4C0
     * @size 0x30
     * @unknownret
     */
    void GetWind(float *out);

    /**
     * Gives the wind as it blows this frame, the steady speed scaled by the
     * gust strength.
     *
     * @mangled GetWindNoise__5CWindFPf
     * @address 0x13B4F0
     * @size 0x38
     * @unknownret
     */
    void GetWindNoise(float *out);

    /**
     * Walks the gust strength one step and holds it inside its range.
     *
     * @mangled Step__5CWindFv
     * @address 0x13B530
     * @size 0xAC
     * @unknownret
     */
    void Step();
};

STATIC_ASSERT(sizeof(CWind) == 0x30);
