#pragma once

#include "common.h"

#include "editground.hpp"
#include "editloop.hpp"

namespace dc::game {

/**
 * Holds the settings and records an interior's info script describes.
 */
struct EDIT_IN_INFO {
    char                   name[0x40]; /**< Path of the interior's map file. */
    u8                     unk_040[0x480];
    float                  projection; /**< Distance of the interior's projection plane. */
    u8                     unk_4c4[0xC];
    float                  ambient[4];            /**< Ambient light colour. */
    float                  light_direction[4][4]; /**< Normalized light directions, one light per column. */
    float                  light_colour[4][4];    /**< Colours of the four lights. */
    float                  background_colour[4];  /**< Colour the interior clears to. */
    EDIT_FOG_INFO          fog;                   /**< Fog distances and colour. */
    u8                     unk_584[0x5AC];
    OBJ_ANIME_SEQ          obj_anime[32];     /**< Object animations of the interior's parts. */
    EDIT_EFFECT_INFO       effects[32];       /**< Effects standing on the interior's parts. */
    ED_EVENT_POINT         event_points[32];  /**< Event points the interior's parts define. */
    EDIT_MOTION_PARTS_INFO motion_parts[4];   /**< Named interior parts with scripted motion. */
    EDIT_WATER_INFO        water_surfaces[8]; /**< Water surfaces the interior defines. */
};

STATIC_ASSERT(sizeof(EDIT_IN_INFO) == 0x44C0);

/**
 * Settings of the interior being run.
 */
extern EDIT_IN_INFO *EdInInfo;

/**
 * Builds everything one interior runs on: its data, models, script and camera.
 */
int EditInInit(float time, char *name);

/**
 * Runs one frame of the interior and reports when it is to be left.
 */
int EditInLoop();

/**
 * Finds the map jump the player is standing on and facing.
 */
EPARTS_FUNC_DATA *SearchMapJump(float *position, float *rotation);

/**
 * Puts the player where the interior's map jump says they arrive.
 */
void GetMapJumpPos(CCharacter *chara);

/**
 * Decodes one interior part's function records from their disc layout into points; returns how many
 * it wrote.
 */
int GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points);

/**
 * Reads one interior object's levels of detail and hangs them off its part.
 */
void LoadMapObject(CMapParts *parts, u_int **data, CDataAlloc2<1> *alloc);

} // namespace dc::game
