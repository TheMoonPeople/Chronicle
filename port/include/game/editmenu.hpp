#pragma once

#include "common.h"

namespace dc::game {

/**
 * What the edit menu hands back to the town, as EDIT_MENU_STATUS::mode holds it.
 */
// clang-format off
enum EditMenuMode {
    EDIT_MENU_MODE_CLOSED  = -1, /**< Closed. */
    EDIT_MENU_MODE_PLACE   = 0,  /**< Place a part picked from the board. */
    EDIT_MENU_MODE_REPLACE = 1,  /**< Place a part picked up off the ground. */
    EDIT_MENU_MODE_MOVE    = 3,  /**< Move or remove placed parts. */
    EDIT_MENU_MODE_EVENT   = 5,  /**< Run a board event. */
};

// clang-format on

/**
 * State that the edit menu keeps between frames.
 */
struct EDIT_MENU_STATUS {
    s32 mode;  /**< What the menu hands back to the town. @see EditMenuMode. */
    s32 parts; /**< Plot of the part the player picked, or -1. */
    u8  unk_08[8];
    s32 event_no; /**< Event the menu asks the loop to run. */
    u8  unk_14[8];
};

STATIC_ASSERT(sizeof(EDIT_MENU_STATUS) == 0x1C);

/**
 * The edit menu's state.
 */
extern EDIT_MENU_STATUS EditMenuStatus;

/**
 * Returns how many of an item the party is carrying.
 *
 * Only weapons are counted; any other item kind gives 0.
 */
int GetNumHowManyItemsHave(int item);

/**
 * Loads the edit menu's data and textures and sets up its windows, pad and cursor.
 *
 * A nonzero start_at_atora opens the menu directly on the Atla page.
 */
void EditMenuInit(int *texture_blocks, int start_at_atora);

/**
 * Reports whether the edit menu is starting and has not yet read its textures and allmenu.mes.
 */
bool EditMenuTexturesLoading();

/**
 * Runs one frame of the edit menu for its current state.
 *
 * Returns nonzero once the menu has finished and EditMenuStatus holds the result.
 */
int EditMenuLoop();

/**
 * Draws the edit menu for its current state.
 */
void EditMenuDraw();

} // namespace dc::game
