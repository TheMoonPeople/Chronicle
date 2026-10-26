#pragma once

/**
 * @file
 * Declarations of what ps2/src/main.cpp defines for the rest of the game.
 */

#include "common.h"

#include <libvu0.h>

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

/** Which entry the main menu has the cursor on. */
extern s32 main_select_menu_no;

/** The system messages every message window shares. */
extern short *SystemMes;

/**
 * Map the game is on or about to load.
 */
extern int MapNo;

/** Initial ambient-light colour used by the renderer. */
extern sceVu0FVECTOR ambientlight;

/** Initial parallel-light direction matrix used by the renderer. */
extern sceVu0FMATRIX light;

/** Initial parallel-light colour matrix used by the renderer. */
extern sceVu0FMATRIX lightcolor;

/**
 * Empty string the game-mode table names modes without a file by.
 */
extern char gamemode_empty_string[];

/**
 * Default constructor of CCharacter, written out by hand for the arrays main builds.
 *
 * @mangled __ct__10CCharacterFv
 * @address 0x143530
 * @size 0xd4
 */
extern "C" void *__ct__10CCharacterFv(void *self);

/**
 * Default constructor of CTextureAnime, written out by hand for the arrays main builds.
 *
 * @mangled __ct__13CTextureAnimeFv
 * @address 0x143620
 * @size 0x28
 */
extern "C" void *__ct__13CTextureAnimeFv(void *self);

/**
 * Default constructor of CObject, written out by hand for the arrays main builds.
 *
 * @mangled __ct__7CObjectFv
 * @address 0x143650
 * @size 0x28
 */
extern "C" void *__ct__7CObjectFv(void *self);

/**
 * Default constructor of MotionParam under the name main's array constructors call it by.
 *
 * @mangled __ct__11MotionParamFv
 * @address 0x143610
 * @size 0xc
 */
extern "C" void *GeneratedMotionParamCtor(void *self);
