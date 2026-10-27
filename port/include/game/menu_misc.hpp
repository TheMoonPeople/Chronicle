#pragma once

#include "common.h"

namespace dc::game {

/**
 * What the travel page offers, as NowGetGameFlagForBtlMenu returns it and MENU_MOVE_INFO::mode holds it.
 */
// clang-format off
enum MenuMoveMode {
    MENU_MOVE_DUNGEON_ESCAPE   = 0,  /**< Leave the dungeon. */
    MENU_MOVE_WORLD_MAP        = 1,  /**< World map. */
    MENU_MOVE_INTERIOR_OUT     = 2,  /**< Leave the interior. */
    MENU_MOVE_WORLD_MAP_DIRECT = 5,  /**< World map opened from a town script. */
    MENU_MOVE_FIRST_DUNGEON    = 10, /**< Only the Divine Beast Cave, before the world map is held. */
    MENU_MOVE_LOCKED           = 11, /**< Travel refused. */
};

// clang-format on

/**
 * What an item is used on, as ItemUseFunc's target and ITEM_MENU_MODE_INFO::use_target take it.
 */
// clang-format off
enum ItemUseTarget {
    ITEM_USE_ON_CHARA        = 1, /**< The current character. */
    ITEM_USE_ON_EQUIP_WEAPON = 2, /**< The equipped weapon. */
    ITEM_USE_ON_BOARD_WEAPON = 4, /**< A weapon on the personal board. */
};

// clang-format on

/**
 * Weapon effects that WeaponDataChangeByRGate applies.
 */
// clang-format off
enum RGateEffect {
    RGATE_ABS_FULL    = 0, /**< Fills the weapon's ABS. */
    RGATE_STAT_DOWN   = 1, /**< Lowers a random stat. */
    RGATE_WHP_UP      = 2, /**< Raises the maximum WHp. */
    RGATE_WHP_DOWN    = 3, /**< Lowers the maximum WHp. */
    RGATE_WHP_CURE    = 4, /**< Restores the WHp. */
    RGATE_WHP_QUARTER = 5, /**< Quarters the WHp. */
};

// clang-format on

class CCharacter;
class CUserStatus;
class ClsMes;
struct BT_SHOT_EFFECT;
struct WEAPON_HAVE;

/**
 * Returns the game flag number the battle menu shows for a page, given the quest dungeon state.
 */
int NowGetGameFlagForBtlMenu(int game_flag);

/**
 * Returns the save data's game flag 0x30, which the menus check for the Hebikiri.
 */
int GetMenuHebikiriFlag();

/**
 * Equips a character's default weapon from among the weapons in its dungeon status slots.
 */
void EquipDefaultWeapon(int chara_no);

/**
 * Draws the menu's empty-slot picture from a named texture at a screen position.
 */
void DrawMenuNothing(int x, int y, int width, int height, char *name, int custom, int alpha);

/**
 * Returns the item use volume the menu holds.
 */
int GetMenuItemUseVolume();

/**
 * Applies the effect of using an item on a character or weapon and returns the outcome.
 */
int ItemUseFunc(CUserStatus *status, int item_no, int chara, int target, WEAPON_HAVE *weapon);

/**
 * Returns a weapon's damage rate: 1.5 for weapon 0x110 once its durability falls to a set fraction, otherwise 1.0.
 */
float GetNowWeaponRate(WEAPON_HAVE *weapon);

/**
 * Reports whether a weapon can be broken down, which needs its unk_02 value to be at least five.
 */
int WeaponStatusBreakEnable(WEAPON_HAVE *weapon);

/**
 * Returns a weapon's number of build-up entries and counts the enabled ones through the reference.
 */
int WeaponStatusBuildUp(WEAPON_HAVE *weapon, int &enabled_count);

/**
 * Shows the intact or broken model frame of weapon 0x110 by its remaining durability.
 */
void MenuWeaponSpSet(CCharacter *chara, WEAPON_HAVE *weapon);

/**
 * Records whether the menu character's weapon effect file is being read.
 */
void SetMenuCharaEffectReadFlag(int flag);

/**
 * Returns whether the menu character's weapon effect file is being read.
 */
int GetMenuCharaEffectReadFlag();

/**
 * Returns the weapon shot effect the menu character uses.
 */
BT_SHOT_EFFECT *GetDngWepEffectPointer();

/**
 * Returns the buffer the menu reads weapon effect files into.
 */
u_long128 *GetWepEffectMenuReadBuf();

/**
 * Records the effect kind that was active before the menu changed it.
 */
void SetOldEffectKind(int kind);

/**
 * Starts reading the active character's weapon effect file and returns the effect it is for.
 */
BT_SHOT_EFFECT *DngWepEffectReadStart();

/**
 * Attaches the read weapon effect to the menu character.
 */
void MenuWeaponEffectSet(int effect_no);

/**
 * Returns the menu's current test number.
 */
int GetNowTestNo();

/**
 * Reads a character's menu weapon models and shadow image into a buffer and, in a dungeon, starts its weapon effect read; returns 0 when a read fails.
 */
int StartReadWepMDS(u_long128 *buffer, int chara);

/**
 * Marks every weapon model reference entry unused.
 */
void InitMenuWeaponModelReference();

/**
 * Stores the two values of one weapon model reference entry.
 */
void SetMenuWeaponModelReference(int index, int frame_no, int value);

/**
 * Returns the first value of a weapon model reference entry, the model's frame number.
 */
int GetMenuWeaponModelFrameNo(int index);

/**
 * Enters a menu page's weapon models and textures from their read files and returns the outcome.
 */
int EnterWeaponModel(int chara, int texture_block, int weapon_slot);

/**
 * Builds the models of a character's weapons into the menu's model cache.
 */
void WeaponModelBuildFunc(int chara, int texture_block);

/**
 * Builds the model of a character's equipped weapon for the dungeon and attaches its effect.
 */
int DngWeaponEquipModelBuild(int chara, int texture_block, u_long128 *read_buffer);

/**
 * Returns the status bits of one character, or zero when there is no dungeon status.
 */
int GetNowActiveCharaStatus(int chara_no);

/**
 * Switches the menu character model to its hurt motion when its status or HP calls for it, and sets the motion's speed.
 */
void SetNowCharaMotionNo(int chara);

/**
 * Saves the ambient light and tints it for the status of a character.
 */
void SetItemMenuColor(int chara);

/**
 * Restores the ambient light saved before the menu tinted it.
 */
void SetItemMenuOldAmbient();

/**
 * Starts the background read of a menu character model file; returns 1 when the read could not be queued and 0 otherwise.
 */
int StartLoadCharaMDS(u_long128 *buffer, int chara, int read_no);

/**
 * Builds the menu character model from its read file and poses it for the menu.
 */
void MenuCharaMDSBuild2(int chara, int texture_block);

/**
 * Starts the background reads of the character change screen and reports whether they were started.
 */
int CharaChangeInitToGL(u_long128 *buffer, int chara);

/**
 * Starts the background read of the character change screen's voice data.
 */
void CharaChangeInitToGL2(int load_icon);

/**
 * Loads the battle menu's character and sets up its weapon effect.
 */
void BtMenuLoadChara();

/**
 * Enters the battle menu's read textures and clears its load state.
 */
void BtMenuLoad2(int load_texture);

/**
 * Reports whether all twelve of the East King's game flags are set.
 */
int EastKingCheckComplete();

/**
 * Records whether the monster name window is drawn.
 */
void SetMonsterNameDrawFlag(int flag);

/**
 * Returns whether the monster name window is drawn.
 */
int GetMonsterNameDrawFlag();

/**
 * Prepares the monster name message window over the given message buffers.
 */
void MonsterNameInit(ClsMes *mes, short *message_buffer, unsigned char *texture_buffer);

/**
 * Builds the monster name message window for a message.
 */
void MonsterNameMake(int mes_no);

/**
 * Sets the screen position of the monster name message window.
 */
void MonsterNamePosSet(int x, int y);

/**
 * Steps and draws the monster name message window while it is enabled.
 */
void MonsterNameDraw();

/**
 * Loads the textures and message windows of the dungeon escape prompt.
 */
void DngEscapeMsgInit(ClsMes *title, ClsMes *choice, int dungeon);

/**
 * Steps the dungeon escape prompt's fade and draws it.
 */
void DngEscapeMsgDraw();

/**
 * Handles one frame of pad input for the dungeon escape prompt and returns its result.
 */
int DngEscapeMsgLoop();

/**
 * Collects what the party holds beyond its inventory's room, which must be thrown away, and reports whether there is any.
 */
int CheckItemThrow(int *items, int *values);

/**
 * Stores the index of a weapon's largest element value in its best_elem.
 */
void SetWeaponElementStatus(WEAPON_HAVE *weapon);

/**
 * Returns weapon option bits with each pair of opposed options cancelled.
 */
int CheckWeaponOptionStatus(int options);

/**
 * Returns whether a weapon option is a benefit or a drawback.
 */
int IsWeaponOptionGoodOrBad(int option);

/**
 * Returns the option flags a weapon's data gives it, or 1 when the weapon has no data.
 */
int DefaultWeaponOptionSet(int weapon_no);

/**
 * Draws the icons of the options a weapon carries.
 */
void WeaponOptionStatusDraw(WEAPON_HAVE *weapon, int x, int y, int alpha);

/**
 * Draws a weapon's star rating.
 */
void WeaponStarDraw(int x, int y, WEAPON_HAVE *weapon, int alpha);

/**
 * Applies one of the R gate's weapon effects by kind, or returns -1 without a weapon. @see RGateEffect.
 */
int WeaponDataChangeByRGate(WEAPON_HAVE *weapon, int kind);

/**
 * Frame numbers of the menu's cached weapon models.
 */
extern int MenuWeaponModelData[42];

/**
 * Each weapon model slot's frame number and read state.
 */
extern int MenuWeaponModelInfo[10][2];

/**
 * The party member the character change menu is previewing.
 */
extern s16 charachangeid;

/**
 * The buffer the menu's weapon effect and model are read into.
 */
extern u_long128 *WepEffectMenuReadBuf;

/**
 * Buffer the menu's weapon models are built in.
 */
extern u_long128 *MenuWeaponModelBuildBuffer;

} // namespace dc::game
