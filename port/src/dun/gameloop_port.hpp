#pragma once

#include <libvu0.h>

// State shared by the dungeon movement and game-loop replacements, defined in the dungeon
// translation unit.
extern sceVu0FVECTOR blowVelo;
extern int           ruby_effect_id;
extern float         stickVector;
extern sceVu0FVECTOR velo__2;
extern float         viewAngleH__2;
extern float         viewAngleV__2;

/**
 * Writes into velocity the velocity collision hit hit_no pushes the player with; a blowing hit
 * (ready phase 10) pushes straight away from its position.
 */
void HitBlowVelocity(int hit_no, float *velocity);
