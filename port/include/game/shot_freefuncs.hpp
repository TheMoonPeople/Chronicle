#pragma once

#include "common.h"

namespace dc::game {

class CTexture;
struct spRGBA;

/**
 * Records the party's position relative to the nearest water surface.
 */
struct CHECK_WATER_INFO {
    float dry_position[4];     /**< Where the party last stood out of the water. */
    float surface_position[4]; /**< Point on the water surface where the party stands. */
    s32   in_water;            /**< Whether the party stood in the water at the last check. */
    char  unk_24[0xC];
};

STATIC_ASSERT(sizeof(CHECK_WATER_INFO) == 0x30);

/**
 * Records one ring spreading across the water.
 */
struct WATER_WAVE_LING {
    float center[4]; /**< Point on the water the ring spreads from. */
    float radius;    /**< Current radius of the ring. */
    s32   life;      /**< Frames left before the ring fades out; 0 while the slot is free. */
    char  unk_18[8];
};

STATIC_ASSERT(sizeof(WATER_WAVE_LING) == 0x20);

/**
 * Debug switches the debug overlay sets and dungeon rendering reads.
 */
extern "C" s32 DebugStatus[21];

/**
 * Tint applied to the party by status ailments.
 */
extern float StatusColor[3];

/**
 * Alpha of the low-life warning pulse, which status bars at or below a fifth draw with.
 */
extern int statusAlarmRate;

/**
 * Clears the water-splash effects.
 */
void WaterSplash_Init();

/**
 * Restores the party while they stand in healing water or a healing zone.
 */
void HealingWater();

/**
 * Draws the rings spreading on the water.
 */
void DrawWaterLing();

/**
 * Advances the rings spreading on the water.
 */
void StepWaterLing();

/**
 * Mixes the battle music against the ambience by the distance to the nearest active monster
 * and returns that distance.
 */
float SetBattleStyle(int map_no, int preserve_bgm);

/**
 * Draws a two- or three-digit value out of the number sheet and returns the digits drawn.
 */
int ValuePrint(int x, int y, int value, int palette, unsigned char alpha);

/**
 * Clears the pulse that warns of low life.
 */
void BtStatusAlarmInit();

/**
 * Advances the pulse that warns of low life.
 */
void BtStatusAlarmAnime();

/**
 * Chooses the colour a status bar draws in from how full it is.
 */
u8 *BtGetStatusPal(int bar, float max, float value);

/**
 * Chooses the second colour a status bar draws in from how full it is.
 */
u8 *BtGetStatusPal2(int bar, float max, float value);

/**
 * Draws the dungeon HUD gauges, quick-use items, floor indicators and weapon icon.
 */
void topStatusInfo(int y, int selected_item, int floor);

/**
 * Reports whether the party is suffering one status ailment.
 */
int StatusErrCheck(int status);

/**
 * Sets StatusColor to the tint of the party's status ailment and returns 1 while one runs.
 */
int BtStatusErrColorSet();

/**
 * Advances the party's status ailments and applies what they cost.
 */
void BtStatusErrStep();

/**
 * Inflicts one status ailment on the party.
 */
void BtSetStatusErr(int status);

/**
 * Draws the icons of the party's status ailments.
 */
void BtStatusErrDraw(int y);

/**
 * Draws one item into the reserved slot area.
 */
void setItemToReserved(char *page_name, int x, int y, char *item_name, int dsax, int dsay);

/**
 * Clears the cached map-jump data.
 */
void BtMapJumpCashClear();

/**
 * Loads the steeb message file for the current floor if it changed, then reads the map a jump
 * leads to, its treasure-box models and item-get motions; returns 0 when that map is already
 * loaded, 1 otherwise.
 */
int BtMapJumpLoad(char *map_name);

/**
 * Draws a textured cell in world space.
 */
void BtSet3DCellModel(float *world, CTexture *texture, float size, int x, int y, int width, int height, int alpha);

/**
 * Gives the way a shot flies for one heading and pitch.
 */
void setShotVector(float *velocity, float speed, float angle_y, float angle_x);

/**
 * Gives the way the player's character faces, at one pitch.
 */
void getCharacterVector(float *vector, float pitch);

/**
 * Gives the way a thrown thing has to leave one point to land on another.
 */
void ParabolicInitialVector(float *velocity, float *from, float *to, float gravity, float time);

} // namespace dc::game
