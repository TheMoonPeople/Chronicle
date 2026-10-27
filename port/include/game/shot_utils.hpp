#pragma once

#include "common.h"

namespace dc::game {

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
 */
SHOT_COLLISION_RESULT checkCollision(float *hit_position, float *position, float *movement, s32 target_mode, float radius);

/**
 * Draws one cell of a model as a camera-facing sprite.
 */
void set3DCellModel(float *world, char *texture_name, float size, s32 x, s32 y, s32 width, s32 height, u8 blend);

} // namespace dc::game
