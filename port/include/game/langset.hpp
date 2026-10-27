#pragma once

#include "common.h"

namespace dc::game {

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
 */
void LangsetInit();

/**
 * Runs one frame of the language screen and reports that a language has been
 * chosen and faded out.
 */
int LangsetLoop();

/**
 * Moves the language cursor with the pad and reports that the choice was
 * confirmed.
 */
int LangsetProc();

/**
 * Draws the language plate and its five entries, brightening the selected one.
 */
void LangsetDraw();

} // namespace dc::game
