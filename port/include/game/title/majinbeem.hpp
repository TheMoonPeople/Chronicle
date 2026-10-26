#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Steps of the demon's beam, as CMajinBeem::state holds them.
 */
// clang-format off
enum MajinBeemState {
    MAJIN_BEEM_AIM    = 0, /**< Aiming at the target. */
    MAJIN_BEEM_TRAVEL = 1, /**< Travelling. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCamera;

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
     *
     * @mangled __ct__10CMajinBeemFv
     * @address 0x1434C0
     * @size 0x30
     */
    CMajinBeem();

    /**
     * Drops the beam, so that nothing draws until it is fired again.
     *
     * @mangled Initialize__10CMajinBeemFv
     * @address 0x1434F0
     * @size 0x10
     */
    void Initialize();

    /**
     * Draws the beam trail as camera-facing sprites.
     *
     * @mangled Draw__10CMajinBeemFP7CCamera
     * @address 0x1DADD50
     * @size 0x250
     */
    void Draw(CCamera *camera);

    /**
     * Draws and contracts the beam's leading sprite.
     *
     * @mangled Draw2__10CMajinBeemFP7CCameraPfPf
     * @address 0x1DADFA0
     * @size 0x394
     */
    void Draw2(CCamera *camera, float *head, float *source);

    /**
     * Advances the beam and records its trail positions.
     *
     * @mangled Step__10CMajinBeemFv
     * @address 0x1DAE340
     * @size 0x24C
     */
    void Step();
};

STATIC_ASSERT(sizeof(CMajinBeem) == 0x6c0);
