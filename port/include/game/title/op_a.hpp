#pragma once

#include "common.h"

#include <libvu0.h>

class OBJ_ANIME_SEQ;
class CMap;
class CFrame;
class CFrameVu1;
struct MOTION_INFO;

/**
 *          Initializes the dungeon-square opening scene.
 *
 * @mangled OpA_InitProcess__Fv
 * @address 0x1DB4EC0
 * @size 0x1A8
 * @unknownret
 */
void OpA_InitProcess();

/**
 *          Draws one frame of the dungeon-square scene.
 *
 * @mangled OpA_DrawProcess__Fv
 * @address 0x1DB59D0
 * @size 0xD34
 * @unknownret
 */
void OpA_DrawProcess();

/**
 *          Advances the dungeon-square scene state.
 *
 * @mangled OpA_MotionProcess__Fv
 * @address 0x1DB6BF0
 * @size 0x9E0
 * @unknownret
 */
void OpA_MotionProcess();

/**
 *          Updates sound and music for the dungeon-square scene.
 *
 * @mangled OpA_SoundProcess__Fv
 * @address 0x1DB7B90
 * @size 0x740
 * @unknownret
 */
void OpA_SoundProcess();

/**
 * Frames the dance has run for.
 */
extern int DanceCnt;

/**
 * Whether the dance has started.
 */
extern int DanceStart;

/**
 * Object animation sequences of the opening.
 */
extern OBJ_ANIME_SEQ OP_AnimeSeq[];

/**
 * Rotation step of the object animations.
 */
extern int OP_AnimeSeqRot;

/**
 * Map of the village buildings.
 */
extern CMap OP_BuildingMap;

/**
 * Second map of the village buildings.
 */
extern CMap OP_BuildingMap2;

/**
 * Frame of the character the opening follows.
 */
extern CFrame *OP_CharaFrame__2;

/**
 * Whether each fire is lit.
 */
extern int OP_FireFlg[96];

/**
 * Number of fires placed.
 */
extern int OP_FireList;

/**
 * Position of each fire.
 */
extern sceVu0FVECTOR OP_FirePosition[96];

/**
 * Scale of each fire.
 */
extern float OP_FireScale[96];

/**
 * Collision frame of the ground.
 */
extern CFrameVu1 *OP_GroundCol;

/**
 * Map of the ground.
 */
extern CMap OP_GroundMap;

/**
 * Frame of the sky dome.
 */
extern CFrameVu1 *OP_SkyFrame;

/**
 * Motion keys of the dancer.
 */
extern MOTION_INFO dancer[];

/**
 * Motion keys of the cursed villager.
 */
extern MOTION_INFO noroi[];
