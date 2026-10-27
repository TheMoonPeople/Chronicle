#pragma once

#include "common.h"

namespace dc::game {

extern s32    MapNo;
extern s32    OldMapNo;
extern s32    LocalMapNo;
extern s32    StartEventNo;
extern short *SystemMes;
/**
 * Language the game's text and voice are shown in. @see Language.
 */
extern s32 LanguageCode;
extern s32 main_select_padrup;
extern s32 PolyCount;

extern s32 GameClearFlag;
extern s32 main_select_menu_no;
/**
 * Argument for the mode picked from the main select menu, such as "title".
 */
extern char main_select_param[256];

extern s32 old_main_mode;

/**
 * Enables the developer's debug behaviours, such as fixed fishing catches.
 */
extern s32 DebugMode;

} // namespace dc::game
