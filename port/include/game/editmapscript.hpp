#pragma once

#include "common.h"

#include "editloop.hpp"

/**
 * What the town loop runs, as GameMode holds it.
 */
// clang-format off
enum EdGameMode {
    ED_MODE_NONE                = -1, /**< No mode recorded yet. */
    ED_MODE_UNK_0               = 0,  /**< Does nothing; never set. */
    ED_MODE_WALK                = 1,  /**< Walking. */
    ED_MODE_TALK                = 2,  /**< Talking. */
    ED_MODE_TIME_CHANGE         = 3,  /**< Counting down a time change. */
    ED_MODE_GEORAMA             = 4,  /**< Georama editing. */
    ED_MODE_GEORAMA_MENU_INIT   = 5,  /**< Starting the edit menu from Georama editing. */
    ED_MODE_GEORAMA_MENU        = 6,  /**< Edit menu. */
    ED_MODE_MENU_INIT           = 7,  /**< Starting a menu from walking. */
    ED_MODE_MENU                = 8,  /**< Script or battle menu. */
    ED_MODE_RETURN_MENU_WALK    = 9,  /**< Pause menu from walking. */
    ED_MODE_RETURN_MENU_GEORAMA = 10, /**< Pause menu from Georama editing. */
    ED_MODE_DOOR_OPEN           = 11, /**< Opening a door. */
    ED_MODE_MAP_JUMP            = 12, /**< Jumping to another map. */
    ED_MODE_UNK_D               = 13, /**< Does nothing; never set. */
    ED_MODE_EVENT               = 14, /**< Event. */
    ED_MODE_UNK_F               = 15, /**< Does nothing; never set. */
    ED_MODE_FISHING             = 16, /**< Fishing. */
};

// clang-format on

struct SPI_FUNC_PARAM;

/** Functions the map script may call. */
extern SPI_FUNC_PARAM func_table;

/** Current game mode of the town editor. @see EdGameMode. */
extern int GameMode;

/** Number of images the map script has declared. */
extern int texture_list;
/** Number of frame objects the map script has declared. */
extern int fobject_list;
/** Number of map objects the map script has declared. */
extern int mapobj_list;
/** Number of object animations the map script has declared. */
extern int objanime_list;
/** Light entry the map script's AMBIENT and LIGHT_C tags fill, chosen by LIGHT_NO. */
extern int light_no;
/** Number of parts effects the map script has declared. */
extern int partseffect_list;
/** Number of object effects the map script has declared. */
extern int objeffect_list;
/** Number of object timers the map script has declared. */
extern int objtimer_list;
/** Number of event points the map script has declared. */
extern int event_list;
/** Number of water surfaces the map script has declared. */
extern int water_list;
/** Map number named by the last MAPJUMP tag. */
extern int mapjump_id;
/** Number of villagers the map script has declared. */
extern int people_list;
/** Time-table set the map script's TIME_TABLE tags fill, chosen by TIME_TABLE_NO. */
extern int week_no;
/** Name of the sky or sun layer the map script's following tags refer to. */
extern char *objframe;
/** Map object the map script is filling in. */
extern CMapObject *mapobj;
/** Map parts the map script is filling in. */
extern CMapParts *mapparts;
/** Water surface the map script is filling in. */
extern EDIT_WATER_INFO *water_info;
/** Index of the parts the map script is filling in. */
extern int now_parts_no;
/** Whether the map script being read is the compiled .cfb form. */
extern int binary;
/** Number of edit-area rectangles the map script has declared. */
extern int edit_rect_list;
/** Number of motion parts the map script has declared. */
extern int motion_parts_list;

/**
 * Returns the integer value addressed by a script argument slot.
 *
 * @mangled test__FPPv
 * @address 0x1741D0
 * @size 0x10
 */
int test(void **argument);

/**
 * Reports whether the current editor state should draw the fishing interface.
 */
int FishingDrawCheck();

/**
 * Tests whether a motion crossed a nearby target time during the current step.
 */
int CheckMotionTime(float target, float previous, float current);

/**
 * Provides the editor hook for stopping all currently managed sound.
 */
void StopAllSound();

/**
 * Loads one file of the active script directory into the editor's read buffer.
 *
 * @mangled EdLoadFile__FPc
 */
void *EdLoadFile(char *name);

/** Name of the map the editor jumps to next. */
extern char mapjump_name[0x80];

/** The default light directions of the editor's twelve lights. */
extern float def_light[12][4];
