#pragma once

class CCameraFollow;
class CEditGround;
class CDungeonMap;

/**
 * Opens a town zoom scope for one walk update; collision correction still owns the eye.
 */
void TownZoomBegin(CCameraFollow *camera, CEditGround *ground, int map);

/**
 * Applies pending mouse-wheel zoom to the town camera inside the open scope.
 */
void TownZoomRead(CCameraFollow *camera);

/**
 * Closes the town zoom scope and restores the retail near and far camera distances.
 */
void TownZoomEnd();

/**
 * Applies collision-validated mouse zoom to the dungeon gameplay camera, never to scripted cameras.
 */
void DungeonZoomApply(CCameraFollow *camera, CDungeonMap *map, bool lock_on, float previous_distance);

/**
 * Returns the dungeon eye-lift distance, capped by the requested zoom.
 */
float DungeonZoomNearDistance(CCameraFollow *camera, float normal, bool lock_on);
