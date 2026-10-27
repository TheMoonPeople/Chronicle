#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

/**
 * Phases of one billboard of the bomb effect, as CBombEffect::phases holds them.
 */
// clang-format off
enum BombPhase {
    BOMB_PHASE_BURST     = 0, /**< Growing fast. */
    BOMB_PHASE_BLOOM     = 1, /**< Growing slowly. */
    BOMB_PHASE_LINGER    = 2, /**< Lingering as it fades. */
    BOMB_PHASE_DISSIPATE = 3, /**< Shrinking away. */
};

// clang-format on

class CCamera;

/**
 * Animates the fire and dust billboards of a title-overlay explosion.
 */
class CBombEffect {
public:
    sceVu0FVECTOR positions[8]; /**< World position of each billboard. */
    s32           phases[8];    /**< Envelope phase each billboard is in; also pushes it towards the camera. */
    s32           counters[8];  /**< Frames elapsed in each billboard's current phase. */
    float         sizes[8];     /**< World-space billboard widths and heights. */
    float         alphas[8];    /**< GS alpha values used to draw the billboards. */
    s32           active[8];    /**< One while the corresponding billboard is animated. */

    /**
     * Initializes the fire and dust billboards before animation begins.
     */
    CBombEffect();

    /**
     * Drops every billboard the effect is carrying.
     */
    void Initialize();

    /**
     * Draws the active fire and dust billboards facing the camera.
     */
    void Draw(CCamera *camera, int dust_only);

    /**
     * Advances every active billboard through its size and alpha envelope.
     */
    void Step();
};

STATIC_ASSERT(sizeof(CBombEffect) == 0x120);

} // namespace dc::game
