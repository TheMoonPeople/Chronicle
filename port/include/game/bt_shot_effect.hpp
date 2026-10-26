#pragma once

#include "common.h"

/**
 * What a shot effect does when it ends, as BT_SHOT_EFFECT::end_effect holds it.
 */
// clang-format off
enum ShotEndEffect {
    SHOT_END_EFFECT_BOMB = 100, /**< Explodes. */
};

// clang-format on

/**
 * Describes one projectile effect and the four phases it passes through.
 */
struct BT_SHOT_EFFECT {
    char  model_name[16]; /**< Model file the effect draws with. */
    s32   stationary;     /**< Nonzero when the effect does not fly toward its target. */
    s32   face_flight;    /**< Whether a free effect turns its model to face its flight direction. */
    float speed[4];       /**< Distance the effect moves each step in each phase. */
    float radius[4];      /**< Radius of the effect's hit in each phase. */
    s32   life_time;      /**< Steps the effect lasts for. */
    s32   damage;         /**< Damage each hit of the effect deals. */
    s32   hit_flags;      /**< Flags recorded with each hit; SetAttribute replaces them. */
    s32   hit_kind;       /**< Kind of hit recorded; 3 throws the target along the flight. */
    s32   target;         /**< Who the effect can hit, passed to the collision test and to its explosion. */
    s16   motion[4];      /**< Motion the model plays in each phase, or -1. */
    s16   end_effect;     /**< Effect played when the shot ends. @see ShotEndEffect. */
    u8    unk_056[2];
    float bomb_scale;  /**< Scale of the explosion the shot ends in. */
    s32   bomb_damage; /**< Damage of the explosion the shot ends in. */
    s32   sound[4];    /**< Sound effect each phase plays, or -1. */
};

STATIC_ASSERT(sizeof(BT_SHOT_EFFECT) == 0x70);
