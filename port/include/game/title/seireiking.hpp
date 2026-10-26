#pragma once

#include "common.h"

#include <libvu0.h>

class CCamera;

/**
 *          Draws the spirit king's spark trail during the title movie.
 */
class CSeireiKing {
public:
    sceVu0FVECTOR position[20]; /**< Positions occupied by the trail and carried spirit. */
    int           count[20];    /**< Ages of the trailing spark positions. */
    float         alpha[20];    /**< Opacity of each trail or spirit position. */

    /** Stages the trail, the same way a caller may stage it again later. */
    CSeireiKing() { Initialize(); }

    /**
     *          Draws the trailing sparks around a supplied world position.
     *
     * @mangled Draw__11CSeireiKingFPffi
     * @address 0x1DAE590
     * @size 0x814
     */
    void Draw(float *origin, float size, int spread);

    /**
     *          Draws the spirit carried at the head of the effect.
     *
     * @mangled Draw2__11CSeireiKingFP7CCameraPfPf
     * @address 0x1DAEDB0
     * @size 0x374
     */
    void Draw2(CCamera *camera, float *head, float *source);

    /**
     *          Advances each stored spark position through the trail.
     *
     * @mangled Step__11CSeireiKingFv
     * @address 0x1DAF130
     * @size 0x84
     */
    void Step();

    /**
     *          Stages the trail so that each spark starts a step behind the one
     *          in front of it and the head is the only one drawn solid.
     */
    void Initialize() {
        for (int i = 0; i < 20; i++) {
            count[i] = -i;
            alpha[i] = 128.0f - i * 6.4f;
        }

        alpha[19] = 128.0f;
    }
};

STATIC_ASSERT(sizeof(CSeireiKing) == 0x1E0);
