#pragma once

/**
 * @file
 * Fade, wait and blink counters the title screen loop shares with the rest of the title overlay.
 */

#include "common.h"

namespace dc::game {

/**
 * First fade counter of the title sequence.
 */
extern int Fade1;

/**
 * Second fade counter of the title sequence.
 */
extern int Fade2;

/**
 * Third fade counter of the title sequence.
 */
extern int Fade3;

/**
 * Fourth fade counter of the title sequence.
 */
extern int Fade4;

/**
 * Frames the title waits before it goes on.
 */
extern int Wait;

/**
 * Frames the title has counted since the opening.
 */
extern int opcnt;

/**
 * Frames until the pad is read again.
 */
extern int keywait;

/**
 * Whether the title text is blinking.
 */
extern u_char brink;

/**
 * Frames the blink has run for.
 */
extern int brinkcnt;

/**
 * Frames the title effect has run for.
 */
extern int EffCnt;

} // namespace dc::game
