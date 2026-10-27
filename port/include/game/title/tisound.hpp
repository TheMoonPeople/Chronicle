#pragma once

/**
 * @file
 * Sound-effect playback for the title overlay.
 */

#include "common.h"

namespace dc::game {

/**
 * Plays a sound effect at its table volume scaled by @p volume.
 * @param group MIDI port the effect plays on.
 * @param no Sound-effect bank number.
 * @param voice Voice within the bank.
 * @param volume Scale applied to the effect's base volume; the result is clamped to 0-127.
 */
void TiPlayVolSE(int group, int no, int voice, float volume);

} // namespace dc::game
