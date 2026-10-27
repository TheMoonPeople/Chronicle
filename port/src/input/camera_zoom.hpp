#pragma once

namespace dc::game {
class CCameraFollow;
class CEditGround;
class CDungeonMap;
} // namespace dc::game

namespace dc::input {

/**
 * Opens a town zoom scope for one walk update; collision correction still owns the eye.
 */
void TownZoomBegin(game::CCameraFollow *camera, game::CEditGround *ground, int map);

/**
 * Applies pending mouse-wheel zoom to the town camera inside the open scope.
 */
void TownZoomRead(game::CCameraFollow *camera);

/**
 * Closes the town zoom scope and restores the retail near and far camera distances.
 */
void TownZoomEnd();

/**
 * Applies collision-validated mouse zoom to the dungeon gameplay camera, never to scripted cameras.
 */
void DungeonZoomApply(game::CCameraFollow *camera, game::CDungeonMap *map, bool lock_on, float previous_distance);

/**
 * Returns the dungeon eye-lift distance, capped by the requested zoom.
 */
float DungeonZoomNearDistance(game::CCameraFollow *camera, float normal, bool lock_on);

} // namespace dc::input
