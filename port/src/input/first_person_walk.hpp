#pragma once

namespace dc::game {
class CDungeonMap;
struct DRAN_MAP_FIELD_SET;
struct MoveCheckInfo;
struct BT_ACT_STATUS;
} // namespace dc::game

namespace dc::input {

/**
 * Moves the first-person character through the dungeon with character (not camera) collision.
 * Returns false without moving when the collision geometry is invalid.
 */
bool FirstPersonDungeonStep(game::CDungeonMap &map, game::DRAN_MAP_FIELD_SET *fields, float *pos, float *velocity, game::MoveCheckInfo &info, int mode);

/**
 * Writes the keyboard-driven horizontal velocity for the given heading into velocity[0] and velocity[2].
 * Returns whether movement is currently allowed; the velocity is zeroed when it is not.
 */
bool FirstPersonDungeonKeyboardVelocity(float *velocity, float heading, const game::BT_ACT_STATUS &status, bool held, bool alive, bool slow);

} // namespace dc::input
