#pragma once

#include "common.h"

namespace dc::game {

class CMapObject;

/**
 * Updates one actor's blink and mouth frames and copies them over the face it draws with.
 */
void FaceChange(int actor_no);

/**
 * Steps the load of one village-scene character into the given character buffer.
 */
void LoadCharaData(int buffer_no, int actor_no);

/**
 * Returns the motion buffer of one dancer (0 or 1), sized for that dancer's largest motion file,
 * or null for any other index.
 */
u_int *OpeningDancePack(int dancer);

/**
 * Swaps in the dancing couple's next motion file and starts reading the one after it.
 */
void LoadMotionData();

/**
 * Waits for the previous background read, then starts reading the village's pack.
 */
void OpB_LoadDataBG();

/**
 * Waits for the previous background read, then starts reading Toan's house image pack.
 */
void OpB_LoadDataBG2();

/**
 * Sets up the first half of the village scene from the pack the background read left in memory.
 */
void OpB_InitProcess();

/**
 * Sets up the second half of the village scene, which starts when the camera cuts to Toan's house.
 */
void OpB_InitProcess2();

/**
 * Advances the village scene's actors and the models the camera's frame tree carries.
 */
void OpB_MotionProcess();

/**
 * Plays the village scene's footfalls and ambient sounds and sets its music volumes.
 */
void OpB_SoundProcess();

/**
 * Draws one frame of the village scene.
 */
void OpB_DrawProcess();

/**
 * Map objects of Norune village.
 */
extern CMapObject OP_NornMapObj[76];

/**
 * Second set of map objects of Norune village.
 */
extern CMapObject OP_NornMapObj2[87];

} // namespace dc::game
