#pragma once

#include "common.h"

#include "menu_draw.hpp"

namespace dc::game {

/**
 * Steps of the save menu, indexing SaveMenuFunc, as SAVE_MENU_STATE::key_no holds them.
 */
// clang-format off
enum SaveMenuKey {
    SAVE_KEY_FADE_IN            = 0,  /**< Fading in. */
    SAVE_KEY_FADE_OUT           = 1,  /**< Fading out. */
    SAVE_KEY_MODE_SELECT        = 2,  /**< Choosing load or save. */
    SAVE_KEY_MC_SELECT          = 3,  /**< Choosing a memory card slot. */
    SAVE_KEY_CHECK_MC_TYPE      = 4,  /**< Checking the card type. */
    SAVE_KEY_CHECK_MC           = 5,  /**< Checking the card. */
    SAVE_KEY_LOAD_CONFIG        = 6,  /**< Loading the configuration. */
    SAVE_KEY_FILE_SELECT        = 7,  /**< Choosing a save file. */
    SAVE_KEY_SAVE_CHECK         = 8,  /**< Checking before a save. */
    SAVE_KEY_SAVE_DECIDE        = 9,  /**< Confirming a save. */
    SAVE_KEY_SAVE               = 10, /**< Saving. */
    SAVE_KEY_END_SAVE           = 11, /**< Save finished. */
    SAVE_KEY_LOAD_DECIDE        = 12, /**< Confirming a load. */
    SAVE_KEY_LOAD               = 13, /**< Loading. */
    SAVE_KEY_ALERT              = 14, /**< Alert shown. */
    SAVE_KEY_NEW_DIR            = 15, /**< Creating the save directory. */
    SAVE_KEY_NEW_DIR_SELECT     = 16, /**< Confirming the save directory. */
    SAVE_KEY_FORMAT             = 17, /**< Formatting. */
    SAVE_KEY_UNFORMAT           = 18, /**< Card unformatted. */
    SAVE_KEY_DIF_VERSION        = 19, /**< Save from another version. */
    SAVE_KEY_DELETE             = 20, /**< Deleting. */
    SAVE_KEY_COPY               = 21, /**< Copying. */
    SAVE_KEY_AFTER_ENDING       = 22, /**< Asking to save after the ending. */
    SAVE_KEY_SAVE_ENDING        = 23, /**< Saving after the ending. */
    SAVE_KEY_SAVE_DECIDE_ENDING = 24, /**< Confirming a save after the ending. */
    SAVE_KEY_END_SAVE_ENDING    = 25, /**< Save after the ending finished. */
};

// clang-format on

/**
 * Screen that opened the save menu, as SAVE_MENU_STATE::mode holds it.
 */
// clang-format off
enum SaveMenuMode {
    SAVE_MENU_MODE_LOAD            = 0, /**< Load from the title. */
    SAVE_MENU_MODE_SAVE            = 1, /**< Save. */
    SAVE_MENU_MODE_ENDING          = 2, /**< Save after the ending. */
    SAVE_MENU_MODE_ENDING_NO_CLEAR = 3, /**< Save after the ending without marking the game cleared. */
};

// clang-format on

/**
 * Card access a save file is chosen for, as SAVE_MENU_STATE::access_kind holds it.
 */
// clang-format off
enum SaveAccessKind {
    SAVE_ACCESS_LOAD = 1, /**< Load. */
    SAVE_ACCESS_SAVE = 2, /**< Save. */
};

// clang-format on

/**
 * Alerts the save menu shows, as SAVE_MENU_STATE::alert_no holds them.
 */
// clang-format off
enum SaveMenuAlert {
    SAVE_ALERT_NONE          = 0,  /**< No alert. */
    SAVE_ALERT_NO_CARD       = 1,  /**< No PS2 memory card. */
    SAVE_ALERT_CARD_FULL     = 2,  /**< The card is full. */
    SAVE_ALERT_UNK_3         = 3,  /**< Shows no message; never set. */
    SAVE_ALERT_UNK_4         = 4,  /**< Shows no message; never set. */
    SAVE_ALERT_UNK_5         = 5,  /**< Shows no message; never set. */
    SAVE_ALERT_CARD_ERROR    = 6,  /**< The card failed or was changed. */
    SAVE_ALERT_FORMAT_FAILED = 7,  /**< Formatting failed. */
    SAVE_ALERT_SAVE_FAILED   = 8,  /**< Saving failed. */
    SAVE_ALERT_LOAD_FAILED   = 9,  /**< Loading failed. */
    SAVE_ALERT_NO_SPACE_DIR  = 10, /**< Too little space for a new save directory. */
    SAVE_ALERT_NO_SPACE_FILE = 11, /**< Too little space for a new save file. */
    SAVE_ALERT_NO_SAVE_DATA  = 12, /**< No save data to load. */
    SAVE_ALERT_CARD_MISSING  = 14, /**< The card was removed or is not a PS2 card. */
};

// clang-format on

/**
 * Steps of the event item menu, as MINI_MENU_INFO::state holds them.
 */
// clang-format off
enum MiniMenuState {
    MINI_MENU_CHOOSING = 0, /**< Choosing. */
    MINI_MENU_REFUSED  = 1, /**< Refusal shown. */
    MINI_MENU_FADE_IN  = 2, /**< Fading in. */
    MINI_MENU_FADE_OUT = 3, /**< Fading out. */
};

// clang-format on

/**
 * What SaveEnableCheck reports.
 */
// clang-format off
enum SaveEnableResult {
    SAVE_ENABLE_NO_SPACE = -1, /**< No card has room for a new save. */
    SAVE_ENABLE_NO_CARD  = 0,  /**< No usable PS2 memory card. */
    SAVE_ENABLE_OK       = 1,  /**< A card can take a save. */
};

// clang-format on

/**
 * Holds the save menu's current step and the arguments its steps pass to
 * each other.
 */
struct SAVE_MENU_STATE {
    s32 mode;          /**< Screen that opened the menu. @see SaveMenuMode. */
    s32 key_no;        /**< Index of the step run next, into SaveMenuFunc. @see SaveMenuKey. */
    s32 return_key_no; /**< Step to go back to once a card operation finishes, or -1. */
    s32 file_no;       /**< Save slot the current step works on. */
    s32 board_y;       /**< Scroll position of the save boards, eased toward the selected slot. */
    s8  result;        /**< How the screen ended. @see MenuSaveResult. */
    u8  unk_15[3];
    s32 loaded;      /**< Whether a save file has been loaded. */
    s32 access_kind; /**< Card operation the menu performs. @see SaveAccessKind. */
    s32 alert_no;    /**< Alert the alert step shows, which picks its message. @see SaveMenuAlert. */
    u8  unk_24[4];
    s32 step_time;     /**< Frames spent in the current fade step. */
    s32 block_no;      /**< Texture block the save board's textures load into. */
    s32 texture_ready; /**< Whether the save board's textures have been entered. */
};

STATIC_ASSERT(sizeof(SAVE_MENU_STATE) == 0x34);

/** Current step of the save menu and the arguments it carries between steps. */
extern SAVE_MENU_STATE SaveMenu;

/** Textures of the save menu's three character sets, indexed by input mode. */
extern CTexture *SaveMenuMojiTextbl[4];

/**
 * Holds the state of the event item selection menu.
 */
struct MINI_MENU_INFO {
    s16 fish_mode; /**< Nonzero when the menu offers fishing bait rather than event items. */
    u8  unk_02[2];
    s16 event_item_num; /**< Items of the pack that events can take. */
    s16 lang;           /**< Menu language, which picks the board's layout. */
    s32 cursor;         /**< Slot the cursor is on. */
    s32 usable[13];     /**< Items the event accepts, ended by a negative number. */
    s32 selected;       /**< Item the player picked, or -1. */
    s8  scroll_row;     /**< Row of five slots shown at the top of the board. */
    s8  usable_num;     /**< Number of items the event accepts. */
    u8  unk_46[2];
    s32 vanish; /**< Whether the item picked is used up. */
    s16 state;  /**< Step of the event item menu. @see MiniMenuState. */
    u8  unk_4E[2];
    s32 state_time; /**< Frames spent in the current state. */
};

STATIC_ASSERT(sizeof(MINI_MENU_INFO) == 0x54);

/**
 * Returns the message number the save menu shows for its current step.
 */
int GetSaveMenuMsgNo();

/**
 * Checks the memory card before a save and picks the save menu's next step.
 */
int SaveMenuKeySaveCheck();

/**
 * Handles the save menu's prompt to confirm a save.
 */
int SaveMenuKeySaveDecide();

/**
 * Steps the save menu while the game is written to the memory card.
 */
int SaveMenuKeySave();

/**
 * Waits for a button press once a save has finished.
 */
int SaveMenuKeyEndSave();

/**
 * Steps the save menu while a save is read from the memory card.
 */
int SaveMenuKeyLoad();

/**
 * Waits for a button press while the save menu shows an alert.
 */
int SaveMenuKeyArart();

/**
 * Handles the save menu's prompt to create new save data.
 */
int SaveMenuKeyNewDirSelect();

/**
 * Steps the save menu while new save data is created on the memory card.
 */
int SaveMenuKeyNewDir();

/**
 * Handles the save menu's memory card format step.
 */
int SaveMenuKeyFormat();

/**
 * Handles the menu response after checking an unformatted memory card.
 */
int SaveMenuKeyUnFormat();

/**
 * Waits for a button press when the save data is of a different version.
 */
int SaveMenuKeyDifVersion();

/**
 * Steps the delete choice of the save menu.
 */
s32 SaveMenuKeyDelete();

/**
 * Steps the copy choice of the save menu.
 */
s32 SaveMenuKeyCopy();

/**
 * Handles key input on the save menu offered after the ending.
 */
int SaveMenuKeyAfterEnding();

/**
 * Handles the prompt to confirm a save after the ending.
 */
int SaveMenuKeySaveDecideEnding();

/**
 * Loads the save menu's textures, messages and memory card icon.
 */
int SaveMenuTextureEnter();

/**
 * Tells whether the save menu's fade-out is running.
 */
int SaveMenuEffectFadeOut();

/**
 * Draws the board of one save slot with its save data summary.
 */
void DrawSaveBoard(SAVEDATA_INFO *info, CTexture **name_texture, int x, int y, int unused, int alpha);

/**
 * Draws the board of a save slot that holds no save data.
 */
void DrawNewFileTemplete(int x, int y, int alpha);

/**
 * Starts memory card access and checks the card for existing save data.
 */
int InitExistData();

/**
 * Checks whether the memory card can take a save.
 */
int SaveEnableCheck();

/**
 * Opens the event item selection menu.
 */
void InitEventItemSelect(int block, int *usable, ITEM_PACK *pack, int x, int y, int vanish, int fish_mode);

/**
 * Runs one frame of the event item selection menu, and returns its result.
 */
int EventItemSelectLoop(int *result);

/**
 * Tells whether the player holds an item, in the dungeon inventory or in storage.
 */
int PlayerAllItemCheck(int item);

/**
 * Tells whether an item identifier is an additive attachment.
 */
s32 GetAddAttachItem(int item_no);

/**
 * Converts a weapon item identifier into its position in the owner's weapon chain.
 */
int TransWepNo(int weapon_no);

/**
 * Converts a position in an owner's weapon chain back into the weapon's item identifier.
 */
int TransWepNoNewToOld(int weapon_no);

/**
 * Takes one hit off the weapon the player holds and gives back what broke:
 * 0 for nothing, 1 for the weapon, 2 for the one it fell back to.
 */
int BattleSubWeaponDmg(float amount, int kind);

/** Texture block the event item selection menu's textures load into. */
extern s32 MiniEventTextureBlock;

// Port additions.

/**
 * Handles pointer input while the door/object item picker owns the menu.
 */
bool EventItemPointerUpdate();

} // namespace dc::game
