#pragma once

class CCameraFollow;
class CEditGround;
class CDungeonMap;

// Town distance rules are scoped to this walk update; collision correction still owns the eye.
void TownZoomBegin(CCameraFollow *camera, CEditGround *ground, int map);
void TownZoomRead(CCameraFollow *camera);
void TownZoomEnd();

// Only called after the normal dungeon gameplay camera checks, never by scripted/item cameras.
void  DungeonZoomApply(CCameraFollow *camera, CDungeonMap *map, bool lock_on, float previous_distance);
float DungeonZoomNearDistance(CCameraFollow *camera, float normal, bool lock_on);
