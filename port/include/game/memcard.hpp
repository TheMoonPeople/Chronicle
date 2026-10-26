#pragma once

#include "common.h"

#include "menu_draw.hpp"
#include "menuetc.hpp"

/**
 * What MenuAtoraSelectKey reports.
 */
// clang-format off
enum AtoraSelectResult {
    ATORA_SELECT_NONE        = 0,   /**< Nothing happened. */
    ATORA_SELECT_PLACE       = 10,  /**< A part was picked to place. */
    ATORA_SELECT_CHANGE_PAGE = 20,  /**< The town page changed. */
    ATORA_SELECT_CLOSE       = 100, /**< The board was closed. */
    ATORA_SELECT_PLACE_EXIT  = 110, /**< A board event was chosen and the board closed. */
};

// clang-format on

/**
 * What MenuSaveKey reports.
 */
// clang-format off
enum MenuSaveResult {
    MENU_SAVE_RUNNING = 0, /**< Still running. */
    MENU_SAVE_LOADED  = 1, /**< Closed after a load. */
    MENU_SAVE_CLOSED  = 2, /**< Closed otherwise. */
};

// clang-format on

/**
 * How an Atla chip links to the chip it needs, as AtoraTipStatusSearch codes it.
 */
// clang-format off
enum AtoraLinkKind {
    ATORA_LINK_NONE   = 0,  /**< No pending link. */
    ATORA_LINK_BESIDE = 1,  /**< Needs the chip beside it. */
    ATORA_LINK_ABOVE  = 2,  /**< Needs the chip above it. */
    ATORA_LINK_HIDDEN = 50, /**< The chip cannot be shown. */
};

// clang-format on

/**
 * Sides of the Atla screen, as MENU_ATORA_SEL::mode and ATORA_TIP_HAVE::mode hold them.
 */
// clang-format off
enum AtoraSide {
    ATORA_SIDE_BOARD     = 0, /**< The town board. */
    ATORA_SIDE_CHIP_LIST = 1, /**< The chip list. */
};

// clang-format on

/**
 * Steps of the Atla screen, as MENU_ATORA_SEL::step holds them.
 */
// clang-format off
enum AtoraStep {
    ATORA_STEP_RUN            = 0,  /**< Taking input. */
    ATORA_STEP_FADE_IN        = 1,  /**< Fading in. */
    ATORA_STEP_FADE_OUT       = 2,  /**< Fading out. */
    ATORA_STEP_EVENT_START    = 3,  /**< Starting a board event. */
    ATORA_STEP_COMPLETE_FLASH = 4,  /**< Flashing a completed part. */
    ATORA_STEP_CHANGE_MAP     = 6,  /**< Changing town. */
    ATORA_STEP_EVENT_FADE_OUT = 7,  /**< Fading out into the East King event. */
    ATORA_STEP_EVENT_PLAY     = 8,  /**< Running the East King event. */
    ATORA_STEP_EVENT_FADE_IN  = 9,  /**< Fading back in after the East King event. */
    ATORA_STEP_WARNING        = 10, /**< Warning shown. */
};

// clang-format on

/**
 * Why the Atla screen was opened, as MENU_ATORA_SEL::open_mode holds it.
 */
// clang-format off
enum AtoraOpenMode {
    ATORA_OPEN_VIEW  = 0, /**< For viewing. */
    ATORA_OPEN_WARN  = 1, /**< Picking a part shows a warning. */
    ATORA_OPEN_PLACE = 2, /**< Picking a part places it. */
};

// clang-format on

/**
 * Where the option screen was opened from, as InitMenuOption takes it.
 */
// clang-format off
enum OptionOpenMode {
    OPTION_OPEN_TITLE     = 0, /**< The title screen. */
    OPTION_OPEN_GAME_MENU = 1, /**< An in-game menu. */
};

// clang-format on

/**
 * Steps of the option screen, as OPTION_MENU_STATE::step holds them.
 */
// clang-format off
enum OptionStep {
    OPTION_STEP_RUN      = 0, /**< Taking input. */
    OPTION_STEP_FADE_IN  = 1, /**< Fading in. */
    OPTION_STEP_FADE_OUT = 2, /**< Fading out. */
};

// clang-format on

/**
 * Parts of the option screen the cursor can be on, as OPTION_MENU_STATE::buttons holds them.
 */
// clang-format off
enum OptionCursorArea {
    OPTION_AREA_ROWS        = 0, /**< The option rows. */
    OPTION_AREA_EXIT_BUTTON = 1, /**< The exit button. */
    OPTION_AREA_SCREEN_POS  = 2, /**< Screen position adjustment. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CEditPartsInfo;
class CMemoryCardAccess;
struct EDITPARTS_INFO;
struct MC_CARD_INFO;
struct RECT;

/**
 * Records the chip that the georama board's cursor has picked up and where it
 * came from.
 */
struct ATORA_TIP_HAVE {
    s32 mode; /**< Where the chip came from. @see AtoraSide. */
    s32 unk_04;
    s32 parts_no; /**< Board position of the part whose slot the chip came from. */
    s32 slot;     /**< Chip slot of the part, or entry of the chip list, that the chip came from. */
    s16 tip_no;   /**< Chip that the cursor holds, or -1 when it holds none. */
};

STATIC_ASSERT(sizeof(ATORA_TIP_HAVE) == 0x14);

/**
 * Holds the state of the georama board screen.
 */
struct MENU_ATORA_SEL {
    s32            mode;      /**< Which half of the screen holds the cursor. @see AtoraSide. */
    s32            open_mode; /**< How the screen was opened. @see AtoraOpenMode. */
    s32            edit_map;  /**< Georama that the menu was opened for, which picks the chip list's first page. */
    s16            map_no;    /**< Georama that the board shows. @see Town. */
    s16            board_pos; /**< Board position of the part that the cursor is on. */
    s32            scroll_y;  /**< Where the board has scrolled to, as the pixel offset of its first row. */
    PERSONAL_BOARD board;     /**< Board that lists the chips the player holds. */
    u8             unk_174[0xA];
    s16            last_board_pos; /**< Board position the cursor was on when the screen last handed control elsewhere. */
    float          cursor_x;       /**< Where the board's cursor icon draws, from the left of the screen. */
    float          cursor_y;       /**< Where the board's cursor icon draws, from the top of the screen. */
    s32            cursor_icon_u;  /**< Column of the stay-frame texture that the cursor icon is cut from, which picks its pose. */
    s32            step;           /**< What the screen is doing. @see AtoraStep. */
    s32            step_count;     /**< Frames the screen has spent on its current step. */
    s16            name_alpha;     /**< Opacity the part names draw at while the screen is not fading. */
    u8             unk_196[2];
    s16            event_board_pos; /**< Board position of the part whose completion event played, restored when the board reopens. */
    s16            event_flag;      /**< Whether the georama menu is running an event. */
    u8             unk_19C[8];
    s16           *prev_mes_buff; /**< Message file CommonMenuMes2 held before the screen opened. */
    s16            load_state;    /**< Cleared whenever the georama's board files start loading. */
    u8             unk_1AA[2];
};

STATIC_ASSERT(sizeof(MENU_ATORA_SEL) == 0x1AC);

/**
 * Returns one when a card is present in the port and is a PlayStation 2 memory
 * card.
 *
 * @mangled McCheckMCPs2__FP12MC_CARD_INFO
 * @address 0x216D50
 * @size 0x34
 */
int McCheckMCPs2(MC_CARD_INFO *card);

/**
 * Draws a menu sprite that sways a few pixels around a position in time with
 * the cursor animation.
 *
 * @mangled DrawObjectVibe__FiiP8CTexture8CRect_i_Uci
 * @address 0x216D90
 * @size 0x134
 */
void DrawObjectVibe(int x, int y, CTexture *texture, CRect_i_ src_rect, unsigned char alpha, int flag);

/**
 * Draws a swaying menu sprite from a source rectangle given as a RECT.
 *
 * @mangled DrawObjectVibe__FiiP8CTexture4RECTUci
 * @address 0x216ED0
 * @size 0x68
 */
void DrawObjectVibe(int x, int y, CTexture *texture, RECT src_rect, unsigned char alpha, int flag);

/**
 * Draws one swaying 32-pixel icon of the common menu texture, with a shadow
 * beneath it when asked.
 *
 * @mangled DrawMenuObjectVibe__Fiiii
 * @address 0x216F40
 * @size 0xD0
 */
void DrawMenuObjectVibe(int x, int y, int shadow, int icon_u);

/**
 * Draws a help window of the given size from its nine-patch texture.
 *
 * @mangled DrawMenuHelpWindow__FP8CTextureiiiffi
 * @address 0x217010
 * @size 0x388
 */
void DrawMenuHelpWindow(CTexture *texture, int style, int x, int y, float width, float height, int alpha);

/**
 * Draws a nine-piece help window from the tiles at a texture position,
 * stretching its middle by a width and a height in tile units.
 *
 * @mangled MenuHelpWinDraw__FiiffiiiP8CTexture
 * @address 0x2173A0
 * @size 0x368
 */
void MenuHelpWinDraw(int x, int y, float width, float height, int alpha, int u, int v, CTexture *texture);

/**
 * Draws the same help window as MenuHelpWinDraw with the width and height of
 * its middle in pixels.
 *
 * @mangled MenuHelpWinDraw2__FiiffiiiP8CTexture
 * @address 0x217710
 * @size 0x34C
 */
void MenuHelpWinDraw2(int x, int y, float width, float height, int alpha, int u, int v, CTexture *texture);

/**
 * Draws a help window from the common menu texture.
 *
 * @mangled MenuHelpWinDraw__Fiiffi
 * @address 0x217A60
 * @size 0x9C
 */
void MenuHelpWinDraw(int x, int y, float width, float height, int alpha);

/**
 * Draws the four corners of a selection frame around a rectangle, drawn in by
 * an offset that cycles every 30 frames.
 *
 * @mangled DrawMenuWaku__FffiiiP8CTexturei
 * @address 0x217B00
 * @size 0x2BC
 */
void DrawMenuWaku(float x, float y, int width, int height, int type, CTexture *texture, int alpha);

/**
 * Draws a number right-aligned to a position, clipped to the height of the
 * screen, and returns the left edge of its leading digit.
 *
 * @mangled DrawMenuNumber__FiiiP8CTexture4RECTii
 * @address 0x217DC0
 * @size 0x58
 */
int DrawMenuNumber(int number, int x, int y, CTexture *texture, RECT rect, int overlap, int flag);

/**
 * Draws a number right-aligned to a position in a tint colour, and returns the
 * left edge of its leading digit.
 *
 * @mangled DrawMenuNumber__Fiii4RECTP8CTextureiUcUcUci
 * @address 0x217E20
 * @size 0x1AC
 */
int DrawMenuNumber(int number, int x, int y, RECT rect, CTexture *texture, int overlap, unsigned char r, unsigned char g, unsigned char b, int flag);

/**
 * Draws a number right-aligned to a position, clipped to a band of the screen,
 * and returns the left edge of its leading digit.
 *
 * @mangled DrawMenuNumber__Fiii4RECTP8CTextureiiii
 * @address 0x217FD0
 * @size 0x1A0
 */
int DrawMenuNumber(int number, int x, int y, RECT rect, CTexture *texture, int overlap, int top, int bottom, int flag);

/**
 * Returns how many codes the first line of a system message holds, or zero when
 * the message is missing.
 *
 * @mangled GetMsgLengthMenu__FP6ClsMesi
 * @address 0x218170
 * @size 0x64
 */
int GetMsgLengthMenu(ClsMes *mes, int mes_no);

/**
 * Draws the icon of a georama chip with a drop shadow, clipped to a band of the
 * screen and skipped outside the board.
 *
 * @mangled DrawAtoraParts__Fiiiiii
 * @address 0x2182C0
 * @size 0x1A8
 */
void DrawAtoraParts(int x, int y, int tip_no, int top, int bottom, int alpha);

/**
 * Returns the message number describing a georama element.
 *
 * @mangled GetAtraMsgNo__Fii
 * @address 0x218CC0
 * @size 0xE0
 */
int GetAtraMsgNo(int map_no, int element);

/**
 * Draws how many of a georama part have been built, over its total.
 *
 * @mangled DrawAtraBuildNum__FP14EDITPARTS_INFOiii
 * @address 0x2192F0
 * @size 0x164
 */
void DrawAtraBuildNum(EDITPARTS_INFO *info, int x, int y, int alpha);

/**
 * Draws the plate of one georama part with its picture, placement gauge and
 * chip sockets, or a pulsing completion sprite once the part is ready for its
 * completion event.
 *
 * @mangled DrawAtora__Fiiii
 * @address 0x219460
 * @size 0x85C
 */
void DrawAtora(int x, int y, int parts_index, int alpha);

/**
 * Returns whether the georama menu is running an event.
 *
 * @mangled GetMenuAtraEventFlag__Fv
 * @address 0x21A090
 * @size 0x10
 */
int GetMenuAtraEventFlag();

/**
 * Sets up the georama menu's state, message windows and drawing buffer.
 *
 * @mangled InitMenuAtora1__FiiPiP1
 * @address 0x21A130
 * @size 0x310
 */
void InitMenuAtora1(int open_mode, int edit_map, int *texture_blocks, u_long128 *buffer);

/**
 * Opens the board for the given georama, queueing its textures when they are
 * not loaded yet, picking its part records and placing the cursor.
 *
 * @mangled InitMenuAtoraSelect__Fi
 * @address 0x21A440
 * @size 0x44C
 */
void InitMenuAtoraSelect(int map_no);

/**
 * Draws one frame of the georama board screen.
 *
 * @mangled DrawMenuAtoraSelect__Fv
 * @address 0x21AA80
 * @size 0x3FC
 */
void DrawMenuAtoraSelect();

/**
 * Steps the georama board screen and returns what the player did. @see AtoraSelectResult.
 *
 * @mangled MenuAtoraSelectKey__Fv
 * @address 0x21C2E0
 * @size 0x7A4
 */
int MenuAtoraSelectKey();

/**
 * Draws the names of the parts in the three board slots around the cursor, and
 * the board's edge fade.
 *
 * @mangled AtoraNameDraw__Fi
 * @address 0x21DC60
 * @size 0x3BC
 */
void AtoraNameDraw(int unused);

/**
 * Starts the option screen, queueing option.pac and copying the current
 * settings into its rows, and returns zero when the loader reports no size for
 * the file.
 *
 * @mangled InitMenuOption__FiiP1
 * @address 0x21E4D0
 * @size 0x2A8
 */
int InitMenuOption(int mode, int block_no, u_long128 *buffer);

/**
 * Steps the option screen and returns one on the frame it closes and stores the
 * settings, zero otherwise.
 *
 * @mangled MenuOptionKey__Fv
 * @address 0x21E9B0
 * @size 0x8F8
 */
int MenuOptionKey();

/**
 * Draws one frame of the option screen.
 *
 * @mangled DrawMenuOption__Fv
 * @address 0x21F2B0
 * @size 0x82C
 */
void DrawMenuOption();

/**
 * Returns whether the option screen has started to close.
 *
 * @mangled OptionMenuFadeOutStart__Fv
 * @address 0x21FAE0
 * @size 0x24
 */
int OptionMenuFadeOutStart();

/**
 * Starts the save screen in the given mode, queueing savetex.pak and starting
 * the memory card library, and returns zero if the library fails to start.
 *
 * @mangled InitMenuSave__FiiP1
 * @address 0x21FB10
 * @size 0x268
 */
int InitMenuSave(int mode, int block_no, u_long128 *buffer);

/**
 * Steps the save screen and reports whether it runs or how it closed. @see MenuSaveResult.
 *
 * @mangled MenuSaveKey__Fv
 * @address 0x21FED0
 * @size 0x4F8
 */
int MenuSaveKey();

/**
 * Draws one frame of the save screen.
 *
 * @mangled DrawMenuSave__FPc
 * @address 0x2203D0
 * @size 0x9C0
 */
void DrawMenuSave(char *frame_name);

/**
 * Holds the georama parts of a town the player is not standing in.
 */
extern CEditPartsInfo BtEditPartsInfo;

/**
 * The memory card state the save, load and option screens work through.
 */
extern CMemoryCardAccess McAccess;

/**
 * Frame counter the swaying menu sprites animate with.
 */
extern s32 CursorVibeCnt;

/**
 * The part records of the georama that the board screen shows.
 */
extern CEditPartsInfo *CommonMenuAtoraInfo;

/**
 * The stay-frame texture that menu selection frames, digits and the hand cursor draw from.
 */
extern CTexture *StayTex;

/**
 * The texture of the item icons.
 */
extern CTexture *ItemIcon;

/**
 * The texture of the weapon icons.
 */
extern CTexture *WepIcon;

/**
 * The texture that the georama board's plates, gauges and frames draw from.
 */
extern CTexture *Sozai;

/**
 * The texture of a chip socket whose chip has been placed.
 */
extern CTexture *HoleGold;

/**
 * The texture of an empty chip socket.
 */
extern CTexture *HoleGray;

/**
 * The texture of the seal a completed georama shows.
 */
extern CTexture *CompleteTex;

/**
 * The texture of the icons of chips that stand for objects.
 */
extern CTexture *ObTip;

/**
 * The texture of the icons of chips that stand for residents.
 */
extern CTexture *ObPerson;

/**
 * The texture of the georama board's town tags.
 */
extern CTexture *VillageBar;

/**
 * The texture of the georama board's town names.
 */
extern CTexture *VillageName;

/**
 * The state of the georama board screen.
 */
extern MENU_ATORA_SEL MenuAtoraSel;

/**
 * The message file that the georama board's messages are read into.
 */
extern short *GetAtraMsgReadBuf;

/**
 * Texture of the second item icon sheet.
 */
extern CTexture *ItemIcon2;

/**
 * Texture of the attachment icons.
 */
extern CTexture *AttachIcon;

/** The texture that the save screen's file boards draw from. */
extern CTexture *SaveBoard;

/**
 * Name of the pack entry that holds the menu messages.
 */
extern char allmenu_mes[];
