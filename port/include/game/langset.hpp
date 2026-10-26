#pragma once

#include "common.h"

/**
 * @file
 * Declares the language-selection screen the game shows before the title.
 */

/**
 * Steps of the language screen, as Proc holds them.
 */
// clang-format off
enum LangsetStep {
    LANGSET_FADE_IN  = 0, /**< Fading in. */
    LANGSET_SELECT   = 1, /**< Choosing a language. */
    LANGSET_FADE_OUT = 2, /**< Fading out. */
};

// clang-format on

class Fader;

/**
 * Fade the language screen opens and closes with.
 */
extern Fader Fade;

/**
 * Language the cursor stands on, counted from zero.
 */
extern int Cursor;

/**
 * What the screen is doing. @see LangsetStep.
 */
extern int Proc;

/**
 * Builds the arenas, textures and pad settings the language screen runs on.
 *
 * @mangled LangsetInit__Fv
 * @address 0x244B30
 * @size 0x140
 */
void LangsetInit();

/**
 * Runs one frame of the language screen and reports that a language has been
 * chosen and faded out.
 *
 * @mangled LangsetLoop__Fv
 * @address 0x244C70
 * @size 0xE4
 */
int LangsetLoop();

/**
 * Moves the language cursor with the pad and reports that the choice was
 * confirmed.
 *
 * @mangled LangsetProc__Fv
 * @address 0x244D60
 * @size 0xD8
 */
int LangsetProc();

/**
 * Draws the language plate and its five entries, brightening the selected one.
 *
 * @mangled LangsetDraw__Fv
 * @address 0x244E40
 * @size 0x3A8
 */
void LangsetDraw();
