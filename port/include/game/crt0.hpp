#pragma once

/**
 * @file
 * What the hand-written ps2/src/crt0.s defines for the rest of the game.
 */

#include "common.h"

/**
 * The first instruction of the main image, where crt0's entry code starts.
 */
extern "C" void func_00100000();
