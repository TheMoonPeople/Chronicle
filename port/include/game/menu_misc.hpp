#pragma once

#include "common.h"

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
class CTexture;
class CUserStatus;
class ClsMes;
struct BT_SHOT_EFFECT;
struct WEAPON_HAVE;

/**
 * Returns the game flag number the battle menu shows for a page, given the quest dungeon state.
 *
 * @mangled NowGetGameFlagForBtlMenu__Fi
 * @address 0x20BEC0
 * @size 0xB8
 */
int NowGetGameFlagForBtlMenu(int game_flag);

/**
 * Returns the save data's game flag 0x30, which the menus check for the Hebikiri.
 *
 * @mangled GetMenuHebikiriFlag__Fv
 * @address 0x20BF80
 * @size 0x28
 */
int GetMenuHebikiriFlag();

/**
 * Equips a character's default weapon from among the weapons in its dungeon status slots.
 *
 * @mangled EquipDefaultWeapon__Fi
 * @address 0x20BFB0
 * @size 0xB4
 */
void EquipDefaultWeapon(int chara_no);

/**
 * Draws the menu's empty-slot picture from a named texture at a screen position.
 *
 * @mangled DrawMenuNothing__FiiiiPcii
 * @address 0x20C070
 * @size 0x360
 */
void DrawMenuNothing(int x, int y, int width, int height, char *name, int custom, int alpha);

/**
 * Returns the item use volume the menu holds.
 *
 * @mangled GetMenuItemUseVolume__Fv
 * @address 0x20C3D0
 * @size 0xC
 */
int GetMenuItemUseVolume();

/**
 * Applies the effect of using an item on a character or weapon and returns the outcome.
 *
 * @mangled ItemUseFunc__FP11CUserStatusiiiP11WEAPON_HAVE
 * @address 0x20C3E0
 * @size 0x9E4
 */
int ItemUseFunc(CUserStatus *status, int item_no, int chara, int target, WEAPON_HAVE *weapon);

/**
 * Returns a weapon's damage rate: 1.5 for weapon 0x110 once its durability falls to a set fraction, otherwise 1.0.
 *
 * @mangled GetNowWeaponRate__FP11WEAPON_HAVE
 * @address 0x20CDD0
 * @size 0x5C
 */
float GetNowWeaponRate(WEAPON_HAVE *weapon);

/**
 * Reports whether a weapon can be broken down, which needs its unk_02 value to be at least five.
 *
 * @mangled WeaponStatusBreakEnable__FP11WEAPON_HAVE
 * @address 0x20CE30
 * @size 0x34
 */
int WeaponStatusBreakEnable(WEAPON_HAVE *weapon);

/**
 * Returns a weapon's number of build-up entries and counts the enabled ones through the reference.
 *
 * @mangled WeaponStatusBuildUp__FP11WEAPON_HAVERi
 * @address 0x20CE70
 * @size 0xC0
 */
int WeaponStatusBuildUp(WEAPON_HAVE *weapon, int &enabled_count);

/**
 * Shows the intact or broken model frame of weapon 0x110 by its remaining durability.
 *
 * @mangled MenuWeaponSpSet__FP10CCharacterP11WEAPON_HAVE
 * @address 0x20CF30
 * @size 0x114
 */
void MenuWeaponSpSet(CCharacter *chara, WEAPON_HAVE *weapon);

/**
 * Records whether the menu character's weapon effect file is being read.
 *
 * @mangled SetMenuCharaEffectReadFlag__Fi
 * @address 0x20D050
 * @size 0xC
 */
void SetMenuCharaEffectReadFlag(int flag);

/**
 * Returns whether the menu character's weapon effect file is being read.
 *
 * @mangled GetMenuCharaEffectReadFlag__Fv
 * @address 0x20D060
 * @size 0xC
 */
int GetMenuCharaEffectReadFlag();

/**
 * Returns the weapon shot effect the menu character uses.
 *
 * @mangled GetDngWepEffectPointer__Fv
 * @address 0x20D070
 * @size 0xC
 */
BT_SHOT_EFFECT *GetDngWepEffectPointer();

/**
 * Returns the buffer the menu reads weapon effect files into.
 *
 * @mangled GetWepEffectMenuReadBuf__Fv
 * @address 0x20D080
 * @size 0xC
 */
u_long128 *GetWepEffectMenuReadBuf();

/**
 * Records the effect kind that was active before the menu changed it.
 *
 * @mangled SetOldEffectKind__Fi
 * @address 0x20D090
 * @size 0xC
 */
void SetOldEffectKind(int kind);

/**
 * Starts reading the active character's weapon effect file and returns the effect it is for.
 *
 * @mangled DngWepEffectReadStart__Fv
 * @address 0x20D0B0
 * @size 0xB4
 */
BT_SHOT_EFFECT *DngWepEffectReadStart();

/**
 * Attaches the read weapon effect to the menu character.
 *
 * @mangled MenuWeaponEffectSet__Fi
 * @address 0x20D170
 * @size 0x44
 */
void MenuWeaponEffectSet(int effect_no);

/**
 * Returns the menu's current test number.
 *
 * @mangled GetNowTestNo__Fv
 * @address 0x20D1C0
 * @size 0xC
 */
int GetNowTestNo();

/**
 * Reads a character's menu weapon models and shadow image into a buffer and, in a dungeon, starts its weapon effect read; returns 0 when a read fails.
 *
 * @mangled StartReadWepMDS__FP1i
 * @address 0x20D1D0
 * @size 0x1D4
 */
int StartReadWepMDS(u_long128 *buffer, int chara);

/**
 * Marks every weapon model reference entry unused.
 *
 * @mangled InitMenuWeaponModelReference__Fv
 * @address 0x20D420
 * @size 0x48
 */
void InitMenuWeaponModelReference();

/**
 * Stores the two values of one weapon model reference entry.
 *
 * @mangled SetMenuWeaponModelReference__Fiii
 * @address 0x20D470
 * @size 0x28
 */
void SetMenuWeaponModelReference(int index, int frame_no, int value);

/**
 * Returns the first value of a weapon model reference entry, the model's frame number.
 *
 * @mangled GetMenuWeaponModelFrameNo__Fi
 * @address 0x20D4A0
 * @size 0x1C
 */
int GetMenuWeaponModelFrameNo(int index);

/**
 * Enters a menu page's weapon models and textures from their read files and returns the outcome.
 *
 * @mangled EnterWeaponModel__Fiii
 * @address 0x20D4C0
 * @size 0x464
 */
int EnterWeaponModel(int chara, int texture_block, int weapon_slot);

/**
 * Builds the models of a character's weapons into the menu's model cache.
 *
 * @mangled WeaponModelBuildFunc__Fii
 * @address 0x20D930
 * @size 0x430
 */
void WeaponModelBuildFunc(int chara, int texture_block);

/**
 * Builds the model of a character's equipped weapon for the dungeon and attaches its effect.
 *
 * @mangled DngWeaponEquipModelBuild__FiiP1
 * @address 0x20DD60
 * @size 0x114
 */
int DngWeaponEquipModelBuild(int chara, int texture_block, u_long128 *read_buffer);

/**
 * Returns the status bits of one character, or zero when there is no dungeon status.
 *
 * @mangled GetNowActiveCharaStatus__Fi
 * @address 0x20DEC0
 * @size 0x30
 */
int GetNowActiveCharaStatus(int chara_no);

/**
 * Switches the menu character model to its hurt motion when its status or HP calls for it, and sets the motion's speed.
 *
 * @mangled SetNowCharaMotionNo__Fi
 * @address 0x20DEF0
 * @size 0x12C
 */
void SetNowCharaMotionNo(int chara);

/**
 * Saves the ambient light and tints it for the status of a character.
 *
 * @mangled SetItemMenuColor__Fi
 * @address 0x20E020
 * @size 0x150
 */
void SetItemMenuColor(int chara);

/**
 * Restores the ambient light saved before the menu tinted it.
 *
 * @mangled SetItemMenuOldAmbient__Fv
 * @address 0x20E170
 * @size 0x28
 */
void SetItemMenuOldAmbient();

/**
 * Starts the background read of a menu character model file; returns 1 when the read could not be queued and 0 otherwise.
 *
 * @mangled StartLoadCharaMDS__FP1ii
 * @address 0x20E1A0
 * @size 0xE0
 */
int StartLoadCharaMDS(u_long128 *buffer, int chara, int read_no);

/**
 * Builds the menu character model from its read file and poses it for the menu.
 *
 * @mangled MenuCharaMDSBuild2__Fii
 * @address 0x20E280
 * @size 0x2A8
 */
void MenuCharaMDSBuild2(int chara, int texture_block);

/**
 * Starts the background reads of the character change screen and reports whether they were started.
 *
 * @mangled CharaChangeInitToGL__FP1i
 * @address 0x20E5B0
 * @size 0x2F4
 */
int CharaChangeInitToGL(u_long128 *buffer, int chara);

/**
 * Starts the background read of the character change screen's voice data.
 *
 * @mangled CharaChangeInitToGL2__Fi
 * @address 0x20E8B0
 * @size 0xB0
 */
void CharaChangeInitToGL2(int load_icon);

/**
 * Loads the battle menu's character and sets up its weapon effect.
 *
 * @mangled BtMenuLoadChara__Fv
 * @address 0x20E960
 * @size 0x94
 */
void BtMenuLoadChara();

/**
 * Enters the battle menu's read textures and clears its load state.
 *
 * @mangled BtMenuLoad2__Fi
 * @address 0x20EA00
 * @size 0xD4
 */
void BtMenuLoad2(int load_texture);

/**
 * Reports whether all twelve of the East King's game flags are set.
 *
 * @mangled EastKingCheckComplete__Fv
 * @address 0x20EAE0
 * @size 0x80
 */
int EastKingCheckComplete();

/**
 * Records whether the monster name window is drawn.
 *
 * @mangled SetMonsterNameDrawFlag__Fi
 * @address 0x20EB60
 * @size 0xC
 */
void SetMonsterNameDrawFlag(int flag);

/**
 * Returns whether the monster name window is drawn.
 *
 * @mangled GetMonsterNameDrawFlag__Fv
 * @address 0x20EB70
 * @size 0xC
 */
int GetMonsterNameDrawFlag();

/**
 * Prepares the monster name message window over the given message buffers.
 *
 * @mangled MonsterNameInit__FP6ClsMesPsPUc
 * @address 0x20EB80
 * @size 0x208
 */
void MonsterNameInit(ClsMes *mes, short *message_buffer, unsigned char *texture_buffer);

/**
 * Builds the monster name message window for a message.
 *
 * @mangled MonsterNameMake__Fi
 * @address 0x20ED90
 * @size 0x74
 */
void MonsterNameMake(int mes_no);

/**
 * Sets the screen position of the monster name message window.
 *
 * @mangled MonsterNamePosSet__Fii
 * @address 0x20EE10
 * @size 0x5C
 */
void MonsterNamePosSet(int x, int y);

/**
 * Steps and draws the monster name message window while it is enabled.
 *
 * @mangled MonsterNameDraw__Fv
 * @address 0x20EE70
 * @size 0x114
 */
void MonsterNameDraw();

/**
 * Loads the textures and message windows of the dungeon escape prompt.
 *
 * @mangled DngEscapeMsgInit__FP6ClsMesP6ClsMesi
 * @address 0x20EF90
 * @size 0x2AC
 */
void DngEscapeMsgInit(ClsMes *title, ClsMes *choice, int dungeon);

/**
 * Steps the dungeon escape prompt's fade and draws it.
 *
 * @mangled DngEscapeMsgDraw__Fv
 * @address 0x20F240
 * @size 0x120
 */
void DngEscapeMsgDraw();

/**
 * Handles one frame of pad input for the dungeon escape prompt and returns its result.
 *
 * @mangled DngEscapeMsgLoop__Fv
 * @address 0x20F360
 * @size 0x164
 */
int DngEscapeMsgLoop();

/**
 * Collects what the party holds beyond its inventory's room, which must be thrown away, and reports whether there is any.
 *
 * @mangled CheckItemThrow__FPiPi
 * @address 0x20F4D0
 * @size 0x1B0
 */
int CheckItemThrow(int *items, int *values);

/**
 * Stores the index of a weapon's largest element value in its best_elem.
 *
 * @mangled SetWeaponElementStatus__FP11WEAPON_HAVE
 * @address 0x20F680
 * @size 0x5C
 */
void SetWeaponElementStatus(WEAPON_HAVE *weapon);

/**
 * Returns weapon option bits with each pair of opposed options cancelled.
 *
 * @mangled CheckWeaponOptionStatus__Fi
 * @address 0x20F6E0
 * @size 0x8C
 */
int CheckWeaponOptionStatus(int options);

/**
 * Returns whether a weapon option is a benefit or a drawback.
 *
 * @mangled IsWeaponOptionGoodOrBad__Fi
 * @address 0x20F770
 * @size 0x40
 */
int IsWeaponOptionGoodOrBad(int option);

/**
 * Returns the option flags a weapon's data gives it, or 1 when the weapon has no data.
 *
 * @mangled DefaultWeaponOptionSet__Fi
 * @address 0x20F7B0
 * @size 0x3C
 */
int DefaultWeaponOptionSet(int weapon_no);

/**
 * Draws the icons of the options a weapon carries.
 *
 * @mangled WeaponOptionStatusDraw__FP11WEAPON_HAVEiii
 * @address 0x20F7F0
 * @size 0x224
 */
void WeaponOptionStatusDraw(WEAPON_HAVE *weapon, int x, int y, int alpha);

/**
 * Draws a weapon's star rating.
 *
 * @mangled WeaponStarDraw__FiiP11WEAPON_HAVEi
 * @address 0x20FA20
 * @size 0x1C0
 */
void WeaponStarDraw(int x, int y, WEAPON_HAVE *weapon, int alpha);

/**
 * Applies one of the R gate's weapon effects by kind, or returns -1 without a weapon. @see RGateEffect.
 *
 * @mangled WeaponDataChangeByRGate__FP11WEAPON_HAVEi
 * @address 0x20FCE0
 * @size 0x4F0
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
 * The ambient light saved before the item menu tinted it.
 */
extern float MenuCharaOldAmbient[4];

/**
 * Path buffer a weapon model's file name is built in.
 */
extern char MenureadFile[64];

/**
 * Base path used to assemble character model file names.
 */
extern char readFilePath[0x40];

/**
 * Model file name for each playable character.
 */
extern const char *charaFile[6];

/**
 * The amount the last item use gave, a base value plus a random part.
 */
extern int MenuItemUseVolume;

/**
 * Buffer the weapon page's model files are read into.
 */
extern u_long128 *WeaponRead_Buf;

/**
 * The menu's weapon-effect read flag.
 */
extern s16 MenuCharaEffectReadFlag;

/**
 * The weapon effect kind SetOldEffectKind records.
 */
extern s16 MenuCharaOldEffect;

/**
 * The weapon effect the menu's character plays.
 */
extern BT_SHOT_EFFECT *WepEffectMenuPt;

/**
 * The buffer the menu's weapon effect and model are read into.
 */
extern u_long128 *WepEffectMenuReadBuf;

/**
 * Buffer the menu's weapon models are built in.
 */
extern u_long128 *MenuWeaponModelBuildBuffer;

/**
 * Read number of the character model loading in the background.
 */
extern int CharaFileBGReadNo;

/**
 * Party member the character change is loading.
 */
extern s16 charachangeid;

/**
 * Start of the buffer a character change reads its files into.
 */
extern u_long128 *CharaChangeBaseBuf;

/**
 * Buffer a character change reads the party member's model into.
 */
extern u_long128 *menucharReadbuf;

/**
 * Buffer a character change reads the first weapon model into.
 */
extern u_long128 *menud0wepReadBuf;

/**
 * Buffer a character change reads the second weapon model into.
 */
extern u_long128 *menud1wepReadBuf;

/**
 * Buffer a character change reads the third weapon model into.
 */
extern u_long128 *menud2wepReadBuf;

/**
 * Buffer a character change reads the weapon icons into.
 */
extern u_long128 *MenuWepIconCharaChangePtr;

/**
 * Buffer a character change reads the party member's voices into.
 */
extern u_long128 *MenuVoiceLoadPtr;

/**
 * The message window that shows a monster's name.
 */
extern ClsMes *CharaNameMes;

/**
 * The dungeon escape prompt's second message window.
 */
extern ClsMes *DngMenuMes;

/**
 * Whether the monster's name is drawn.
 */
extern s16 CharaNameDrawFlag;

/**
 * How the monster's name is drawn.
 */
extern s16 CharaNameDrawCase;

/**
 * The picture drawn behind the dungeon escape prompt.
 */
extern CTexture *DngEscapeTex;

/**
 * The texture block the dungeon escape prompt's picture is loaded into.
 */
extern s16 DngEscapeBlock;

/**
 * Whether the dungeon escape prompt is closing and fades to black.
 */
extern s16 DngEscapeEndFlag;

/** The directory the weapon models are read from. */
extern const char MenuWepDir[];

/**
 * The character file extension.
 */
extern const char CharaFileExtension[5];

/**
 * Name of the frame image texture.
 */
extern const char FrameImageTexture[];
