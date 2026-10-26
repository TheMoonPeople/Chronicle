#pragma once

#include "common.h"

#include <libvu0.h>

/**
 *          Animates and draws the nine-letter title logo.
 */
class CLogo {
public:
    int           alpha[9];     /**< Opacity of each letter. */
    int           light[9];     /**< Light level each letter is drawn at. */
    int           count;        /**< Ticks the fade has run for. */
    int           unused_count; /**< Cleared alongside count and never read. */
    sceVu0FVECTOR position;     /**< Where the letters hang in the world. */

    /**
     *          Constructs the logo in its initial animation state.
     *
     * @mangled __ct__5CLogoFv
     * @address 0x1DD3E00
     * @size 0x3C
     */
    CLogo();

    /**
     *          Resets the logo letters and placement.
     *
     * @mangled Init__5CLogoFv
     * @address 0x1DD3E40
     * @size 0xC0
     * @unknownret
     */
    void Init();

    /**
     *          Reveals the logo letters in sequence.
     *
     * @mangled Fade__5CLogoFv
     * @address 0x1DD3F00
     * @size 0x80
     * @unknownret
     */
    void Fade();

    /**
     *          Fades all logo letters out.
     *
     * @mangled FadeOut__5CLogoFv
     * @address 0x1DD3F80
     * @size 0x40
     * @unknownret
     */
    void FadeOut();

    /**
     *          Draws the moving sparkle highlight.
     *
     * @mangled Sparkdraw__5CLogoFf
     * @address 0x1DD3FC0
     * @size 0x2C8
     * @unknownret
     */
    void Sparkdraw(float time);

    /**
     *          Draws the animated logo letters.
     *
     * @mangled Draw__5CLogoFv
     * @address 0x1DD4290
     * @size 0x134
     * @unknownret
     */
    void Draw();

    /**
     *          Advances the logo animation clock.
     *
     * @mangled Move__5CLogoFv
     * @address 0x1DD43D0
     * @size 0x48
     * @unknownret
     */
    void Move();
};
