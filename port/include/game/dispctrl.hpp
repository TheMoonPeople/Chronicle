#pragma once

#include "common.h"

struct sceVif1Packet;

namespace dc::game {

/**
 * Opens a direct-mode GIF tag in the supplied VIF packet.
 */
void openGiftag(sceVif1Packet *packet);

/**
 * Finds a record by file name in a loaded PTS archive, ignoring any directory part of the name.
 */
u_int *SearchPTS(u_int *archive, char *name);

/**
 * Finds a record by index in a loaded PTS archive.
 */
u_int *SearchPTS(u_int *archive, int index);

/**
 * Which way a CDispCtrl curtain is moving, as CDispCtrl::mode holds it.
 */
enum class DispFadeMode : s32 {
    Fixed = -1,  /**< Holds the current alpha. */
    None = 0,    /**< Not yet started. */
    FadeOut = 1, /**< Alpha rises towards black. */
    FadeIn = 2,  /**< Alpha falls towards clear. */
};

/**
 * Controls the black screen curtain used for dungeon transitions.
 */
class CDispCtrl {
public:
    float        rate;  /**< Current curtain alpha on the GS zero-to-128 scale. */
    float        speed; /**< Amount that the curtain alpha changes each step. */
    DispFadeMode mode;  /**< Selects fading out, fading in, or a fixed alpha. */

    /**
     * Starts increasing the curtain alpha at the requested speed.
     */
    void FadeOutStart(float fade_speed);

    /**
     * Starts decreasing the curtain alpha at the requested speed.
     */
    void FadeInStart(float fade_speed);

    /**
     * Sets a fixed curtain alpha without selecting either fade direction.
     */
    void FadeInit(float start_rate);

    /**
     * Returns the current curtain alpha.
     */
    float GetRate();

    /**
     * Advances and draws a curtain that is fading to black.
     */
    float FadeOut(sceVif1Packet *packet);

    /**
     * Advances and draws a curtain that is fading from black.
     */
    float FadeIn(sceVif1Packet *packet);
};

STATIC_ASSERT(sizeof(CDispCtrl) == 0xC);

} // namespace dc::game
