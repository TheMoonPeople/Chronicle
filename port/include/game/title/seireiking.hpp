#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

class CCamera;

/**
 *          Draws the spirit king's spark trail during the title movie.
 */
class CSeireiKing {
public:
    sceVu0FVECTOR position[20]; /**< Positions occupied by the trail and carried spirit. */
    int           count[20];    /**< Ages of the trailing spark positions. */
    float         alpha[20];    /**< Opacity of each trail or spirit position. */

    /**
     *          Stages the trail, the same way a caller may stage it again later.
     */
    CSeireiKing() { Initialize(); }

    /**
     *          Draws the trailing sparks around a supplied world position.
     */
    void Draw(float *origin, float size, int spread);

    /**
     *          Draws the spirit carried at the head of the effect.
     */
    void Draw2(CCamera *camera, float *head, float *source);

    /**
     *          Advances each stored spark position through the trail.
     */
    void Step();

    /**
     *          Stages the trail so that each spark starts a step behind the one
     *          in front of it and the head is the only one drawn solid.
     */
    void Initialize() {
        for (int i = 0; i < 20; i++) {
            count[i] = -i;
            alpha[i] = 128.0f - (static_cast<float>(i) * 6.4f);
        }

        alpha[19] = 128.0f;
    }
};

STATIC_ASSERT(sizeof(CSeireiKing) == 0x1E0);

} // namespace dc::game
