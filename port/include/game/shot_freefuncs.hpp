#pragma once

#include "common.h"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
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

/** Debug switches the debug overlay sets and dungeon rendering reads. */
extern "C" s32 DebugStatus[21];

/** The tint applied to the party by status ailments. */
extern "C" float StatusColor[3];

/**
 * Clears the water-splash effects.
 *
 * @mangled WaterSplash_Init__Fv
 * @address 0x1AF360
 * @size 0x48
 */
void WaterSplash_Init();

/**
 * Reports whether the party stands in the water of the nearest map part, records the surface
 * point in CheckWaterInfo and starts the splash as they enter it.
 *
 * @mangled CheckHealingWater__Fv
 * @address 0x1AF3B0
 * @size 0x328
 */
int CheckHealingWater();

/**
 * Reports whether the party stands in a healing zone.
 *
 * @mangled CheckHealZone__Fv
 * @address 0x1AF6E0
 * @size 0x29C
 */
int CheckHealZone();

/**
 * Restores the party while they stand in healing water.
 *
 * @mangled HealingWater__Fv
 * @address 0x1AF980
 * @size 0x158
 */
void HealingWater();

/**
 * Draws the rings spreading on the water.
 *
 * @mangled DrawWaterLing__Fv
 * @address 0x1AFAE0
 * @size 0x27C
 */
void DrawWaterLing();

/**
 * Advances the rings spreading on the water.
 *
 * @mangled StepWaterLing__Fv
 * @address 0x1AFD60
 * @size 0x130
 */
void StepWaterLing();

/**
 * Mixes the battle music against the ambience by the distance to the nearest active monster
 * and returns that distance.
 *
 * @mangled SetBattleStyle__Fii
 * @address 0x1AFE90
 * @size 0x1D0
 */
float SetBattleStyle(int map_no, int preserve_bgm);

/**
 * Draws a three-digit value out of the number sheet.
 *
 * @mangled ValuePrint__FiiiiUc
 * @address 0x1B0060
 * @size 0x1F8
 */
int ValuePrint(int x, int y, int value, int palette, unsigned char alpha);

/**
 * Clears the pulse that warns of low life.
 *
 * @mangled BtStatusAlarmInit__Fv
 * @address 0x1B0260
 * @size 0xB8
 */
void BtStatusAlarmInit();

/**
 * Advances the pulse that warns of low life.
 *
 * @mangled BtStatusAlarmAnime__Fv
 * @address 0x1B0320
 * @size 0xC8
 */
void BtStatusAlarmAnime();

/**
 * Chooses the colour a status bar draws in from how full it is.
 *
 * @mangled BtGetStatusPal__Fiff
 * @address 0x1B03F0
 * @size 0x80
 */
u8 *BtGetStatusPal(int bar, float max, float value);

/**
 * Chooses the second colour a status bar draws in from how full it is.
 *
 * @mangled BtGetStatusPal2__Fiff
 * @address 0x1B0470
 * @size 0x80
 */
u8 *BtGetStatusPal2(int bar, float max, float value);

/**
 * Draws the dungeon HUD gauges, quick-use items, floor indicators and weapon icon.
 *
 * @mangled topStatusInfo__Fiii
 * @address 0x1B04F0
 * @size 0x1438
 */
void topStatusInfo(int y, int selected_item, int floor);

/**
 * Reports whether the party is suffering one status ailment.
 *
 * @mangled StatusErrCheck__Fi
 * @address 0x1B1930
 * @size 0x28
 */
int StatusErrCheck(int status);

/**
 * Chooses the tint the party's status ailment gives them.
 *
 * @mangled BtStatusErrColorSet__Fv
 * @address 0x1B1960
 * @size 0xE8
 * Gives the colour a status ailment tints a model with, and 1 while one runs.
 */
int BtStatusErrColorSet();

/**
 * Advances the party's status ailments and applies what they cost.
 *
 * @mangled BtStatusErrStep__Fv
 * @address 0x1B1A50
 * @size 0x154
 */
void BtStatusErrStep();

/**
 * Inflicts one status ailment on the party.
 *
 * @mangled BtSetStatusErr__Fi
 * @address 0x1B1BB0
 * @size 0x1CC
 */
void BtSetStatusErr(int status);

/**
 * Draws the icons of the party's status ailments.
 *
 * @mangled BtStatusErrDraw__Fi
 * @address 0x1B1D80
 * @size 0x16C
 */
void BtStatusErrDraw(int y);

/**
 * Draws one item into the reserved slot area.
 *
 * @mangled setItemToReserved__FPciiPcii
 * @address 0x1B1EF0
 * @size 0x1CC
 */
void setItemToReserved(char *page_name, int x, int y, char *item_name, int dsax, int dsay);

/**
 * Clears the cached map-jump data.
 *
 * @mangled BtMapJumpCashClear__Fv
 * @address 0x1B20C0
 * @size 0x1C
 */
void BtMapJumpCashClear();

/**
 * Loads the steeb message file for the current floor if it changed, then reads the map a jump
 * leads to, its treasure-box models and item-get motions; returns 0 when that map is already
 * loaded, 1 otherwise.
 *
 * @mangled BtMapJumpLoad__FPc
 * @address 0x1B20E0
 * @size 0x70C
 */
int BtMapJumpLoad(char *map_name);

/**
 * Draws a textured cell in world space.
 *
 * @mangled BtSet3DCellModel__FPfP8CTexturefiiiii
 * @address 0x1B27F0
 * @size 0x108
 */
void BtSet3DCellModel(float *world, CTexture *texture, float size, int x, int y, int width, int height, int alpha);

/**
 * Gives the way a shot flies for one heading and pitch.
 *
 * @mangled setShotVector__FPffff
 * @address 0x1D4100
 * @size 0x98
 * @unknownret
 */
void setShotVector(float *velocity, float speed, float angle_y, float angle_x);

/**
 * Gives the way the player's character faces, at one pitch.
 *
 * @mangled getCharacterVector__FPff
 * @address 0x1D41A0
 * @size 0xC0
 * @unknownret
 */
void getCharacterVector(float *vector, float pitch);

/**
 * Gives the way a thrown thing has to leave one point to land on another.
 *
 * @mangled ParabolicInitialVector__FPfPfPfff
 * @address 0x1D4080
 * @size 0x7C
 * @unknownret
 */
void ParabolicInitialVector(float *velocity, float *from, float *to, float gravity, float time);
