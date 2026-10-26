#pragma once

#include "common.h"

/**
 * Identifies what stopped a moving projectile during a collision query.
 */
enum SHOT_COLLISION_RESULT {
    SHOT_COLLISION_NONE = 0,
    SHOT_COLLISION_MAP = 1,
    SHOT_COLLISION_PLAYER = 2,
    SHOT_COLLISION_MONSTER = 3,
};

/**
 * Checks projectile movement against the map, player, and active monsters.
 *
 * @mangled checkCollision__FPfPfPfif
 * @address 0x1AB740
 * @size 0x3B4
 */
SHOT_COLLISION_RESULT checkCollision(float *hit_position, float *position, float *movement, s32 target_mode, float radius);

/**
 * Draws one cell of a model as a camera-facing sprite.
 *
 * @mangled set3DCellModel__FPfPcfiiiiUc
 * @address 0x1ABB00
 * @size 0x134
 */
void set3DCellModel(float *world, char *texture_name, float size, s32 x, s32 y, s32 width, s32 height, u8 blend);
