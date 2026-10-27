#pragma once

#include "common.h"

#include "itemdata.hpp"
#include "shop.hpp"

namespace dc::game {

/**
 * Pages of a personal board, as PERSONAL_BOARD::page holds them.
 */
// clang-format off
enum PersonalBoardPage {
    BOARD_PAGE_NONE         = -1, /**< No page. */
    BOARD_PAGE_ITEM         = 0,  /**< Items. */
    BOARD_PAGE_WEAPON       = 1,  /**< Weapons. */
    BOARD_PAGE_ATTACH       = 2,  /**< Attachments. */
    BOARD_PAGE_ATLA_VILLAGE = 3,  /**< First town's Atla; each later town follows it. */
};

// clang-format on

/**
 * Parts of a personal board the cursor can be on, as PERSONAL_BOARD::cursor_area holds them.
 */
// clang-format off
enum PersonalBoardArea {
    PERSONAL_BOARD_AREA_CELLS = 1, /**< The cells. */
    PERSONAL_BOARD_AREA_TRASH = 2, /**< The trash can. */
};

// clang-format on

/**
 * Menus a personal board serves, as PERSONAL_BOARD::menu_kind holds them.
 */
// clang-format off
enum PersonalBoardKind {
    PERSONAL_BOARD_ITEM          = 0, /**< Item menu and shops. */
    PERSONAL_BOARD_WEAPON_ATTACH = 1, /**< Weapon menu's attachment list. */
    PERSONAL_BOARD_ATLA          = 2, /**< Atla board. */
};

// clang-format on

/**
 * Message window presets that InitMenuMesSet sets up.
 */
// clang-format off
enum MenuMesSet {
    MENU_MES_SET_ALLMENU       = 0, /**< Common menu messages. */
    MENU_MES_SET_EVENT_ITEM    = 1, /**< Event item selection. */
    MENU_MES_SET_SHOP          = 2, /**< Shops. */
    MENU_MES_SET_NAME_ENTRY    = 3, /**< Name entry. */
    MENU_MES_SET_ATLA_NAME     = 4, /**< Atla name window only. */
    MENU_MES_SET_WORLD_MAP     = 5, /**< World map opened from a town script. */
    MENU_MES_SET_DUNGEON_ENTER = 7, /**< Dungeon entrance menu. */
};

// clang-format on

/**
 * Steps of the dungeon entrance menu, as DUN_ENTER_MENU::state holds them.
 */
// clang-format off
enum DunEnterMenuState {
    DUN_ENTER_SELECT   = 0, /**< Choosing a floor. */
    DUN_ENTER_FADE_IN  = 1, /**< Loading and fading in. */
    DUN_ENTER_FADE_OUT = 2, /**< Fading out before returning the floor. */
    DUN_ENTER_DIRECT   = 3, /**< Loading a requested floor without drawing. */
};

// clang-format on

struct DNG_CONSUMABLE;
class CCamera;
class CRect_i_;
class CTexture;
class CUserStatus;
class CDngStatusData;
class ClsMes;
struct SAVEDATA_INFO;
struct spRGBA;

/**
 * State of the dungeon entrance menu, and what it shows of each floor.
 */
struct DUN_ENTER_MENU {
    s8    dungeon;             /**< Dungeon the menu is for. */
    s8    scroll_top;          /**< First floor the list shows. */
    s8    selected_floor;      /**< Floor the cursor stands on. */
    s8    requested_floor;     /**< Floor the player asked to enter. */
    float list_y;              /**< Screen Y of the floor list's first row, eased toward the scroll position. */
    float scroll_bar_y;        /**< Screen Y of the scroll bar, eased toward the scroll position. */
    s8    textures_ready;      /**< Set once the menu's textures and messages have been installed. */
    s8    texture_block;       /**< Texture block the menu loads into. */
    s8    loading;             /**< Set while the menu's archive is still being read. */
    s8    floor_count;         /**< Floors the list holds. */
    s8    result;              /**< Floor chosen, or a negative value while the menu runs. */
    s8    max_atra[100];       /**< Atla each floor holds. */
    s8    collected_atra[100]; /**< Atla the player has taken from each floor. */
    u8    unk_0D9;
    s16   kills[100]; /**< Monsters the player has defeated on each floor. */
    u8    unk_1A2[2];
    s16   state; /**< Step of the menu's state machine. */
    u8    unk_1A6[2];
    s32   counter; /**< Frames spent in the current step. */
};

STATIC_ASSERT(sizeof(DUN_ENTER_MENU) == 0x1AC);

/**
 * State of a personal inventory board, the item list that the item, shop and Atla menus share.
 */
struct PERSONAL_BOARD {
    s32             menu_kind;   /**< Menu the board belongs to. @see PersonalBoardKind. */
    s32             page;        /**< Page of the board that is showing. @see PersonalBoardPage. */
    s32             cursor_area; /**< Part of the board the cursor is on. @see PersonalBoardArea. */
    s32             cursor;      /**< Cell that the cursor is on. */
    float           y;           /**< Screen Y the board draws at, eased toward its top row. */
    float           scroll;      /**< Scroll bar position the board draws. */
    s32             top_row;     /**< Row the board shows first. */
    s16             trash_anim;  /**< One while the trash can plays its throw animation. */
    u8              unk_1E[2];
    s32             trash_frame;        /**< Frame of the trash can's throw animation. */
    ITEM_PACK      *item_pack;          /**< Item pack the board lists. */
    DNG_CONSUMABLE *consumables;        /**< Stored attachments the board lists. */
    s16            *atla_elements;      /**< Atla piece in each cell of a village page, or -1 for an empty cell. */
    IHAVEITEM       held_item;          /**< Item the board's cursor is holding. */
    WEAPON_HAVE     weapon;             /**< Weapon record of the weapon the cursor is holding. */
    ATTACH_LIST     held_attach;        /**< Attachment record of the attachment the cursor is holding. */
    s32             held_equipped_slot; /**< Slot the held weapon was equipped in, or -1 when it was not equipped. */
};

STATIC_ASSERT(sizeof(PERSONAL_BOARD) == 0x160);

/**
 * Where one main menu icon and its label sit on the menu frame texture.
 */
struct MENU_ICON_INFO {
    s16 id; /**< Icon the entry describes. */
    s16 unk_02;
    s32 selected_u;   /**< Texel X of the icon drawn while it is selected. */
    s32 selected_v;   /**< Texel Y of the icon drawn while it is selected. */
    s32 u;            /**< Texel X of the icon drawn while it is not selected. */
    s32 v;            /**< Texel Y of the icon drawn while it is not selected. */
    s32 label_u;      /**< Texel X of the icon's label. */
    s32 label_v;      /**< Texel Y of the icon's label. */
    s32 label_width;  /**< Width of the icon's label. */
    s32 label_height; /**< Height of the icon's label. */
    s16 help_mes_no;  /**< Message the help window shows for the icon. */
    s16 unk_26;
};

STATIC_ASSERT(sizeof(MENU_ICON_INFO) == 0x28);

/**
 * The dungeon entrance menu.
 */
extern DUN_ENTER_MENU DEnterMenu;

/**
 * Dungeon progress the entrance menu shows.
 */
extern CDngStatusData *DEnterStatusPt;

/**
 * Frames a menu error message has shown for.
 */
extern int MenuEtcErrCnt;

/**
 * Texture block the shadow and icon sheets load into.
 */
extern int MenuShadowReadBlock;

/**
 * Highest weapon-chain position available to each playable character.
 */
extern s16 MenuCharaWeaponMax[];

/**
 * Texture block the battle menu loads into.
 */
extern int BtlMenuReadBlock;

/**
 * Volume the battle menu drops the background music to.
 */
extern int BtlMenuBGMvol;

/**
 * Progress of the extra-menu model read. @see MenuExTextureRead.
 */
extern int MenuExTextureReadFlag;

/**
 * Returns the directory that menu textures load from.
 */
const char *GetMenuTextureDir();

/**
 * Returns the language the menus display in.
 */
int GetMenuLangFlag();

/**
 * Returns the path fragment of a language, or of Japanese for an unknown one.
 */
const char *GetNowSelectLanguage(int language);

/**
 * Writes the selected language's menu texture directory, NUL-terminated, into path.
 */
void GetPathReadDifferntLang(char *path);

/**
 * Loads a menu file from the selected language's directory, and returns its size or -1.
 */
int LoadFileBGMenuData(const char *name, u_long128 *buffer);

/**
 * Loads a menu file from the selected language's directory, and returns its size.
 */
int LoadFileMenuData(char *name, unsigned int *buffer);

/**
 * Returns the buffer the battle menu or the edit menu loads its files into.
 */
u_long128 *BtlMenuBufferSet(int mode);

/**
 * Rounds a buffer pointer up to the next 64-byte boundary.
 */
u_long128 *MenuCalcBufAlignment(u_long128 *buffer);

/**
 * Returns the upper village bound for Atla selection based on story progress.
 */
int GetAtoraMaxVillage();

/**
 * Converts a map number into the village the Atla menu shows, capped at the villages available.
 */
int GetNowMapTransAtraMap(int map_no);

/**
 * Sets the menu's projection and view matrices from a camera.
 */
void MenuWorldTrans(CCamera *camera);

/**
 * Draws a menu model through a callback under the menu's camera and lights.
 */
void MenuPolygonDraw(int ambient_alpha, void (*draw)());

/**
 * Projects a frame's world position to screen coordinates.
 */
void Get3DPosTo2DPos(CFrame *frame, int *screen);

/**
 * Returns the common menu font width of a style.
 */
int GetMenuCommonFontW(int language, int style);

/**
 * Centres a message window on a horizontal position.
 */
int GetMenuCommonPutXY(ClsMes *mes, int x);

/**
 * Sets up the common menu message windows.
 */
void InitMenuMesSet(int mode, short *messages);

/**
 * Steps and draws a message window at a position.
 */
void DrawMenuClsMes(ClsMes *message, int x, int y);

/**
 * Sound effects the menus play through ComMenuSePlay.
 */
enum MenuSound {
    MENU_SOUND_CURSOR = 0,  /**< Cursor or page moved. */
    MENU_SOUND_CONFIRM = 1, /**< Selection accepted. */
    MENU_SOUND_REFUSE = 2,  /**< Selection refused. */
};

/**
 * Plays a menu sound effect, unless the identifier is negative.
 */
void ComMenuSePlay(int sound);

/**
 * Draws a part of a menu texture with an alpha.
 */
void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, int alpha);

/**
 * Draws a part of a menu texture tinted by a colour.
 */
void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, unsigned char r, unsigned char g, unsigned char b, int alpha);

/**
 * Draws a part of a menu texture with a colour at each corner.
 */
void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right);

/**
 * Reloads one menu texture block through the texture manager.
 */
void MenuTextureReload(int block);

/**
 * Deletes the texture block a menu holds.
 */
void MenuTextureDelete(int *blocks);

/**
 * Fills the whole screen with a colour.
 */
void AllFillBoxForMenu(unsigned char r, unsigned char g, unsigned char b, unsigned char alpha);

/**
 * Darkens the whole screen behind a menu.
 */
void AllFadeForMenu(int alpha);

/**
 * Draws the menu frame image over the whole screen at the given tint and alpha.
 */
void FrameImageDraw(int brightness, int alpha);

/**
 * Draws a rectangle shaded between a colour at each corner.
 */
void DrawMenuColorGradation(CRect_i_ &rect, spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right);

/**
 * Draws a rectangle shaded between two colours from side to side.
 */
void DrawMenuSideGradation(CRect_i_ &rect, spRGBA *left, spRGBA *right);

/**
 * Draws an item's icon with its attachment count or weapon mark.
 */
void DrawIconParts(int item_no, int x, int y, int top, int bottom, int alpha, int number);

/**
 * Draws the attachment count or the weapon mark over one menu item slot.
 */
void DrawAttachNumberOrWeapon(int x, int y, int top, int bottom, int item_no, int value, int alpha, int vibe);

/**
 * Draws a named texture with its alpha fading from one side to the other.
 */
void FadeTexX(int left_x, int left_width, int right_x, int right_width, char *name, int dim);

/**
 * Looks up the icon sheet for an item and the icon's offset within it.
 */
CTexture *RetCTex(short item_no, int &u, int &v);

/**
 * Clips a texture strip to a range of screen positions.
 */
void MenuTextureClip(int &position, int &source, int &length, int minimum, int maximum);

/**
 * Counts the decimal digits of a number.
 */
int GetNumberKeta(int value);

/**
 * Returns the description of one menu icon in the language the menus display in.
 */
MENU_ICON_INFO *GetMenuIconInfo(int icon);

/**
 * Draws one of the main menu's icons.
 */
void DrawMainMenuIcon(int x, int y, int icon, int selected, int bright, int alpha);

/**
 * Draws a shaking item icon with its attachment count or weapon mark.
 */
void DrawMenuVibeItem(int x, int y, int offset_x, int offset_y, int alpha);

/**
 * Reads the selected language's offsets for the main menu's help window.
 */
int GetMainMenuRightHelpWinLangOffset(float &x, float &y, float &width, float &height);

/**
 * Reads the selected language's offsets for the main menu's help message.
 */
int GetMainMenuRightHelpMsgLangOffset(int &x, int &y);

/**
 * Empties an inventory record.
 */
void InitHaveData(IHAVEITEM *item);

/**
 * Empties a held-weapon record.
 */
void InitHaveWep(WEAPON_HAVE *weapon);

/**
 * Empties a held-attachment record.
 */
void InitHaveAttach(ATTACH_LIST *attachment);

/**
 * Exchanges two menu values, doing nothing if either pointer is null.
 */
void MenuDataSwap(s16 *first, s16 *second);

/**
 * Exchanges two menu values, doing nothing if either pointer is null.
 */
void MenuDataSwap(int *first, int *second);

/**
 * Exchanges two weapon records, doing nothing if either pointer is null.
 */
void MenuDataSwap(WEAPON_HAVE *first, WEAPON_HAVE *second);

/**
 * Exchanges two attachment records, doing nothing if either pointer is null.
 */
void MenuDataSwap(ATTACH_LIST *first, ATTACH_LIST *second);

/**
 * Marks which entries of an item pack the menu shows as disposable.
 */
void SetMenuTrushMark(ITEM_PACK *items);

/**
 * Clears the menu's disposable-item marks.
 */
void DeleteMenuTrushMark();

/**
 * Sets up a personal inventory board for a character.
 */
void InitPersonalBoardMode(CUserStatus *status, PERSONAL_BOARD *board, int mode, int page);

/**
 * Handles the key input that changes the personal board's mode.
 */
int BoardModeChangeKey();

/**
 * Keeps the personal board's cursor and scroll inside the list.
 */
void PersonalBoardLimmitCheck();

/**
 * Handles cursor movement on the personal board.
 */
int PersonalBoardKeySub();

/**
 * Handles key input on the personal board and returns what the cursor handler returned.
 */
int PersonalBoardKey();

/**
 * Swaps an inventory entry into a personal board slot.
 */
int PersonalBoardItemPush(IHAVEITEM *item, int board_pos);

/**
 * Returns the sort category containing an attachment identifier.
 */
int GetAttachKind(int item_no);

/**
 * Texture the menu's percentage bars are drawn from.
 */
extern CTexture *PerBoardTex;

} // namespace dc::game
