#pragma once

#include "common.h"

class CMapObject;

/**
 *          Updates and transfers one actor's eye and mouth textures.
 *
 * @mangled FaceChange__Fi
 * @address 0x1DB87D0
 * @size 0x72C
 */
void FaceChange(int actor_no);

/**
 *          Loads one village-scene character into the requested slot.
 *
 * @mangled LoadCharaData__Fii
 * @address 0x1DB8F00
 * @size 0x1D4
 * @unknownret
 */
void LoadCharaData(int buffer_no, int actor_no);

/**
 *          Streams the next motion segment for the dancing couple.
 *
 * @mangled LoadMotionData__Fv
 * @address 0x1DB90E0
 * @size 0x2E4
 * @unknownret
 */
void LoadMotionData();

/**
 *          Loads the first village background phase.
 *
 * @mangled OpB_LoadDataBG__Fv
 * @address 0x1DB93D0
 * @size 0x4C
 * @unknownret
 */
void OpB_LoadDataBG();

/**
 *          Loads the second village background phase.
 *
 * @mangled OpB_LoadDataBG2__Fv
 * @address 0x1DB9420
 * @size 0x4C
 * @unknownret
 */
void OpB_LoadDataBG2();

/**
 *          Initializes the first phase of the village scene.
 *
 * @mangled OpB_InitProcess__Fv
 * @address 0x1DB9470
 * @size 0xD4C
 * @unknownret
 */
void OpB_InitProcess();

/**
 *          Initializes the Toan-house phase of the village scene.
 *
 * @mangled OpB_InitProcess2__Fv
 * @address 0x1DBA1C0
 * @size 0x6F4
 * @unknownret
 */
void OpB_InitProcess2();

/**
 *          Advances the village scene's scripted motion state.
 *
 * @mangled OpB_MotionProcess__Fv
 * @address 0x1DBA8C0
 * @size 0x454
 * @unknownret
 */
void OpB_MotionProcess();

/**
 *          Updates sound and music for the village scene.
 *
 * @mangled OpB_SoundProcess__Fv
 * @address 0x1DBAD20
 * @size 0x4F4
 * @unknownret
 */
void OpB_SoundProcess();

/**
 *          Draws one frame of the village scene.
 *
 * @mangled OpB_DrawProcess__Fv
 * @address 0x1DBB220
 * @size 0x680
 * @unknownret
 */
void OpB_DrawProcess();

/**
 * Map objects of Norune village.
 */
extern CMapObject OP_NornMapObj[];

/**
 * Second set of map objects of Norune village.
 */
extern CMapObject OP_NornMapObj2[];
