#pragma once

#include "common.h"

struct sceVif1Packet;

namespace dc::game {

/**
 * Direction a CDispFade is currently moving in.
 */
enum class TitleFadeMode : s32 {
    Idle = -1, /**< Not fading. */
    Out = 1,   /**< Increasing the cover towards opaque. */
    In = 2,    /**< Decreasing the cover towards clear. */
};

/**
 * Controls the title overlay's black or white full-screen fade.
 */
class CDispFade {
public:
    float         fade_rate;   /**< Current GS alpha on the zero-to-128 scale. */
    float         fade_speed;  /**< Amount by which the alpha changes each frame. */
    TitleFadeMode mode;        /**< Active direction. */
    s32           fade_colour; /**< Zero selects black and any nonzero value selects white. */

    /**
     * Starts increasing the screen-cover alpha.
     */
    void FadeOutStart(float speed, int colour);

    /**
     * Starts decreasing the screen-cover alpha.
     */
    void FadeInStart(float speed, int colour);

    /**
     * Resets the fade to an idle state at a chosen alpha.
     */
    void FadeInit(float initial_rate);

    /**
     * Returns the current screen-cover alpha.
     */
    float GetRate();

    /**
     * Advances and draws a fade towards an opaque screen.
     */
    float FadeOut(sceVif1Packet *packet);

    /**
     * Advances and draws a fade towards a clear screen.
     */
    float FadeIn(sceVif1Packet *packet);
};

STATIC_ASSERT(sizeof(CDispFade) == 0x10);

} // namespace dc::game
