#pragma once

#include "common.h"

struct sceVif1Packet;

/**
 * Opens a direct-mode GIF tag in the supplied VIF packet.
 *
 * @mangled openGiftag__FP13sceVif1Packet
 * @address 0x13DEA0
 * @size 0x58
 */
void openGiftag(sceVif1Packet *packet);

/**
 * Finds a named record in a loaded PTS archive.
 *
 * @mangled SearchPTS__FPUiPc
 * @address 0x13E4E0
 * @size 0xD8
 */
u_int *SearchPTS(u_int *archive, char *name);

/**
 * Finds a record by index in a loaded PTS archive.
 *
 * @mangled SearchPTS__FPUii
 * @address 0x13E5C0
 * @size 0x50
 */
u_int *SearchPTS(u_int *archive, int index);

/**
 * Controls the black screen curtain used for dungeon transitions.
 */
class CDispCtrl {
public:
    float rate;  /**< Current curtain alpha on the GS zero-to-128 scale. */
    float speed; /**< Amount that the curtain alpha changes each step. */
    s32   mode;  /**< Selects fading out, fading in, or a fixed alpha. */

    /**
     * Starts increasing the curtain alpha at the requested speed.
     *
     * @mangled FadeOutStart__9CDispCtrlFf
     * @address 0x13E610
     * @size 0x18
     */
    void FadeOutStart(float fade_speed);

    /**
     * Starts decreasing the curtain alpha at the requested speed.
     *
     * @mangled FadeInStart__9CDispCtrlFf
     * @address 0x13E630
     * @size 0x1C
     */
    void FadeInStart(float fade_speed);

    /**
     * Sets a fixed curtain alpha without selecting either fade direction.
     *
     * @mangled FadeInit__9CDispCtrlFf
     * @address 0x13E650
     * @size 0x14
     */
    void FadeInit(float start_rate);

    /**
     * Returns the current curtain alpha.
     *
     * @mangled GetRate__9CDispCtrlFv
     * @address 0x13E670
     * @size 0xC
     */
    float GetRate();

    /**
     * Advances and draws a curtain that is fading to black.
     *
     * @mangled FadeOut__9CDispCtrlFP13sceVif1Packet
     * @address 0x13E680
     * @size 0x168
     */
    float FadeOut(sceVif1Packet *packet);

    /**
     * Advances and draws a curtain that is fading from black.
     *
     * @mangled FadeIn__9CDispCtrlFP13sceVif1Packet
     * @address 0x13E7F0
     * @size 0x160
     */
    float FadeIn(sceVif1Packet *packet);
};

STATIC_ASSERT(sizeof(CDispCtrl) == 0xC);
