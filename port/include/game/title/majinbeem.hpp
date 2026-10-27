#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

/**
 * Steps of the demon's beam, as CMajinBeem::state holds them.
 */
// clang-format off
enum MajinBeemState {
    MAJIN_BEEM_AIM    = 0, /**< Aiming at the target. */
    MAJIN_BEEM_TRAVEL = 1, /**< Travelling. */
};

// clang-format on

class CCamera;

/**
 * The demon's beam in the opening: a head sprite followed by a trail of the places it has been.
 */
class CMajinBeem {
public:
    sceVu0FVECTOR positions[60]; /**< Trail element positions, the head first. */
    sceVu0FVECTOR target;        /**< Point the beam is aimed at on its first step. */
    int           state;         /**< What the beam is doing. @see MajinBeemState. */
    int           counters[60];  /**< Ages of the recorded trail elements. */
    float         sizes[60];     /**< Draw sizes of the trail elements. */
    float         alphas[60];    /**< Alpha values of the trail elements. */
    int           active;        /**< Whether the beam is active. */
    float         pitch;         /**< Vertical travel angle. */
    float         yaw;           /**< Horizontal travel angle. */
    float         speed;         /**< Distance travelled during each update. */
    u8            unk_6b4[0xc];

    /**
     * Initializes the beam before it is fired.
     */
    CMajinBeem();

    /**
     * Drops the beam, so that nothing draws until it is fired again.
     */
    void Initialize();

    /**
     * Draws the beam trail as camera-facing sprites.
     */
    void Draw(CCamera *camera);

    /**
     * Draws and contracts the beam's leading sprite at `head`, depth-biased towards the camera from `source`.
     */
    void Draw2(CCamera *camera, float *head, float *source);

    /**
     * Advances the beam and records its trail positions.
     */
    void Step();
};

STATIC_ASSERT(sizeof(CMajinBeem) == 0x6c0);

} // namespace dc::game
