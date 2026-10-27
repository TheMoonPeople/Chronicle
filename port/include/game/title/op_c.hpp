#pragma once

#include "common.h"

namespace dc::game {

class CWater;

/**
 * Copies the actor's current eye and mouth frames onto its face plate and steps its talking mouth.
 */
void FaceChangeC(int actor_no);

/**
 * Waits for the previous background read, then starts reading Norune at night's image pack.
 */
void OpC_LoadDataBG();

/**
 * Waits for the previous background read, then starts reading the dance's pack.
 */
void OpC_LoadDataBG2();

/**
 * Waits for the previous background read, then starts reading the demon's pack.
 */
void OpC_LoadDataBG3();

/**
 * Waits for the previous background read, then starts reading Norune at night's image pack again.
 */
void OpC_LoadDataBG4();

/**
 * Waits for the previous background read, then starts reading the burning village's pack.
 */
void OpC_LoadDataBG5();

/**
 * Sets up Norune at night: its scenery, its images and the scene's shared state.
 */
void OpC_InitProcess();

/**
 * Loads the dancers: the shared shadow model and the villagers who join the dance.
 */
void OpC_InitProcess2();

/**
 * Sets up the demon's arrival: the demon, its beam, and the sky and moonlight behind it.
 */
void OpC_InitProcess3();

/**
 * Loads the images of the village after the demon, with the explosion and its dust.
 */
void OpC_InitProcess4();

/**
 * Sets up the burning village: the ruined map, the smoke pool and the actors who walk through it.
 */
void OpC_InitProcess5();

/**
 * Advances every displayed actor's motion and places it from the camera's model for this tick.
 */
void OpC_MotionProcess();

/**
 * Plays this tick's scheduled sounds and the actors' footfalls.
 */
void OpC_SoundProcess();

/**
 * Draws the scene for this tick: world, actors, shadows, water, fires, beam and explosions.
 */
void OpC_DrawProcess();

/**
 * Water surface of the opening.
 */
extern CWater Water;

} // namespace dc::game
