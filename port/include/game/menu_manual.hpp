#pragma once

#include "common.h"

#include <string>
#include <utility>
#include <vector>

namespace dc::game {

/**
 * Steps of the manual, as MANUAL_MENU_STATE::mode holds them.
 */
// clang-format off
enum ManualMenuMode {
    MANUAL_MODE_LOADING  = 0, /**< Loading. */
    MANUAL_MODE_CLOSING  = 1, /**< Closing. */
    MANUAL_MODE_SELECT   = 2, /**< Choosing an entry. */
    MANUAL_MODE_OPENING  = 3, /**< Loading an entry's images. */
    MANUAL_MODE_READING  = 4, /**< Reading an entry. */
    MANUAL_MODE_SLIDE_IN = 5, /**< Sliding an entry in. */
    MANUAL_MODE_COUNT    = 6, /**< Number of modes. */
};

// clang-format on

class ClsMes;
class CTexture;

/**
 * Holds the manual screen's loading, selection, transition and message state.
 */
struct MANUAL_MENU_STATE {
    s16        common_texture_block;  /**< Texture block containing the manual's common controls. */
    s16        image_texture_block;   /**< Texture block containing the first three page images. */
    s16        extra_texture_block;   /**< Texture block containing any remaining page images. */
    s16        messages_ready;        /**< Whether the manual message archive has been installed. */
    s16        images_ready;          /**< Whether the selected entry's images have been installed. */
    s16        selection_level;       /**< Selects categories (-2), entries (-1), or an open entry (0). */
    s16        mode;                  /**< Current manual screen transition and interaction mode. @see ManualMenuMode. */
    s16        char_width;            /**< Width of one character in the menu's preloaded message font. */
    s32        category;              /**< Selected manual category. */
    s32        entry;                 /**< Selected entry within the category. */
    s32        transition_frame;      /**< Frames elapsed in the current opening or closing transition. */
    s32        image_offset;          /**< Horizontal offset of the strip of page images. */
    s32        image_reset;           /**< Cleared when a page image load starts; nothing reads it. */
    s16        message_page;          /**< Page reached in the selected entry's explanatory message. */
    s16        image_page;            /**< Page image currently displayed. */
    s16       *common_message_buffer; /**< Message data restored while leaving an entry. */
    s16       *menu_message_buffer;   /**< Message data used by the category menu. */
    u_long128 *load_buffer;           /**< Aligned scratch buffer used for manual archives. */
};

STATIC_ASSERT(sizeof(MANUAL_MENU_STATE) == 0x34);

/** State of the open manual screen. */
extern MANUAL_MENU_STATE ManualMenu;

/** Textures for the page images of the selected entry. */
extern CTexture *ManualMenuTex[6];

/** Message window used by the manual's category list. */
extern ClsMes *ManualMsg;

/**
 * Returns the image number for the current manual page and part.
 */
int GetNowManualPartTgaNum();

/**
 * Reports whether the party has the item that unlocks the manual.
 */
int GetGameFlagForManualMenu();

/**
 * Installs a completed manual image load into the menu texture state.
 */
s16 ManualImgEnter();

/**
 * Draws the previous-page and next-page cursors for the manual.
 */
void DrawPrevNextCursor();

/**
 * Initializes the manual menu from its placement data and load buffer.
 */
int InitMenuManual(int *texture_blocks, u_long128 *load_buffer);

/**
 * Builds the message buffer for the current manual entry.
 */
int SetManualMsgBuffer();

/**
 * Releases manual-menu state and restores the surrounding menu.
 */
void ExitManualMenu();

/**
 * Returns the manual menu's current mode.
 */
int GetNowManualMenuMode();

/**
 * Processes manual-menu input and page transitions, and returns 1 on the
 * frame the manual closes.
 */
int MenuManualKey();

/**
 * Draws the current manual page and its controls.
 */
void MenuManualDraw();

// Port additions.

/**
 * Returns the journal's translation keys, each paired with its English default text, for the
 * options screen's table of port strings.
 */
std::vector<std::pair<std::string, std::string>> JournalStrings();

} // namespace dc::game
