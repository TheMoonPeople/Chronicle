#pragma once

#include "common.h"

struct sceVif1Packet;

/**
 * Controls the title overlay's black or white full-screen fade.
 */
class CDispFade {
public:
    float fade_rate;   /**< Current GS alpha on the zero-to-128 scale. */
    float fade_speed;  /**< Amount by which the alpha changes each frame. */
    s32   mode;        /**< Active direction: one fades out, two fades in, and minus one is idle. */
    s32   fade_colour; /**< Zero selects black and any nonzero value selects white. */

    /**
     * Starts increasing the screen-cover alpha.
     *
     * @mangled FadeOutStart__9CDispFadeFfi
     * @address 0x1DAD1E0
     * @size 0x18
     */
    void FadeOutStart(float speed, int colour);

    /**
     * Starts decreasing the screen-cover alpha.
     *
     * @mangled FadeInStart__9CDispFadeFfi
     * @address 0x1DAD200
     * @size 0x18
     */
    void FadeInStart(float speed, int colour);

    /**
     * Resets the fade to an idle state at a chosen alpha.
     *
     * @mangled FadeInit__9CDispFadeFf
     * @address 0x1DAD220
     * @size 0x18
     */
    void FadeInit(float initial_rate);

    /**
     * Returns the current screen-cover alpha.
     *
     * @mangled GetRate__9CDispFadeFv
     * @address 0x1DAD240
     * @size 0xC
     */
    float GetRate();

    /**
     * Advances and draws a fade towards an opaque screen.
     *
     * @mangled FadeOut__9CDispFadeFP13sceVif1Packet
     * @address 0x1DAD250
     * @size 0x1E0
     */
    float FadeOut(sceVif1Packet *packet);

    /**
     * Advances and draws a fade towards a clear screen.
     *
     * @mangled FadeIn__9CDispFadeFP13sceVif1Packet
     * @address 0x1DAD430
     * @size 0x1E0
     */
    float FadeIn(sceVif1Packet *packet);
};

STATIC_ASSERT(sizeof(CDispFade) == 0x10);
