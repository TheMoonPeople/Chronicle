#pragma once

/**
 * @file
 * Top-level game modes and the globals that the main loop shares with the rest of the game.
 */

#include "common.h"

#include <libvu0.h>

#include "dataalloc.hpp"

namespace dc::game {

/**
 * What the main loop runs; each mode loads its overlay and calls its own
 * initialise and step functions.
 */
enum GameMode {
    GAME_MODE_TITLE = 0,         /**< Title screen (TitleInit, TitleLoop). */
    GAME_MODE_RUSH_MOVIE = 1,    /**< Attract movie before the title (RushInit). */
    GAME_MODE_EDIT = 2,          /**< Towns and the Georama editor (EditInit, EditLoop). */
    GAME_MODE_DUNGEON = 3,       /**< Dungeons (GameInit, GameLoop). */
    GAME_MODE_UNUSED_4 = 4,      /**< Never entered; loads the dungeon overlay. */
    GAME_MODE_OPENING = 5,       /**< Opening movie (OpeningInit). */
    GAME_MODE_UNUSED_6 = 6,      /**< Never entered; does nothing. */
    GAME_MODE_MENU = 7,          /**< Developer menu (MenuInit, MenuLoop). */
    GAME_MODE_UNUSED_8 = 8,      /**< Never entered; does nothing. */
    GAME_MODE_LOADER = 9,        /**< Loads a dungeon before entering it (LoaderInit). */
    GAME_MODE_MEMORY_CHECK = 10, /**< Memory card check at start-up (MemCheckInit). */
    GAME_MODE_TRIAL_END = 11,    /**< End of the trial version (TrialEndInit). */
    GAME_MODE_UNUSED_12 = 12,    /**< Never entered; does nothing. */
    GAME_MODE_SAVE = 13,         /**< Save screen (InitSave, LoopSave). */
    GAME_MODE_LANGUAGE = 14,     /**< Language select (LangsetInit). */
};

/**
 * Which entry the main menu has the cursor on.
 */
extern s32 main_select_menu_no;

/**
 * Top-level game mode currently being updated. @see GameMode
 */
extern s32 mode;

/**
 * Memory-card operation selected by the developer menu.
 */
extern s32 mc_mode;

/**
 * The next map the game loads; -1 for none.
 */
extern s32 NextMapNo;

/**
 * The system messages every message window shares.
 */
extern short *SystemMes;

/**
 * Arena the system messages are read into.
 */
extern CDataAlloc<1, 6000> SystemMesBuffer;

/**
 * Map the game is on or about to load.
 */
extern int MapNo;

/**
 * Initial ambient-light colour used by the renderer.
 */
extern sceVu0FVECTOR ambientlight;

/**
 * Initial parallel-light direction matrix used by the renderer.
 */
extern sceVu0FMATRIX light;

/**
 * Initial parallel-light colour matrix used by the renderer.
 */
extern sceVu0FMATRIX lightcolor;

} // namespace dc::game
