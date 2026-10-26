#pragma once

#include "common.h"

/**
 * What the town edit menu is doing, as EditSwitch holds it.
 */
// clang-format off
enum EditMenuState {
    EDIT_MENU_START         = 1,  /**< Starting. */
    EDIT_MENU_SELECT        = 2,  /**< Choosing a page. */
    EDIT_MENU_ATORA         = 3,  /**< Atla page. */
    EDIT_MENU_ATORA_MOVE    = 4,  /**< Moving placed parts. */
    EDIT_MENU_ANALYZE       = 5,  /**< Analysis page. */
    EDIT_MENU_SAVE          = 6,  /**< Save page. */
    EDIT_MENU_OPTION        = 7,  /**< Option page. */
    EDIT_MENU_MANUAL        = 8,  /**< Manual page. */
    EDIT_MENU_ATORA_IN      = 9,  /**< Entering the Atla page. */
    EDIT_MENU_ATORA_MOVE_IN = 10, /**< Entering part moving. */
    EDIT_MENU_ANALYZE_IN    = 11, /**< Entering the analysis page. */
    EDIT_MENU_SAVE_IN       = 12, /**< Entering the save page. */
    EDIT_MENU_OPTION_IN     = 13, /**< Entering the option page. */
    EDIT_MENU_MANUAL_IN     = 14, /**< Entering the manual page. */
    EDIT_MENU_EXIT          = 15, /**< Leaving the menu. */
    EDIT_MENU_ATORA_OUT     = 16, /**< Leaving the Atla page. */
    EDIT_MENU_ANALYZE_OUT   = 18, /**< Leaving the analysis page. */
    EDIT_MENU_SAVE_OUT      = 19, /**< Leaving the save page; never set. */
    EDIT_MENU_OPTION_OUT    = 20, /**< Leaving the option page; never set. */
    EDIT_MENU_MANUAL_OUT    = 21, /**< Leaving the manual page; never set. */
    EDIT_MENU_OUT_END       = 22, /**< End of the leaving states. */
};

// clang-format on

/**
 * How the edit menu's icons are moving, as EdMenuEffectFlag holds it.
 */
// clang-format off
enum EdMenuIconMove {
    ED_MENU_ICON_STILL     = 0, /**< Still. */
    ED_MENU_ICON_MOVE_FROM = 1, /**< Moving away from the menu. */
    ED_MENU_ICON_MOVE_TO   = 2, /**< Moving back to the menu. */
};

// clang-format on

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

class CTexture;

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
 * The edit menu's eased cursor position and current icon selection.
 */
struct EDIT_MENU_CURSOR {
    float x;         /**< Cursor's current screen x, eased toward its target. */
    float y;         /**< Cursor's current screen y, eased toward its target. */
    s8    selection; /**< Index of the selected icon. */
};

STATIC_ASSERT(sizeof(EDIT_MENU_CURSOR) == 0xC);

/**
 * The edit menu's state.
 */
extern EDIT_MENU_STATUS EditMenuStatus;

/**
 * Returns how many of an item the party is carrying.
 *
 * @mangled GetNumHowManyItemsHave__Fi
 * @address 0x2101D0
 * @size 0x104
 */
int GetNumHowManyItemsHave(int item);

/**
 * Loads the edit menu's data and textures and sets up its windows, pad and cursor.
 *
 * @mangled EditMenuInit__FPii
 * @address 0x210AB0
 * @size 0x2E4
 */
void EditMenuInit(int *texture_blocks, int start_at_atora);

/**
 * Runs one frame of the edit menu for its current state and returns the result.
 *
 * @mangled EditMenuLoop__Fv
 * @address 0x210EA0
 * @size 0x110
 */
int EditMenuLoop();

/**
 * Draws the edit menu for its current state.
 *
 * @mangled EditMenuDraw__Fv
 * @address 0x210FB0
 * @size 0x304
 */
void EditMenuDraw();

/**
 * Resting screen position of each edit menu icon, one x/y pair per icon.
 */
extern float MenuIconPos[6][2];

/**
 * The edit menu's cursor.
 */
extern EDIT_MENU_CURSOR EdCur;

/**
 * How far each of the analysis page's three bars has filled, in screen columns.
 */
extern float AnalyzeFill[3];

/**
 * The icon shown at each edit menu slot.
 */
extern s8 EditMenuIconID[6];

/**
 * The texture the analysis page's headings come from.
 */
extern CTexture *Analyze;

/**
 * The texture the analysis page's panels, bars and digits come from.
 */
extern CTexture *AnaBar;

/**
 * Whether the edit menu's page textures have finished reading.
 */
extern int EdMenuTextureReadEndFlag;

/**
 * Texture block containing the edit menu's main page textures.
 */
extern int EdMenuTextureBlock;

/**
 * Work buffer the edit menu reads its files into.
 */
extern u_long128 *EdMenuWorkBuf;

/**
 * Texture block containing the edit menu's first extra texture set.
 */
extern int EdMenuExTextureBlock;

/**
 * Texture block containing the edit menu's second extra texture set.
 */
extern int EdMenuExTextureBlock1;

/**
 * Texture block containing the edit menu's third extra texture set.
 */
extern int EdMenuExTextureBlock2;

/**
 * The state the edit menu is in. @see EditMenuState.
 */
extern int EditSwitch;

/**
 * How bright the edit menu's message windows draw while it closes.
 */
extern s16 EdMenuRGB;

/**
 * The frame count of the edit menu's current transition.
 */
extern int EdEffectCt;

/**
 * Opacity of the edit menu's help window.
 */
extern s16 EdMenuHelpWinAlpha;

/**
 * The selection on the edit menu's analyze page.
 */
extern s16 AnalyzeSelect;

/**
 * Message the edit menu's second window shows.
 */
extern int EdMenuMesNo2;

/**
 * Whether the edit menu's second window's message is to be made.
 */
extern int EdMenuMesMake2;

/**
 * The speed-up the analyze bar draws with, set to 2 while any input is held.
 */
extern s16 ButtonAdd;

/**
 * How far the analysis page's scrolling background has moved.
 */
extern s16 AnalyzeBackBlockCnt;

/**
 * The edit menu icon's movement. @see EdMenuIconMove.
 */
extern s16 EdMenuEffectFlag;

/**
 * The frame count of the edit menu icon's movement.
 */
extern float EdMenuEffectCt;

/**
 * Screen position of the edit menu's help window.
 */
extern float WindowPos[2];

/**
 * Width of the edit menu's help window.
 */
extern float EditMenuWinW;

/**
 * Height of the edit menu's help window.
 */
extern float EditMenuWinH;

/**
 * Whether the edit menu's second window is to be made.
 */
extern s16 MakeWin2Flag;
