#pragma once

#include <libvu0.h>

// State shared by the dungeon movement and game-loop replacements. The definitions remain in
// the retail dungeon translation unit.
extern sceVu0FVECTOR blowVelo;
extern int           ruby_effect_id;
extern float         stickVector;
extern sceVu0FVECTOR velo__2;
extern float         viewAngleH__2;
extern float         viewAngleV__2;

void HitBlowVelocity(int hit_no, float *velocity);
