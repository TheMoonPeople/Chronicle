#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"
#include "itemdata.hpp"
#include "menu_draw.hpp"
#include "rect.hpp"

namespace dc::game {

/**
 * Where the battle menu was opened from, as BtlMenuMode holds it.
 */
// clang-format off
enum BattleMenuMode {
    BATTLE_MENU_MODE_DUNGEON = 0, /**< Opened from the dungeon game loop. */
    BATTLE_MENU_MODE_TOWN    = 1, /**< Opened from a town. */
};

// clang-format on

/**
 * What the battle menu is doing, as BattleMenuFlag holds it; a page's opening state is the page plus BTLMENU_STATE_OPEN_OFFSET and its closing state the page plus BTLMENU_STATE_CLOSE_OFFSET.
 */
// clang-format off
enum BattleMenuState {
    BTLMENU_STATE_APPEAR       = -1, /**< Menu bar sliding in. */
    BTLMENU_STATE_MAIN         = 0,  /**< Choosing a page from the menu bar. */
    BTLMENU_STATE_ITEM         = 1,  /**< Item page running. */
    BTLMENU_STATE_WEAPON       = 2,  /**< Weapon page running. */
    BTLMENU_STATE_CHARA        = 3,  /**< Character page running. */
    BTLMENU_STATE_ATLA         = 4,  /**< Atla page running. */
    BTLMENU_STATE_MOVE         = 5,  /**< Travel page running. */
    BTLMENU_STATE_OPTION       = 6,  /**< Option page running. */
    BTLMENU_STATE_SAVE         = 7,  /**< Save page running. */
    BTLMENU_STATE_MANUAL       = 8,  /**< Manual page running. */
    BTLMENU_STATE_ITEM_OPEN    = 9,  /**< Item page opening. */
    BTLMENU_STATE_WEAPON_OPEN  = 10, /**< Weapon page opening. */
    BTLMENU_STATE_CHARA_OPEN   = 11, /**< Character page opening. */
    BTLMENU_STATE_ATLA_OPEN    = 12, /**< Atla page opening. */
    BTLMENU_STATE_MOVE_OPEN    = 13, /**< Travel page opening. */
    BTLMENU_STATE_OPTION_OPEN  = 14, /**< Option page opening. */
    BTLMENU_STATE_SAVE_OPEN    = 15, /**< Save page opening. */
    BTLMENU_STATE_MANUAL_OPEN  = 16, /**< Manual page opening. */
    BTLMENU_STATE_ITEM_CLOSE   = 17, /**< Item page closing. */
    BTLMENU_STATE_WEAPON_CLOSE = 18, /**< Weapon page closing. */
    BTLMENU_STATE_CHARA_CLOSE  = 19, /**< Character page closing. */
    BTLMENU_STATE_ATLA_CLOSE   = 20, /**< Atla page closing. */
    BTLMENU_STATE_MOVE_CLOSE   = 21, /**< Travel page closing. */
    BTLMENU_STATE_OPTION_CLOSE = 22, /**< Option page closing. */
    BTLMENU_STATE_SAVE_CLOSE   = 23, /**< Save page closing. */
    BTLMENU_STATE_MANUAL_CLOSE = 24, /**< Manual page closing. */
    BTLMENU_STATE_ITEM_TRUSH   = 25, /**< Item page's trash list open. */
    BTLMENU_STATE_UNK_1A       = 26, /**< Handled with the character page; never set. */
    BTLMENU_STATE_EXIT         = 28, /**< Menu closing. */
    BTLMENU_STATE_OPEN_OFFSET  = 8,  /**< Added to a page to give its opening state. */
    BTLMENU_STATE_CLOSE_OFFSET = 16, /**< Added to a page to give its closing state. */
};

// clang-format on

/**
 * Pages of the battle menu bar, as MenuSelect[0] holds them.
 */
// clang-format off
enum BtlMenuPage {
    BTLMENU_PAGE_NONE   = 0, /**< No page. */
    BTLMENU_PAGE_ITEM   = 1, /**< Items. */
    BTLMENU_PAGE_WEAPON = 2, /**< Weapons. */
    BTLMENU_PAGE_CHARA  = 3, /**< Party. */
    BTLMENU_PAGE_ATLA   = 4, /**< Atla placement. */
    BTLMENU_PAGE_MOVE   = 5, /**< Travel. */
    BTLMENU_PAGE_OPTION = 6, /**< Options. */
    BTLMENU_PAGE_SAVE   = 7, /**< Save. */
    BTLMENU_PAGE_MANUAL = 8, /**< Manual. */
};

// clang-format on

/**
 * Which way the menu bar's icons are moving, as BtlEffectFlag holds it.
 */
// clang-format off
enum BtlEffect {
    BTLEFFECT_NONE       = -1, /**< Icons still. */
    BTLEFFECT_PAGE_CLOSE = 0,  /**< Icons returning as a page closes. */
    BTLEFFECT_PAGE_OPEN  = 1,  /**< Icons sliding away as a page opens. */
};

// clang-format on

/**
 * Steps of the character page, as MENU_CHARA_INFO::state holds them.
 */
// clang-format off
enum MenuCharaState {
    MENU_CHARA_SELECT      = 0, /**< Choosing a character. */
    MENU_CHARA_OPEN        = 1, /**< Faces spiralling in. */
    MENU_CHARA_CLOSE       = 2, /**< Sliding out. */
    MENU_CHARA_TURN        = 3, /**< Ring turning to the next character. */
    MENU_CHARA_CHANGE_LOAD = 4, /**< Loading the new leader. */
    MENU_CHARA_CHANGE_EXIT = 5, /**< Leaving the menu with the new leader. */
    MENU_CHARA_MESSAGE     = 6, /**< Message window shown; never set. */
};

// clang-format on

/**
 * Pages of the weapon tag board, as WEP_MENU_INFO::tag_page holds them.
 */
// clang-format off
enum WepTagPage {
    WEP_TAG_STATUS     = 0, /**< Weapon status. */
    WEP_TAG_ELEMENT    = 1, /**< Element values. */
    WEP_TAG_VS_MONSTER = 2, /**< Damage against each monster family. */
};

// clang-format on

/**
 * Screens of the weapon page, as WEP_MENU_INFO::mode holds them.
 */
// clang-format off
enum WepMenuMode {
    WEP_MENU_LIST                 = 0,  /**< Weapon list. */
    WEP_MENU_ACTION               = 1,  /**< Action rows for the chosen weapon. */
    WEP_MENU_ELEMENT              = 2,  /**< Element selection. */
    WEP_MENU_LEVELUP_CONFIRM      = 3,  /**< Level-up yes or no. */
    WEP_MENU_STATUS_BREAK_CONFIRM = 4,  /**< Status break yes or no. */
    WEP_MENU_BUILDUP_SELECT       = 5,  /**< Build-up target list. */
    WEP_MENU_BUILDUP_CONFIRM      = 6,  /**< Build-up yes or no. */
    WEP_MENU_BUILDUP_PREVIEW      = 7,  /**< Build-up target's tag board. */
    WEP_MENU_ATTACH_WEAPON        = 8,  /**< Attachment screen, on the weapon. */
    WEP_MENU_ATTACH_SOCKETS       = 9,  /**< Attachment screen, on the weapon's sockets. */
    WEP_MENU_ATTACH_BOARD         = 10, /**< Attachment screen, on the attachment board. */
    WEP_MENU_ATTACH_TAGS          = 11, /**< Attachment screen, on the tag board. */
};

// clang-format on

/**
 * What the weapon page is doing, as WEP_MENU_INFO::state holds it.
 */
// clang-format off
enum WepMenuState {
    WEP_STATE_IDLE                = 0,  /**< Taking input. */
    WEP_STATE_OPEN                = 1,  /**< Sliding in. */
    WEP_STATE_CLOSE               = 2,  /**< Sliding out. */
    WEP_STATE_UNK_3               = 3,  /**< Drawn but never set. */
    WEP_STATE_CHARA_CHANGE        = 4,  /**< Switching to another character's weapons. */
    WEP_STATE_EQUIP               = 5,  /**< Equipping the chosen weapon. */
    WEP_STATE_LEVELUP_EFFECT      = 7,  /**< Level-up effect playing. */
    WEP_STATE_STATUS_BREAK_EFFECT = 8,  /**< Status break effect playing. */
    WEP_STATE_BUILDUP_EFFECT      = 9,  /**< Build-up effect playing. */
    WEP_STATE_REPAIR_EFFECT       = 10, /**< Repair effect playing. */
    WEP_STATE_WARNING             = 11, /**< Warning message shown. */
    WEP_STATE_SCRAP_PROMPT        = 12, /**< Asking whether to scrap the weapon. */
    WEP_STATE_SCRAP_CONFIRM       = 13, /**< Confirming the scrap. */
    WEP_STATE_SCRAP_REFUSED       = 14, /**< Refusing to scrap the weapon. */
};

// clang-format on

/**
 * How the weapon page was opened, as WEP_MENU_INFO::open_mode holds it.
 */
// clang-format off
enum WepMenuOpenMode {
    WEP_OPEN_FROM_BAR  = 0, /**< From the menu bar, sliding in. */
    WEP_OPEN_IN_PLACE  = 1, /**< Already in place, without sliding in. */
    WEP_OPEN_VIEW_ONLY = 2, /**< For viewing; circle and cross do nothing. */
};

// clang-format on

/**
 * Rows of the weapon page's action list.
 */
// clang-format off
enum WepActionRow {
    WEP_ACTION_EQUIP        = 0, /**< Equip. */
    WEP_ACTION_ATTACH       = 1, /**< Attach. */
    WEP_ACTION_ELEMENT      = 2, /**< Choose an element. */
    WEP_ACTION_REPAIR       = 3, /**< Repair with Repair Powder. */
    WEP_ACTION_LEVELUP      = 4, /**< Level up. */
    WEP_ACTION_STATUS_BREAK = 5, /**< Break down into attachments. */
    WEP_ACTION_BUILDUP      = 6, /**< Build up. */
};

// clang-format on

/**
 * Rows of the weapon status tag that the debug editor changes.
 */
// clang-format off
enum WepStatusTagRow {
    WEP_TAG_ROW_DURABILITY = 0, /**< Durability. */
    WEP_TAG_ROW_EXP        = 1, /**< Experience. */
    WEP_TAG_ROW_ATTACK     = 2, /**< Attack. */
    WEP_TAG_ROW_ENDURANCE  = 3, /**< Endurance. */
    WEP_TAG_ROW_SPEED      = 4, /**< Speed. */
    WEP_TAG_ROW_MAGIC      = 5, /**< Magic. */
};

// clang-format on

/**
 * Progress of the menu's model read, as MenuExTextureReadFlag holds it.
 */
// clang-format off
enum MenuExTextureRead {
    MENU_EX_TEX_UNREAD   = 0, /**< Waiting for the read. */
    MENU_EX_TEX_BUILDING = 1, /**< Building the model. */
    MENU_EX_TEX_READY    = 2, /**< Ready to draw. */
};

// clang-format on

/**
 * What the item page is doing, as ITEM_MENU_MODE_INFO::state holds it.
 */
// clang-format off
enum ItemMenuState {
    ITEM_MENU_STATE_IDLE            = 0, /**< Taking input. */
    ITEM_MENU_STATE_LOADING         = 1, /**< Waiting for the weapon icons. */
    ITEM_MENU_STATE_OPEN            = 2, /**< Sliding in. */
    ITEM_MENU_STATE_CLOSE           = 3, /**< Sliding out. */
    ITEM_MENU_STATE_EFFECT          = 4, /**< Item effect playing on a weapon. */
    ITEM_MENU_STATE_DEBUG_ITEMS     = 5, /**< Debug item list. */
    ITEM_MENU_STATE_POWERUP_CONFIRM = 6, /**< Asking whether to power up a weapon. */
};

// clang-format on

/**
 * Parts of the item page the cursor can be on, as ITEM_MENU_MODE_INFO::mode holds them.
 */
// clang-format off
enum ItemMenuArea {
    ITEM_MENU_QUICK_SLOTS = 0, /**< Quick-use slots. */
    ITEM_MENU_CHARA       = 1, /**< Character panel. */
    ITEM_MENU_WEAPON      = 2, /**< Equipped weapon panel. */
    ITEM_MENU_DEFENSE     = 3, /**< Defense panel. */
    ITEM_MENU_BOARD       = 4, /**< Personal board. */
    ITEM_MENU_TRUSH       = 5, /**< Trash list. */
};

// clang-format on

/**
 * How the player leaves a dungeon from the travel page.
 */
// clang-format off
enum EscapeDngMode {
    ESCAPE_DNG_PAY    = 0, /**< Pays half the money. */
    ESCAPE_DNG_POWDER = 1, /**< Uses Escape Powder. */
    ESCAPE_DNG_FREE   = 2, /**< Free, because no monsters remain. */
};

// clang-format on

/**
 * What the travel page is doing, as MENU_MOVE_INFO::state holds it.
 */
// clang-format off
enum MenuMoveState {
    MENU_MOVE_STATE_SELECT        = 0, /**< Choosing a place. */
    MENU_MOVE_STATE_OPEN          = 1, /**< Opening. */
    MENU_MOVE_STATE_CLOSE         = 2, /**< Closing. */
    MENU_MOVE_STATE_ZOOM_IN       = 3, /**< Moving the map toward the chosen place. */
    MENU_MOVE_STATE_CONFIRM       = 4, /**< Asking whether to go there. */
    MENU_MOVE_STATE_ZOOM_OUT      = 6, /**< Moving the map back out. */
    MENU_MOVE_STATE_INTERIOR_FADE = 7, /**< Fading out to leave an interior. */
};

// clang-format on

/**
 * Places on the world map, indexing TownOrDngPos and MenuGrobalMapNoTbl.
 */
// clang-format off
enum WorldMapPlace {
    WORLD_PLACE_NORUNE            = 0,  /**< Norune Village. */
    WORLD_PLACE_DIVINE_BEAST_CAVE = 1,  /**< Divine Beast Cave. */
    WORLD_PLACE_MATATAKI          = 2,  /**< Matataki Village. */
    WORLD_PLACE_WISE_OWL_FOREST   = 3,  /**< Wise Owl Forest. */
    WORLD_PLACE_FIELD_S04         = 4,  /**< Field map s04. */
    WORLD_PLACE_QUEENS            = 5,  /**< Queens. */
    WORLD_PLACE_SHIPWRECK         = 6,  /**< Shipwreck. */
    WORLD_PLACE_MUSKA_LACKA       = 7,  /**< Muska Lacka. */
    WORLD_PLACE_FIELD_S32         = 8,  /**< Field map s32. */
    WORLD_PLACE_SUN_MOON_TEMPLE   = 9,  /**< Sun and Moon Temple. */
    WORLD_PLACE_FIELD_S13         = 10, /**< Field map s13. */
    WORLD_PLACE_YELLOW_DROPS      = 11, /**< Yellow Drops. */
    WORLD_PLACE_MOON_SEA          = 12, /**< Moon Sea. */
    WORLD_PLACE_FIELD_S30         = 13, /**< Field map s30. */
    WORLD_PLACE_GALLERY_OF_TIME   = 14, /**< Gallery of Time. */
    WORLD_PLACE_DEMON_SHAFT       = 15, /**< Demon Shaft. */
    WORLD_PLACE_COUNT             = 16, /**< Number of places. */
};

// clang-format on

/**
 * What the menu's option-icon window shows, as MenuClsMes::mode holds it.
 */
// clang-format off
enum MenuClsMesMode {
    MENU_CLSMES_OFF     = 0, /**< Nothing. */
    MENU_CLSMES_OPTIONS = 1, /**< The selected weapon's options. */
    MENU_CLSMES_UNK_2   = 2, /**< Handled as MENU_CLSMES_OPTIONS; never set. */
};

// clang-format on

class CCharacter;
class CMenuIconAutoGet;
class CSaveData;
class CWeaponLevelUp;

/**
 * Holds a gradient's four corner colours.
 */
struct GRADATION_COLOR_INFO2 {
    spRGBA colors[4]; /**< Corner colours, in the order DrawMenuColorGradation takes them. */
};

STATIC_ASSERT(sizeof(GRADATION_COLOR_INFO2) == 0x10);

/**
 * Tracks which character and weapon the weapon menu's cursor is on.
 */
struct WEP_MENU_INFO {
    s16            open_mode;       /**< How the weapon page was opened. @see WepMenuOpenMode. */
    s16            mode;            /**< Area of the weapon page the cursor is in. @see WepMenuMode. */
    s8             weapon_slot;     /**< Weapon slot the cursor is on, within the selected character's chara_weapons row. */
    s8             chara;           /**< Party member index the weapon menu is showing. */
    s8             sockets_enabled; /**< Nonzero while moving up from the tags or the pack leads to the weapon's sockets. */
    char           element;         /**< Element the cursor is on in the element dialog. */
    s8             warning_no;      /**< Warning message the weapon page shows in its warning state, or -1 for none. */
    char           confirm[3];      /**< Cursor on the scrap confirmation dialog in its first byte, zero for yes. */
    s16            state;           /**< Transition, effect or dialog the weapon page is in. @see WepMenuState. */
    char           unk_0E[2];
    s32            counter; /**< Frames spent in the current state. */
    PERSONAL_BOARD board;   /**< Personal board the weapon menu lists the party member's weapons on. */
    char           unk_174[4];
    s8             tag_page; /**< Tag page shown. @see WepTagPage. */
    s8             tag_row;  /**< Row the cursor is on within the tag page. */
    char           unk_17A[2];
};

STATIC_ASSERT(sizeof(WEP_MENU_INFO) == 0x17C);

/**
 * Tracks what a message on the item menu is currently about.
 */
struct ITEM_MENU_MODE_INFO {
    s16            mode;  /**< Area of the item page the cursor is in. @see ItemMenuArea. */
    s16            chara; /**< Party member index the item page is showing. */
    s16            unk_04;
    s16            use_target;        /**< Kind of target the item waiting for confirmation is used on. */
    s16            confirm;           /**< Cursor on the use confirmation dialog, zero for yes. */
    s16            overflow_pages;    /**< Bit per board page that holds items the pack could not take. */
    s16            overflow;          /**< Nonzero while the pack is holding items it could not take. */
    s16            overflow_count[3]; /**< Number of items each board page could not take. */
    WEAPON_HAVE   *target_weapon;     /**< Weapon an item waiting for confirmation is used on. */
    s16            state;             /**< Transition or dialog the item page is in. @see ItemMenuState. */
    char           unk_1A[2];
    s32            counter; /**< Frames spent in the current state. */
    PERSONAL_BOARD board;   /**< Personal board the item page lists the pack on. */
    char           unk_180[4];
    s16            message_item_no; /**< Item number the last SetNowEquipWeaponDataForMsg call named. */
    s16            message_slot;    /**< Slot number the last SetNowEquipWeaponDataForMsg call named. */
};

STATIC_ASSERT(sizeof(ITEM_MENU_MODE_INFO) == 0x188);

/**
 * Holds the battle menu's character page state.
 */
struct MENU_CHARA_INFO {
    s16   chara;          /**< Party member at the front of the turntable. */
    s8    slot_count;     /**< Number of places on the turntable. */
    s8    state;          /**< Transition, turn or party change the character page is in. @see MenuCharaState. */
    float timer;          /**< Frames spent in the current transition or turn. */
    s32   change_counter; /**< Frames since the change to another party member began. */
};

STATIC_ASSERT(sizeof(MENU_CHARA_INFO) == 0xC);

/**
 * Holds one party member's place on the character page's turntable.
 */
struct SYS_CHARA_INFO {
    s8    chara; /**< Party member this place shows. */
    s8    place; /**< Place on the turntable counted from the front, zero at the front. */
    char  unk_02[2];
    float x; /**< Horizontal position of the party member's face. */
    float y; /**< Vertical position of the party member's face. */
};

STATIC_ASSERT(sizeof(SYS_CHARA_INFO) == 0xC);

/**
 * Holds the travel page's state.
 */
struct MENU_MOVE_INFO {
    s32  mode;        /**< Where the travel page was opened from. @see MenuMoveMode. */
    s32  cursor;      /**< Place or yes/no answer the cursor is on. */
    s16  load_map;    /**< World map region whose model is loaded. */
    s16  last_place;  /**< Highest world-map place number the travel page offers. */
    s16  start_place; /**< Place the party stood on when the page opened. */
    s16  ready;       /**< Nonzero once the world map model has loaded. */
    char unk_10[4];
    s32  tex_block; /**< Texture block the world map loads into. */
    s16  state;     /**< Transition or step the travel page is in. @see MenuMoveState. */
    char unk_1A[2];
    s32  counter; /**< Frames spent in the current state. */
};

STATIC_ASSERT(sizeof(MENU_MOVE_INFO) == 0x20);

/**
 * Names one world-map place and where it sits on the map.
 */
struct WORLD_MAP_POS {
    char frame[6]; /**< Name of the map frame the place's marker hangs off. */
    s16  visited;  /**< One once the party has visited the place, -1 before. */
    s32  x;        /**< Horizontal position of the place on the map. */
    s32  y;        /**< Vertical position of the place on the map. */
};

STATIC_ASSERT(sizeof(WORLD_MAP_POS) == 0x10);

/**
 * Ranks one world-map destination by how well it lines up with a pushed direction.
 */
struct MAP_JUMP_COMPARE {
    s8    index;     /**< World-map place this entry ranks. */
    s8    reachable; /**< One when the place has been visited and is not the current one. */
    char  unk_02[2];
    float alignment;   /**< Cosine between the pushed direction and the direction to the place; entries sort on it, largest first. */
    s32   dx;          /**< Horizontal offset from the current place to this one. */
    s32   dy;          /**< Vertical offset from the current place to this one. */
    s32   distance_sq; /**< Squared distance from the current place to this one. */
};

STATIC_ASSERT(sizeof(MAP_JUMP_COMPARE) == 0x14);

/**
 * Holds the screen position of one icon on the battle menu ring.
 */
struct MENU_ICON_POS {
    float x; /**< Horizontal screen position of the icon. */
    float y; /**< Vertical screen position of the icon. */
};

STATIC_ASSERT(sizeof(MENU_ICON_POS) == 0x8);

/**
 * Places one party member's weapon model on the weapon page.
 */
struct MENU_WEP_POLY_OFFSET {
    float position[3]; /**< Offset of the model from the page's model origin. */
    float scale;       /**< Scale the model is drawn at. */
    float rotation[3]; /**< Rotation the model is drawn with. */
    float unk_1C[3];
};

STATIC_ASSERT(sizeof(MENU_WEP_POLY_OFFSET) == 0x28);

/**
 * Gives the weapon a party member starts with.
 */
int GetDefaultWeaponNo(int character_no);

/**
 * Reports which party member a weapon is the starting weapon of, or -1 for none.
 */
int IsDefaultWeapon(int weapon_no);

/**
 * Remembers the weapon and slot a message is about to name.
 */
void SetNowEquipWeaponDataForMsg(int item_no, int slot);

/**
 * Gives the item number and slot the last message was told about.
 */
void GetNowEquipWeaponDataForMsg(int &item_no, int &slot);

/**
 * Gives one of the menu's gradient colour pairs.
 */
GRADATION_COLOR_INFO2 *GetGradationColorInfo2(int index);

/**
 * Sets whether the party is escaping the dungeon.
 */
void SetEscapeDngFlag(int flag);

/**
 * Sets whether the party is leaving an interior area.
 */
void SetInteriorOutFlag(int flag);

/**
 * Returns the flag that says the party is leaving an interior area.
 */
int GetInteriorOutFlag();

/**
 * Chooses the message that explains why the party may not leave the zone.
 */
int GetLimmitMsg();

/**
 * Draws the menu's main page: the bar, the selected icon's title and its help text.
 */
void DrawBattleMain();

/**
 * Draws one unselected party member's face, life and magic bars.
 */
void DrawOtherCharaStatus(int x, int y, int chara, int alpha);

/**
 * Draws one party member's dungeon status panel with their weapon and condition.
 */
void DngComStatus(int x, int y, int chara, int alpha);

/**
 * Draws the selected party member's full status panel.
 */
void DrawSelCharaStatus(float x, float y, int chara, int alpha, int face_size, int face_alpha, int face_x, int face_y);

/**
 * Loads the battle menu's texture blocks and message buffers.
 */
void BattleMenuTexEnter();

/**
 * Clears the menu's item list, frees its textures and restores the background music volume.
 */
void ExitBattleMenu(int unused);

/**
 * Takes the texture blocks the menu was given, sets the pad up and opens the chosen
 * mode.
 */
void BattleMenuInit(int *texture_blocks, int mode);

/**
 * Draws the whole menu for one frame, choosing the page from the menu state.
 */
void BattleMenuDraw();

/**
 * Runs one frame of the menu, stepping the party and dispatching on the menu state.
 */
int BattleMenuCursor();

/**
 * Moves the cursor along the menu bar and opens the page it settles on.
 */
int BattleMenuSelect();

/**
 * Slides the page in or out and reports when the movement has finished.
 */
int ToFromSelect(int out);

/**
 * Initializes the Battle Menu character selection page and turntable.
 */
void Btl_InitMenuChara(u_long128 *buffer);

/**
 * Handles input on the character page, including the turntable and the equipment list.
 */
int BattleMenuCharaKey();

/**
 * Draws the character page with its model, status and equipment.
 */
void DrawCharaSelect();

/**
 * Draws one weapon's attack and endurance bars with its experience.
 */
void DrawWepDamageDraw(RECT rect, WEAPON_HAVE *weapon, int alpha);

/**
 * Draws one weapon's status panel.
 */
void DrawWepStatus(int x, int y, WEAPON_HAVE *weapon, int selected, int alpha);

/**
 * Draws the bars comparing a weapon's current values against its built-up ones.
 */
void DrawWepVolumeDisplay(int x, int y, WEAPON_HAVE *weapon, int alpha);

/**
 * Returns the x position that centers a weapon name of a width.
 */
s32 GetWeaponNamePutX(int center_x, int width);

/**
 * Draws the gradient frame a weapon-status row sits in.
 */
void DrawWeaponStatusWaku(int x, int y, int width, int limit);

/**
 * Draws the left and right cursor arrows, bobbing them with a sine.
 */
void DrawBtlMenuLRCursor(int x, int y, int width, int alpha);

/**
 * Draws a weapon's status tags with their volume bars.
 */
void DrawWeaponStatusTag(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha);

/**
 * Draws a weapon's elemental tags with their volume bars.
 */
void DrawWeaponElemTag(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha);

/**
 * Draws one weapon's anti-monster tags with their volume bars.
 */
void DrawWeaponVsMonster(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha);

/**
 * Draws one weapon's model, name and every panel that describes it.
 */
void DrawAallWeapon(int x, int y, float depth, CCharacter *model, WEAPON_HAVE *weapon, int equipped, int selected, int alpha);

/**
 * Draws the weapon page for one party member, sliding between their weapons.
 */
void BtlWeaponDraw(int x, float depth, int chara, int alpha);

/**
 * Draws the dialog that offers to repair, build up or scrap the selected weapon.
 */
void DrawWeaponSelectDialog(int x, int y, int alpha);

/**
 * Initializes the Battle Menu weapon selection page for the given character.
 */
void Btl_InitWeaponSelect(int mode, int chara);

/**
 * Runs one frame of the weapon page, dispatching on which of its modes is open.
 */
void WeaponMenuSelect();

/**
 * Moves the cursor between the three attachment pages.
 */
void WeaponMenuAttachModeKey();

/**
 * Moves the cursor down the tags of the open attachment page.
 */
void WeaponMenuTagKey();

/**
 * Draws the repair and build-up dialog with the counts of the items it needs.
 */
void RepairAndLevelUpDraw(int x, int y, int alpha);

/**
 * Draws the list of weapons the selected one may be built up into.
 */
void DrawBuildUpWeaponSelect(int x, int y, int cursor);

/**
 * Draws the weapon page and whichever dialog is open over it.
 */
void WeaponMenuDraw();

/**
 * Draws the list of items that may be thrown away.
 */
void DrawTrushItem();

/**
 * Waits for the item page's weapon icons and enters them into the texture manager.
 */
void ReadSyncItemMenuWepIcon();

/**
 * Initializes the Battle Menu item inventory page for the given character.
 */
void Btl_InitItemMode(int mode, int chara);

/**
 * Gives the item page's textures back and re-equips anyone left without a weapon.
 */
void ExistItemMenu();

/**
 * Runs one frame of the item page, dispatching on which of its modes is open.
 */
int ItemMenuMainKey();

/**
 * Draws the item page for whichever of its modes is open.
 */
void ItemMenuModeDraw();

/**
 * Moves the cursor across the item list and wraps it at the ends.
 */
int ItemMenuModeKey();

/**
 * Draws the party member's active item slots and what stands in them.
 */
void ActiveItemDraw(int x, int y, int alpha);

/**
 * Draws the item page's status panel for one party member.
 */
void ItemMenuCharaStatusDraw(int x, int y, int chara, int alpha);

/**
 * Chooses which of the use, equip and throw prompts an item offers.
 */
void ItemNaviCursor(int item_no);

/**
 * Draws the party member's status ailments, bobbing the icons with a sine.
 */
void CharaStatusMsgDraw(int x, int y, int chara, int per_member, int alpha);

/**
 * Opens the travel page, either on the world map or on the local one.
 */
void InitMenuMove(int mode, int texture_block, u_long128 *buffer);

/**
 * Updates the screen positions and visit markers of world-map destinations.
 */
void GetTownOrDngPos();

/**
 * Handles input on the travel page and reports the destination that was chosen.
 */
int MenuMoveKey();

/**
 * Draws the travel page with its map, its names and its confirmation plate.
 */
void DrawMenuMove();

/**
 * Draws the escape item dialog with its count, its money board and its help window.
 */
void DrawEscapeItem(int x, int y, int alpha);

/**
 * Waits for the world map and enters its textures and model.
 */
int LoadWorldMap();

/**
 * Exchanges two world-map destination rankings.
 *
 * Unlike the menu_draw.cpp overloads of this name, this one does not check
 * either pointer: its only caller passes addresses of stack entries.
 */
void MenuDataSwap(MAP_JUMP_COMPARE *first, MAP_JUMP_COMPARE *second);

/**
 * Finds the reachable world-map place nearest the current one in the direction the pad was
 * pushed, or -1 when there is none.
 */
int GetNearWorldPos(int direction, int *position);

/**
 * Draws the plate that asks whether to travel to the chosen place.
 */
void DrawMapCheck(int alpha);

/**
 * Initializes the battle manual menu from its placement data and load buffer.
 */
void Btl_BattleManualInit(int *result, u_long128 *load_buffer);

/**
 * Processes battle-manual input and returns zero after updating transitions.
 */
int BattleManualKey();

/**
 * Draws the sockets of a weapon and what is fitted into them.
 */
void DrawWepHole(int x, int y, WEAPON_HAVE *weapon, int selected, int alpha);

/**
 * Displays the selected weapon's option messages in the battle menu.
 */
class MenuClsMes {
public:
    s8           mode; /**< Selects the weapon option display mode. */
    char         unk_01;
    s16          alpha;        /**< Opacity used for the option icons and message window. */
    s16          option_count; /**< Number of option messages selected for the weapon. */
    s16          option_start; /**< First option line the window starts from, reset with the option list. */
    s32          draw_x;       /**< Horizontal screen position the option messages are drawn at. */
    s32          draw_y;       /**< Vertical screen position the option messages are drawn at. */
    s32          option_flags; /**< Combined option bits of the weapon and its attachments. */
    WEAPON_HAVE *weapon;       /**< Weapon whose option messages are displayed. */
    void        *work_area;    /**< Work area the message window builds its text in. */
    ClsMes      *message;      /**< Message window that holds the option text. */
    /**
     * Selects the shared East King message window for weapon option text.
     */
    void InitMes();

    /**
     * Resets the weapon option display state and position.
     */
    void InitData();

    /**
     * Presets the shared message window and points it at a message buffer.
     */
    void SetBuffInfo(short *buffer);

    /**
     * Updates option messages from the weapon and its attachments.
     */
    void NowWeaponStatus(WEAPON_HAVE *selected_weapon);

    /**
     * Updates the active weapon option messages and advances their window.
     */
    void Step();

    /**
     * Draws the message window's single line over the menu.
     */
    void Draw1(int x, int y, int draw_alpha);
};

STATIC_ASSERT(sizeof(MenuClsMes) == 0x20);

/**
 * Draws the selected weapon's option messages.
 */
extern MenuClsMes MenuMes;

/**
 * Cache the weapon menu's level-up, build-up and repair effects are built in.
 */
extern CDataAlloc2<1> MenuExCashBuffer;

/**
 * Stores the screen positions of the battle menu ring's eight icons.
 */
extern MENU_ICON_POS NorMenuIcon[8];

/**
 * Holds the menu cursor's screen position.
 */
extern float SysCur[3];

/**
 * Holds the character page's state.
 */
extern MENU_CHARA_INFO MenuChara;

/**
 * Character the menus draw a party member or map marker with.
 */
extern CCharacter MenuCharaFrame;

/**
 * Tracks the weapon menu's selected character and weapon slot.
 */
extern WEP_MENU_INFO WepMenu;

/**
 * Tracks what the item menu's message is currently about.
 */
extern ITEM_MENU_MODE_INFO ItemMenuMode;

/**
 * Icons flying from the field into the item pack.
 */
extern CMenuIconAutoGet IconAutoGet;

/**
 * Holds the travel page's state.
 */
extern MENU_MOVE_INFO MenuMove;

/**
 * Holds the source rectangle of the menu's digit sprites.
 */
extern RECT NumberSprite[2];

/**
 * Icon numbers the menu bar shows for each menu mode.
 */
extern s16 BtlDrawTbl[2][8];

/**
 * Holds each party member's place on the character page's turntable.
 */
extern SYS_CHARA_INFO SysChara[6];

/**
 * Holds each party member's starting weapon.
 */
extern s16 MenuDefaultWeaponNo[7];

/**
 * Highest weapon-chain position available to each playable character.
 */
extern s16 MenuCharaWeaponMax[6];

/**
 * Places each party member's weapon model on the weapon page.
 */
extern MENU_WEP_POLY_OFFSET MenuWepPolyOffset[6];

/**
 * Holds the menu's gradient colour pairs.
 */
extern GRADATION_COLOR_INFO2 MenuColorInfo2[26];

/**
 * Source rectangle of the digits a weapon-status bar prints its value with.
 */
extern RECT WeaponVolumeNumberRect;

/**
 * Map number of each world-map place; dungeons are numbered from 200.
 */
extern s16 MenuGrobalMapNoTbl[16];

/**
 * Holds each world-map place's frame name and position.
 */
extern WORLD_MAP_POS TownOrDngPos[16];

/**
 * Holds the world-map marker's position.
 */
extern float mapo[4];

/**
 * Holds the step the world-map marker moves by each frame.
 */
extern float mapmovev[4];

/**
 * Language the battle menu's text is drawn in.
 */
extern s16 BtlMenuNowLang;

/**
 * Stores the active battle-menu state. @see BattleMenuState.
 */
extern s32 BattleMenuFlag;

/**
 * Stores the character page's turntable radius.
 */
extern float chara_r_long;

/**
 * Number of rows each item-board page scrolls through when items are thrown away.
 */
extern s16 TrushMoveMax[3];

/**
 * Stores the battle-menu transition phase. @see BtlEffect.
 */
extern s32 BtlEffectFlag;

/**
 * Whether the travel page may jump to another map, or -1 while it cannot.
 */
extern s32 MenuMapJumpMode;

/**
 * Save data the travel page reads visits and dungeon progress from.
 */
extern CSaveData *BtlMenuSaveDataPt;

/**
 * Stores the dungeon status used by the battle menu.
 */
extern CDngStatusData *BtlMenuStatusPt;

/**
 * Where the battle menu was opened from. @see BattleMenuMode.
 */
extern s32 BtlMenuMode;

/**
 * Volume the battle menu drops the background music to.
 */
extern s32 BtlMenuBGMvol;

/**
 * Texture block the battle menu loads into.
 */
extern s32 BtlMenuReadBlock;

/**
 * Buffer the battle menu reads its files into.
 */
extern u_long128 *BtlMenuReadBuf;

/**
 * Is nonzero once the battle menu's textures have finished loading.
 */
extern s32 BtlMenuReadEndFlag;

/**
 * Texture block holding the item menu's weapon icons.
 */
extern s32 ItemMenuWeaponIconReadBlock;

/**
 * Is set once the item page's weapon icons have been entered.
 */
extern s32 ItemMenuAlreadyReadWepIconTexFlag;

/**
 * Texture block the shadow and icon sheets load into.
 */
extern s32 MenuShadowReadBlock;

/**
 * Texture block the second battle menu image loads into.
 */
extern s32 BtlMenuExtBlock;

/**
 * Texture block the character page's fonts and faces load into.
 */
extern s32 BtlMenuExReadBlock;

/**
 * Is set once the character page's extra data has been read.
 */
extern s32 BtlMenuExReadFlag;

/**
 * Texture block the menu's pages read their extra textures into.
 */
extern s32 MenuExtendReadBlock;

/**
 * Progress of the extra-menu model read. @see MenuExTextureRead.
 */
extern s32 MenuExTextureReadFlag;

/**
 * Buffer the item page reads the party member's model into.
 */
extern u_long128 *ItemMenuCharaReadBuf;

/**
 * Counts the frames the weapon page spends building a weapon model.
 */
extern s32 BtlMDSBuildCnt;

/**
 * Texture block the battle manual loads into.
 */
extern s32 BtlMenuManualReadBlock;

/**
 * Points to the weapons of the party member the weapon menu shows.
 */
extern WEAPON_HAVE *DngWepHavePt;

/**
 * Buffer the weapon menu's level-up, build-up and repair effects are read into.
 */
extern CWeaponLevelUp *WepMenuEffectReadBuf;

/**
 * Texture the character page's status plate is drawn from.
 */
extern CTexture *CharaStatus;

/**
 * Texture the party members' faces are drawn from.
 */
extern CTexture *MenuCharaFace;

/**
 * Texture drawn in place of the face of a party member not yet met.
 */
extern CTexture *NonCharaFace;

/**
 * Texture the dungeon status plates are drawn from.
 */
extern CTexture *BtStatus;

/**
 * Texture the weapon status panels are drawn from.
 */
extern CTexture *WepStatus;

/**
 * Texture holding the alphabet font the character page writes with.
 */
extern CTexture *BtlAlpha;

/**
 * Texture holding the hiragana font the character page writes with.
 */
extern CTexture *BtlHira;

/**
 * Texture holding the katakana font the character page writes with.
 */
extern CTexture *BtlKata;

/**
 * Texture holding the accented European font the character page writes with.
 */
extern CTexture *BtlEuro;

/**
 * Screen position of the battle menu's help window.
 */
extern float HelpWinHead[2];

/**
 * Is nonzero while the menu's frames need rebuilding.
 */
extern u8 BtlWakuMake2;

/**
 * Opacity of the battle menu's help window.
 */
extern s32 BtlHelpWinAlpha;

/**
 * Width of the battle menu's help window.
 */
extern float BtlHelpWinW;

/**
 * Height of the battle menu's help window.
 */
extern float BtlHelpWinH;

/**
 * Stores the selected battle-menu page, then the icon highlighted on the menu ring. @see BtlMenuPage.
 */
extern s32 MenuSelect[2];

/**
 * Points to the item pack the battle menu shows.
 */
extern ITEM_PACK *MenuItemPackPt;

/**
 * Stores whether the party is escaping the dungeon.
 */
extern s16 EscapeDngFlg;

/**
 * Stores whether the party is leaving an interior area.
 */
extern s16 RoomOutFlag;

/**
 * Stores the character page's turntable movement.
 */
extern float MenuCharaMove;

/**
 * Angle between two neighbouring party members on the character page's turntable.
 */
extern float PosAngle;

/**
 * Points to the item the weapon menu is holding.
 */
extern IHAVEITEM *BtlHaveItemPt;

/**
 * Frame rate the weapon models turn at.
 */
extern float WepFrameRate;

/**
 * Horizontal position of the weapon list's model row.
 */
extern float WeaponPos;

/**
 * Horizontal position of the weapon list's polygons.
 */
extern float WepPolyPos;

/**
 * Stores the battle-menu transition timer.
 */
extern float BtlEffectCt;

/**
 * Is nonzero while the battle menu shows a warning message, which dims its icons.
 */
extern s32 MenuWarningMsgFlag;

/**
 * Buffer the character page reads the next party member's model into.
 */
extern u_long128 *BtlMenuCharaChangeBuf;

/**
 * Is nonzero while the weapon page ignores its selection keys.
 */
extern s32 WeaponMenuSelectKeyLockFlag;

/**
 * Points to the frame name of the world-map place the cursor moves to.
 */
extern char *NextWorldPos;

/**
 * Frame of the world-map place the cursor stands on.
 */
extern CFrame *MapMoveCursor;

/**
 * Texture the travel page draws its map with.
 */
extern CTexture *MenuMoveTex;

/**
 * Characters the weapon page draws each listed weapon's model with.
 */
extern CCharacter DngWeaponFrm[];

/**
 * Name of the status board texture.
 */
extern char CharaStatusTextureName[];

} // namespace dc::game
