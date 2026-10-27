#pragma once

#include "common.h"

namespace dc::game {

/**
 * Copies the actor's current eye and mouth rows from its strip texture onto its face plate.
 */
void FaceChangeD(int actor_no);

/**
 * Waits for the previous background read and starts loading the spirit king's hall.
 */
void OpD_LoadDataBG();

/**
 * Waits for the previous background read and starts loading the meadow and its music bank.
 */
void OpD_LoadDataBG2();

/**
 * Sets up the spirit king's hall, the first half of the scene.
 */
void OpD_InitProcess();

/**
 * Sets up the meadow the girl walks out into, the second half of the scene.
 */
void OpD_InitProcess2();

/**
 * Applies the script's motions to the actors, places them and steps the wind and cloth.
 */
void OpD_MotionProcess();

/**
 * Plays the scene's camera-cued sound effects and music.
 */
void OpD_SoundProcess();

/**
 * Draws the scene, its effects, the screen fades and the caption.
 */
void OpD_DrawProcess();

} // namespace dc::game
