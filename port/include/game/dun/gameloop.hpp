#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc.hpp"
#include "hit_machingun_effect.hpp"
#include "runscript.hpp"
#include "textureanime.hpp"

/**
 * What the dungeon game loop runs, as BtGameModeFlag holds it.
 */
// clang-format off
enum BtGameMode {
    BT_GAME_MODE_PLAY          = 1, /**< Play. */
    BT_GAME_MODE_BATTLE_MENU   = 2, /**< Battle menu. */
    BT_GAME_MODE_ITEM_SELECT   = 3, /**< Event item selection. */
    BT_GAME_MODE_ENTRANCE_MENU = 4, /**< Dungeon entrance menu. */
    BT_GAME_MODE_CHARA_CHANGE  = 5, /**< Character change. */
    BT_GAME_MODE_ESCAPE_MSG    = 7, /**< Escape message. */
};

// clang-format on

/**
 * What MoveChara is doing, as gameTask holds it; each INIT task is followed by its LOOP task.
 */
// clang-format off
enum GameTask {
    GAME_TASK_PLAY                     = 0x0,   /**< Under the player's control. */
    GAME_TASK_EYE_CAMERA               = 0xA,   /**< First-person camera. */
    GAME_TASK_MENU_OPEN                = 0x1E,  /**< Opening the battle menu. */
    GAME_TASK_MENU_WAIT_FRAME          = 0x1F,  /**< Waiting to capture the frame behind the menu. */
    GAME_TASK_MENU_SKIP                = 0x20,  /**< Passed over. */
    GAME_TASK_MENU_INIT                = 0x21,  /**< Starting the battle menu. */
    GAME_TASK_MENU_CLOSE               = 0x22,  /**< Restoring play after the battle menu. */
    GAME_TASK_BOX_BIG_INIT             = 0x78,  /**< Starting a large treasure box. */
    GAME_TASK_BOX_BIG_LOOP             = 0x79,  /**< Running a large treasure box. */
    GAME_TASK_BOX_SMALL_INIT           = 0x82,  /**< Starting a small treasure box. */
    GAME_TASK_BOX_SMALL_LOOP           = 0x83,  /**< Running a small treasure box. */
    GAME_TASK_ATRA_GET_INIT            = 0x8C,  /**< Starting an Atla pickup. */
    GAME_TASK_ATRA_GET_LOOP            = 0x8D,  /**< Running an Atla pickup. */
    GAME_TASK_ATRA_REFUSE_INIT         = 0x8E,  /**< Telling another character that only Toan takes Atla. */
    GAME_TASK_ATRA_REFUSE_WAIT         = 0x8F,  /**< Waiting for that message to close. */
    GAME_TASK_MAPJUMP_FADE_START       = 0x97,  /**< Fading out for a map jump. */
    GAME_TASK_MAPJUMP_FADE_WAIT        = 0x98,  /**< Jumping once the fade ends. */
    GAME_TASK_PAUSE                    = 0x9B,  /**< Paused. */
    GAME_TASK_URA_SWAP                 = 0xA0,  /**< Switching between the floor and its back floor. */
    GAME_TASK_ESCAPE_INIT              = 0xAA,  /**< Starting an escape. */
    GAME_TASK_ESCAPE_LOOP              = 0xAB,  /**< Running an escape. */
    GAME_TASK_EXIT_FADE_START          = 0xAF,  /**< Fading out to leave the dungeon. */
    GAME_TASK_EXIT_FADE_WAIT           = 0xB0,  /**< Leaving for the town once the fade ends. */
    GAME_TASK_DEAD                     = 0xC8,  /**< Knocked out. */
    GAME_TASK_DEAD_GAMEOVER            = 0xC9,  /**< Party defeated. */
    GAME_TASK_DEAD_CHANGE_CHARA        = 0xCA,  /**< Choosing a character after a knockout. */
    GAME_TASK_DEBUG_OPEN               = 0xDC,  /**< Opening the debug overlay. */
    GAME_TASK_DEBUG_MENU               = 0xDD,  /**< Debug overlay. */
    GAME_TASK_DEBUG_RUN_EVENT          = 0xDE,  /**< Running the debug overlay's event. */
    GAME_TASK_DEBUG_RELOAD_MONSTER     = 0xE3,  /**< Reloading a monster. */
    GAME_TASK_DEBUG_RELOAD_MONSTER_SET = 0xE4,  /**< Reloading a monster set. */
    GAME_TASK_UNK_F0                   = 0xF0,  /**< Only compared; never set. */
    GAME_TASK_CHARA_CHANGED            = 0x122, /**< Showing a character change. */
    GAME_TASK_UNK_123                  = 0x123, /**< Does nothing; never set. */
    GAME_TASK_CHARA_SELECT_LOOP        = 0x127, /**< Running the character selection. */
    GAME_TASK_SCRIPT_START             = 0x190, /**< Starting a system script. */
    GAME_TASK_SCRIPT_RUN               = 0x191, /**< Running a system script. */
    GAME_TASK_SCRIPT_ITEM_SELECT       = 0x19A, /**< Script's item selection. */
    GAME_TASK_SCRIPT_RESTART           = 0x1F4, /**< Returning to the system script. */
    GAME_TASK_GATE_KEY_LOOP            = 0x1FE, /**< Running a gate key pickup. */
    GAME_TASK_ATTACH_LOOP              = 0x208, /**< Running an attachment pickup. */
    GAME_TASK_REVIVE_WAIT              = 0x212, /**< Recovering after a revival item. */
    GAME_TASK_REVIVE_DONE              = 0x213, /**< Waiting for the revival message to close. */
    GAME_TASK_SYSMES_WAIT              = 0x21C, /**< Waiting for a system message to close. */
    GAME_TASK_FLOOR_FADE_RESET         = 0x226, /**< Resetting the floor once a fade ends. */
};

// clang-format on

/**
 * Transition a system script asks the game loop for, as BT_EVENT_INFO::request holds it.
 */
// clang-format off
enum BtEventRequest {
    BT_REQUEST_NONE            = 0, /**< Nothing. */
    BT_REQUEST_ITEM_WINDOW     = 1, /**< Open the item use window. */
    BT_REQUEST_URA_DUNGEON     = 2, /**< Switch to the back floor. */
    BT_REQUEST_ENTRANCE_WINDOW = 3, /**< Open the entrance window. */
    BT_REQUEST_GO_DUNGEON      = 4, /**< Go into the dungeon. */
    BT_REQUEST_RUN_SCRIPT      = 5, /**< Run a script. */
    BT_REQUEST_ESCAPE_WINDOW   = 6, /**< Open the escape window. */
};

// clang-format on

class CSHOT_EFFECT;
class CWeaponEffect;
class CDispCtrl;
class CActiveItemPack;
class CMotionModel;
class CNPCharacter;
class ClsMes;
class CDebugFont;
class CItemBombEffect;
class CShockWave;
struct MOTION_INFO;
class CHealEffect;

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCharacter;
class CCameraFollow;
class CCPoly;
struct BT_SHOT_EFFECT;
struct WEAPON_HAVE;
class CFrame;
class CCollisionData;
class CDngMessageMan;
class CDungeonEventMan;
class CDungeonMap;
class CDranMapField;
class CHitValue;
class CMenuItemStep;
class CMonstorUnit;
class CRandomItem;
class CSHOT_EFFECT_PACK;
class CStealItem;
class CUserStatus;
struct MAP_TRAP_CIRCLE;

/**
 * Stores the state shared by dungeon event scripts and the dungeon loop.
 */
struct BT_EVENT_INFO {
    sceVu0FVECTOR position;                /**< Where the event run by the script plays. */
    sceVu0FVECTOR direction;               /**< Which way that event faces. */
    s32           init_flag_20;            /**< Set to 1 when the event state is initialised and never read. */
    s32           action_mode;             /**< How the player started the event: 0 automatically, 1 with the action button, 2 with the hold button. */
    s32           init_flag_28;            /**< Set to 1 when the event state is initialised and never read. */
    s32           script_no;               /**< System script the dungeon loop runs next, or -1 when none is queued. */
    s32           running_script_no;       /**< System script currently running, or -1 when none is. */
    s32           ext_memory;              /**< Whether the queued script runs out of the monster model arena instead of the script work buffer. */
    s32           event_marker;            /**< Whether the event marker is drawn over the player this frame. */
    s32           item_select_list[16];    /**< Items the script's item window offers, ended by -1. */
    s32           item_select_filtered;    /**< Whether the script's item window is limited to item_select_list. */
    s32           item_select_result;      /**< Address of the script stack entry the chosen item is written back to, or 0. */
    RS_STACKDATA *entrance_result;         /**< Where the floor the player chose is written back. */
    RS_STACKDATA *escape_result;           /**< Where the escape answer is written back. */
    s32           floor_title_off;         /**< Suppresses the floor title when the floor starts. */
    s32           fade_on_start;           /**< Whether the next system script resets the editor fade when it starts. */
    s32           bee_npc;                 /**< NPC unit the bee follows, or -1 when there is no bee. */
    s32           request;                 /**< Dungeon transition requested by the event script. @see BtEventRequest. */
    s32           chained_script_no;       /**< Script _RUN_SCRIPT_NO asks to run after the current one ends. */
    s32           clear_script_no;         /**< Script to run once every monster on the floor is defeated, or -1. */
    s32           clear_script_ext_memory; /**< ext_memory setting for clear_script_no. */
    s32           script_main_chr;         /**< Character _SET_MAIN_CHR2 last asked for, or -1 when none is set. */
    s32           treasure_box;            /**< Treasure box the item-box event scripts act on. */
    s32           no_reset_chara;          /**< Keeps the script's chosen character when the script ends. */
    s32           chara_help;              /**< Whether the event was started by the character it asks for. */
    s32           no_status_recover;       /**< Prevents restoring party status when entering the floor. */
    s32           unk_BC;
};

STATIC_ASSERT(sizeof(BT_EVENT_INFO) == 0xC0);

/**
 * State shared by dungeon event scripts and the dungeon loop.
 */
extern "C" BT_EVENT_INFO BtEventInfo;

/**
 * Player status shared by the dungeon loop and battle commands.
 */
extern CUserStatus *UserStatus;

/**
 * Monster unit currently used by the dungeon loop and its scripts.
 */
extern CMonstorUnit *NowMonstorUnit;

/** Player character. */
extern "C" CCharacter CharaMain;

/** Character currently controlled by the player. */
extern s32 nowUnitNow;

/** Debug switches shared by dungeon rendering. */
extern "C" s32 DebugStatus[21];

/** Message window the dungeon shows. */
extern "C" CDngMessageMan DngMessMan;

/** Item the player is stealing back from a monster. */
extern "C" CStealItem StealItem;

/** Item count animation of the dungeon's menus. */
extern "C" CMenuItemStep ItemVolumeStep;

/** Model used to draw the monster cursor. */
extern CFrame *cursorFrame;

/** Dungeon the player is in. */
extern s32 selectMapNo;

/** Items lying on the floor. */
extern CRandomItem *RandomItem;

/** Whether the player is currently using the movement speed boost. */
extern s32 BtBySpeedFlag;

/** Whether the party is in the back area of the current dungeon floor. */
extern s32 BtUraDongeon;

/** Whether the player has the current floor map. */
extern s32 BtEquipMap;

/** Whether the player has the magical crystal that reveals monsters on the minimap. */
extern s32 BtEquipMasuisyou;

/** Map of the floor the player is on. */
extern CDungeonMap *NowDngMap;

/** Drainage-field set used by the current dungeon floor. */
extern CDranMapField *NowDranMapField;

/** Damage numbers the dungeon shows. */
extern CHitValue *NowHitValue;

/**
 * Stores the machine-gun hit flashes used by dungeon projectile effects.
 *
 * @mangled OzumondShotEffect
 * @address 0x1EFC000
 * @size 0x140
 */
extern "C" CHIT_MACHINGUN_EFFECT OzumondShotEffect;

/** Projectile effects the dungeon has running. */
extern CSHOT_EFFECT_PACK *NowShotEffect;

/** Hits the collision test found this frame. */
extern CCollisionData *NowColData;

/** Weapon currently equipped by the player character. */
extern WEAPON_HAVE *NowWeaponHave;

/**
 * Event manager of the floor the player is on.
 */
extern CDungeonEventMan *NowEventMan;

/**
 * Fade-in level of the Atlamillia pickup's light, up to 256.
 */
extern float atraGetStatusRate__2;

/**
 * Pack buffer the dungeon started with, reused for every map load.
 */
extern u_int *old_read_buffer;

/**
 * @mangled LoadBaseTexture__Fv
 * @address 0x1DABD80
 * @size 0x14C
 * @unknownret
 */
void LoadBaseTexture();

/**
 * @mangled LoadTempTexture__FPPciPc
 * @address 0x1DABED0
 * @size 0x120
 */
int LoadTempTexture(char **files, int block, char *buffer);

/**
 * @mangled SetTempTexture__FiPc
 * @address 0x1DABFF0
 * @size 0x74
 * @unknownret
 */
void SetTempTexture(int block, char *name);

/**
 * @mangled MemoryMapDump__Fv
 * @address 0x1DAC070
 * @size 0x14C
 * @unknownret
 */
void MemoryMapDump();

/**
 * @mangled GameInit__Fv
 * @address 0x1DAC1C0
 * @size 0x17B8
 * @unknownret
 */
void GameInit();

/**
 * @mangled GameLoop__Fv
 * @address 0x1DAD980
 * @size 0x2BC
 * @unknownret
 */
int GameLoop();

/**
 * @mangled Draw_MainUnitShadow__Fv
 * @address 0x1DADC40
 * @size 0x1D0
 * @unknownret
 */
void Draw_MainUnitShadow();

/**
 * @mangled Draw_MainUnit__Fv
 * @address 0x1DADE10
 * @size 0x490
 * @unknownret
 */
void Draw_MainUnit();

/**
 * @mangled MainDraw__Fv__3
 * @address 0x1DAE2A0
 * @size 0x25F8
 * @unknownret
 * @note disambiguated by disassembler ("__3" suffix); real retail name has no suffix
 */
void MainDraw();

/**
 * @mangled MoveChara__Fv__2
 * @address 0x1DB08A0
 * @size 0x6BA8
 * @unknownret
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
void MoveChara();

/**
 * @mangled motionDrive__Fv
 * @address 0x1DB7450
 * @size 0x1338
 * @unknownret
 */
void motionDrive();

/**
 * @mangled BtCleatRandomMap__Fv
 * @address 0x1DB8790
 * @size 0xA24
 * @unknownret
 */
void BtCleatRandomMap();

/**
 * @mangled BtCleatFreeMap__Fv
 * @address 0x1DB91C0
 * @size 0x120
 * @unknownret
 */
void BtCleatFreeMap();

/**
 * Clears the monsters that the player can see and spreads them over the floor again.
 *
 * @mangled BtArrengeMonstor__Fv
 * @address 0x1DB92E0
 * @size 0x48
 */
void BtArrengeMonstor();

/**
 * Loads the monsters assigned to the current floor.
 *
 * @mangled BtLoadMonstor__Fi
 * @address 0x1DB9330
 * @size 0x25C
 */
void BtLoadMonstor(int ura);

/**
 * @mangled EquipReAttach__FP10CCharacteri
 * @address 0x1DB9590
 * @size 0xC8
 * @unknownret
 */
void EquipReAttach(CCharacter *equipment, int held_out);

/**
 * @mangled EquipWeaponFrame__FP10CCharacterii
 * @address 0x1DB9660
 * @size 0x1E0
 * @unknownret
 */
void EquipWeaponFrame(CCharacter *weapon, int chara, int held_out);

/**
 * Loads a character's crash, default and equipped weapon models, releasing the
 * old weapon textures first on a reload.
 *
 * @mangled LoadWeapon2__FPUiPUiPUiii
 * @address 0x1DB9840
 * @size 0x2F0
 */
void LoadWeapon2(unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data, int chara, int reload);

/**
 * @mangled SwordDmgCheck1__Ffi
 * @address 0x1DB9B30
 * @size 0x2C8
 * @unknownret
 */
void SwordDmgCheck1(float amount, int kind);

/**
 * Colours the trail of the player's weapon from the element that the weapon leans on.
 *
 * @mangled SetWeaponColor__Fv
 * @address 0x1DB9E00
 * @size 0x260
 */
void SetWeaponColor();

/**
 * @mangled Get_Main_EffectPtr__Fii
 * @address 0x1DBA060
 * @size 0x1CC
 * @unknownret
 */
BT_SHOT_EFFECT *Get_Main_EffectPtr(int chara, int form);

/**
 * @mangled MainChara_Effect__FP14BT_SHOT_EFFECTPUii
 * @address 0x1DBA230
 * @size 0x18C
 * @unknownret
 */
void MainChara_Effect(BT_SHOT_EFFECT *effect, unsigned int *data, int reload);

/**
 * @mangled LoadChara2__FiiPUiPUiPUiPUi
 * @address 0x1DBA3C0
 * @size 0x568
 * @unknownret
 */
void LoadChara2(int chara, int keep_place, unsigned int *chara_data, unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data);

/**
 * @mangled BtCheckDamageProc__Fv
 * @address 0x1DBAFD0
 * @size 0xF0C
 */
int BtCheckDamageProc();

/**
 * @mangled BattleActionThlow__Fv
 * @address 0x1DBBEE0
 * @size 0x720
 * @unknownret
 */
void BattleActionThlow();

/**
 * @mangled BattleActionDrink__Fv
 * @address 0x1DBC600
 * @size 0x294
 * @unknownret
 */
void BattleActionDrink();

/**
 * Starts Xiao's gun action, if the player is free and the gauge is full.
 *
 * @mangled BattleActionOn_Jinn__Fv
 * @address 0x1DBC8A0
 * @size 0x8C
 */
void BattleActionOn_Jinn();

/**
 * @mangled BattleActionPlay_Jinn__FP10CCharacteri
 * @address 0x1DBC930
 * @size 0x5CC
 * @unknownret
 */
void BattleActionPlay_Jinn(CCharacter *chara, int aimed);

/**
 * Starts Ruby's magic action, if the player is free and the gauge is full.
 *
 * @mangled BattleActionOn_Ruby__Fv
 * @address 0x1DBCF00
 * @size 0x78
 */
void BattleActionOn_Ruby();

/**
 * @mangled BattleActionShotRuby__FP10CCharacterifi
 * @address 0x1DBCF80
 * @size 0x3CC
 * @unknownret
 */
void BattleActionShotRuby(CCharacter *chara, int aimed, float scale, int repeat);

/**
 * @mangled BattleActionPlay_Ruby__FP10CCharacteri
 * @address 0x1DBD350
 * @size 0x7F0
 * @unknownret
 */
void BattleActionPlay_Ruby(CCharacter *chara, int aimed);

/**
 * Starts Osmond's action, if the player is free and nothing blocks it.
 *
 * @mangled BattleActionOn_Ozumond__Fv
 * @address 0x1DBDB40
 * @size 0x8C
 */
void BattleActionOn_Ozumond();

/**
 * Starts Osmond's charged action, if the player is free and the gauge is full.
 *
 * @mangled BattleActionOn_Ozumond_H__Fv
 * @address 0x1DBDBD0
 * @size 0xA0
 */
void BattleActionOn_Ozumond_H();

/**
 * Starts Osmond's flight action, if the player is free and nothing blocks it.
 *
 * @mangled BattleActionOn_Ozumond_F__Fv
 * @address 0x1DBDC70
 * @size 0x80
 */
void BattleActionOn_Ozumond_F();

/**
 * @mangled BattleActionPlay_Ozumond__Fi
 * @address 0x1DBDCF0
 * @size 0x3C4
 * @unknownret
 */
void BattleActionPlay_Ozumond(int aimed);

/**
 * @mangled BattleActionPlay_Ozumond_H__Fi
 * @address 0x1DBE0C0
 * @size 0x2D8
 * @unknownret
 */
void BattleActionPlay_Ozumond_H(int aimed);

/**
 * @mangled BattleActionPlay_Ozumond_F__Fi
 * @address 0x1DBE3A0
 * @size 0x3C0
 * @unknownret
 */
void BattleActionPlay_Ozumond_F(int aimed);

/**
 * @mangled autoCamTrial__Fv
 * @address 0x1DBE970
 * @size 0x1054
 * @unknownret
 */
void autoCamTrial();

/**
 * @mangled DelActiveItem__Fi
 * @address 0x1DBF9D0
 * @size 0x98
 * @unknownret
 */
void DelActiveItem(int slot);

/**
 * Applies the trap selected by a dungeon-floor trap circle.
 *
 * @mangled Run_TrapCircle__FP15MAP_TRAP_CIRCLE
 * @address 0x1DBFA70
 * @size 0x264
 */
int Run_TrapCircle(MAP_TRAP_CIRCLE *trap);

/**
 * Drops the lock-on, and with it the cursor and the life gauge.
 *
 * @mangled LockOffTargte__Fv
 * @address 0x1DBFCE0
 * @size 0x28
 */
void LockOffTargte();

/**
 * Draws the life gauge of the enemy that the player has locked on to.
 *
 * @mangled DrawTargetLife__Fv
 * @address 0x1DBFD10
 * @size 0x1B4
 */
void DrawTargetLife();

/**
 * @mangled DrawtargetCursor__FPffff
 * @address 0x1DBFED0
 * @size 0x288
 * @unknownret
 */
void DrawtargetCursor(float *world, float width, float height, float alpha);

/**
 * @mangled SetNearLockOnTarget__Fii
 * @address 0x1DC0160
 * @size 0x638
 */
int SetNearLockOnTarget(int from, int nearest_only);

/**
 * @mangled setTargetCursor__Fi
 * @address 0x1DC07A0
 * @size 0x6DC
 * @unknownret
 */
void setTargetCursor(int on);

/**
 * Turns the player's model to face the way a blow throws them.
 *
 * @mangled unitBlowActionRot__FPf
 * @address 0x1DC0E80
 * @size 0x94
 */
void unitBlowActionRot(float *velocity);

/**
 * @mangled setUnitDamageColor__Fi
 * @address 0x1DC0F20
 * @size 0xCC
 */
float setUnitDamageColor(int hit);

/**
 * Stops the ambient-colour animation on the player's model.
 *
 * @mangled RsetUnitAmbientAnime__Fv
 * @address 0x1DC0FF0
 * @size 0xC
 */
void RsetUnitAmbientAnime();

/**
 * Starts an ambient-colour animation on the player's model.
 *
 * @mangled setUnitAmbientAnime__Ffffff
 * @address 0x1DC1000
 * @size 0x50
 */
void setUnitAmbientAnime(float speed, float time, float red, float green, float blue);

/**
 * Steps the ambient-colour animation and writes the colour it reaches.
 *
 * @mangled unitAmbientAnime__FPf
 * @address 0x1DC1050
 * @size 0x12C
 */
int unitAmbientAnime(float *colour);

/**
 * @mangled LoadStartLogo__Fi
 * @address 0x1DC1180
 * @size 0x110
 */
int LoadStartLogo(int map);

/**
 * Brings the floor title on screen.
 *
 * @mangled FloorTitleOn__Fv
 * @address 0x1DC1290
 * @size 0x24
 */
void FloorTitleOn();

/**
 * Takes the mini-map away, or gives back what it drew before.
 *
 * @mangled SetMIniMapStatus__Fi
 * @address 0x1DC12C0
 * @size 0x40
 */
void SetMIniMapStatus(int on);

/**
 * Puts the player back into the state a floor starts in.
 *
 * @mangled ResetStatusInfo__Fv
 * @address 0x1DC1300
 * @size 0xF8
 */
void ResetStatusInfo();

/**
 * Takes away the push that the player's action gives them.
 *
 * @mangled ResetMovePower__Fv
 * @address 0x1DC1400
 * @size 0x18
 */
void ResetMovePower();

/**
 * @mangled LoaderInit__Fv
 * @address 0x1DC1420
 * @size 0xE4
 * @unknownret
 */
void LoaderInit();

/**
 * @mangled LoaderLoop__Fv
 * @address 0x1DC1510
 * @size 0x278
 */
int LoaderLoop();

/**
 * Points the first-person camera the way the player faces.
 *
 * @mangled InitEyeCamera__Fv
 * @address 0x1DC1790
 * @size 0x48
 */
void InitEyeCamera();

/**
 * @mangled EyeCamera__Fv
 * @address 0x1DC17E0
 * @size 0x2A0
 * @unknownret
 */
void EyeCamera();

/**
 * Carries the Georama editor's own fade on by one step.
 *
 * @mangled EdFadeInOut__Fv
 * @address 0x189860
 * @size 0x1B0
 * @unknownret
 */
void EdFadeInOut();

/**
 * Blurs whatever lies outside the depth the focus names.
 *
 * @mangled DepthOfField__FPfiii
 * @address 0x1652C0
 * @size 0x9D0
 * @unknownret
 */
void DepthOfField(float *focus, int level, int alpha, int blur);

/**
 * Draws the bee that follows one of the walkers.
 *
 * @mangled DrawBee__FP6CFramei
 * @address 0x1D9590
 * @size 0x270
 * @unknownret
 */
void DrawBee(CFrame *frame, int count);

/**
 * Draws the event battle's own overlay.
 *
 * @mangled EBDraw__Fv
 * @address 0x168B80
 * @size 0x560
 */
void EBDraw();

/**
 * Draws the Georama editor's debug text.
 *
 * @mangled EdDDrawFont__Fv
 * @address 0x170270
 * @size 0xB0
 * @unknownret
 */
void EdDDrawFont();

/**
 * Draws the debug overlay.
 *
 * @mangled DebugInfomationDraw__Fv
 * @address 0x1B3780
 * @size 0xF70
 * @unknownret
 */
void DebugInfomationDraw();

/**
 * Carries the system message on by one step.
 *
 * @mangled SystemMesStep__Fv
 * @address 0x160120
 * @size 0xB4
 * @unknownret
 */
void SystemMesStep();

/**
 * Draws the system message.
 *
 * @mangled SystemMesDraw__Fv
 * @address 0x1601E0
 * @size 0xB0
 * @unknownret
 */
void SystemMesDraw();

/**
 * Gives back the floor number that one of the deeper dungeons shows.
 *
 * @mangled BtGetFloorLevel__Fi
 * @address 0x1B77C0
 * @size 0x3C
 */
int BtGetFloorLevel(int floor);

/**
 * Copies the textures the item list holds into the frame it draws from.
 *
 * @mangled DngActiveItemTextureCopy__Fv
 * @address 0x22A5A0
 * @size 0x104
 * @unknownret
 */
int DngActiveItemTextureCopy();

/**
 * The same, for the weapon the player holds.
 *
 * @mangled DngActiveWeaponTextureCopy__Fv
 * @address 0x22A6B0
 * @size 0x130
 * @unknownret
 */
int DngActiveWeaponTextureCopy();

/** The weapon models the player's character can hold. */
extern CCharacter DefaultWeapon;

/** The weapon models the player's character can hold. */
extern CCharacter MainWeapon;

/**
 * Effect the player's weapon is running.
 */
extern CSHOT_EFFECT *NowMainEffect;

/** The effect the player's own character breaks things with. */
extern CSHOT_EFFECT CharaMainEffectCrash;

/** Whether the player's character holds an item out. */
extern s32 CharaMainHandViewFlag;

/**
 * Holds the party where it stands instead of running its movement step.
 */
extern int driveStepHold;

/**
 * Freezes the drawn frame so a menu can open over it.
 */
extern int frameCaputer;

/**
 * Item the small item-select window returned, or -1.
 */
extern int miniItemSelNo;

/**
 * Mode the battle loop runs its menus in. @see BtGameMode.
 */
extern int BtGameModeFlag;

/**
 * Event the dungeon loop is running, or -1 when none is.
 */
extern int iventInfo;

/**
 * Stops the monster units stepping.
 */
extern int CMonUnitHold;

/**
 * Stops the effects stepping.
 */
extern int CEffectHold;

/**
 * Model of the item a pickup presentation is showing.
 */
extern int itemOpenItemMds;

/**
 * Texture of the item a pickup presentation is showing.
 */
extern int itemOpenItemImg;

/**
 * Hides the monster units instead of drawing them.
 */
extern int CMonUnitHyde;

/**
 * Hides the effects instead of drawing them.
 */
extern int CEffectHyde;

/**
 * Arena the battle item cache is carved out of.
 */
extern CDataAlloc2<1> BtCashBuffer;

/** The trail that the player's weapon leaves as it swings. */
extern CWeaponEffect CWeaponFx;

/**
 * Weapon model the player currently holds.
 */
extern CCharacter *NowWeapon;

/** Whether the item list the menu caches is still good. */
extern s32 BtItemListCashFlag;

/**
 * Model a treasure box's contents are drawn with.
 */
extern CFrame *itemBoxModel;

/** The model the item a gate holds draws with. */
extern s32 itemOpenBigFlag;

/** Whether the Atlamillia message board is up. */
extern s32 itemOpenSmallFlag;

/** The model the item a gate holds draws with. */
extern float itemWeponScale;

/** Whether the Atlamillia message board is up. */
extern float itemNormalScale;

/** Which event the player is standing on, or -1 for none. */
extern s32 iventActive;

/**
 * Camera the dungeon cuts to for events.
 */
extern CCameraFollow SubCamera;

/**
 * Camera that follows the player through the dungeon.
 */
extern CCameraFollow MainCamera__4;

/**
 * Camera the dungeon is drawn through.
 */
extern CCameraFollow *NowCamera__3;

/** Which short Atlamillia use is running. */
extern s32 atraShortGetType;

/** Which short Atlamillia use is running. */
extern s32 atraGetStatus;

/** Whether the Atlamillia message board is up. */
extern s32 atraGetMsgBord;

/** Whether the Atlamillia message board is up. */
extern float atraGetMsgBordRate;

/** Where the player took the Atlamillia they last picked up. */
extern sceVu0FVECTOR atraGetPos;

/** Which way the player faced as they took it. */
extern sceVu0FVECTOR atraGetRot;

/** The fade the dungeon draws over the picture. */
extern CDispCtrl DispFade__3;

/** Whether the player's model skips interpolation on its next drive step. */
extern s32 driveNoInterpolate;

/** Whether the player is escaping the dungeon. */
extern s32 EscapeFlag;

/**
 * Model data of the item a pickup presentation is showing.
 */
extern u_int *itemOpenItemChr;

/**
 * Model data of the short Atlamillia effect.
 */
extern u_int *shortAtraEffectPtr;

/** The models of the items the player is running. */
extern CActiveItemPack activeItem;

/** The models an opened treasure box shows, and the effects that play over them. */
extern CMotionModel itemOpenBig;

/** The models an opened treasure box shows, and the effects that play over them. */
extern CMotionModel itemOpenBigFx;

/** The models an opened treasure box shows, and the effects that play over them. */
extern CMotionModel itemOpenSmall;

/** The models an opened treasure box shows, and the effects that play over them. */
extern CMotionModel itemOpenSmallFx;

/** The effect that a short Atlamillia use lights the player with. */
extern CCharacter shortAtraEffect;

/** The effect that plays as the player escapes the dungeon. */
extern CCharacter EscapeEffect;

/** Randomized element the current floor's sealed door wants. */
extern int BtRubyDoorKey;

/** Nonzero while the dungeon floor is drawn at all. */
extern s32 BtAllDrawFlag;

/** The dungeon floor that the player is on. */
extern CDungeonMap MainDungeonMap;

/** The events of the floor that the player is on. */
extern CDungeonEventMan DngEventMan;

/** The events of the back dungeon's floor. */
extern CDungeonEventMan UraEventMan;

/** The characters that walk the dungeon alongside the player. */
extern CNPCharacter NPCUnit[];

/** Nonzero while a system event script holds the dungeon. */
extern s32 BtEventMode;

/** Nonzero while a system event script clears the dungeon's usual work. */
extern s32 BtAllClear;

/** Message window the system script talks through first. */
extern ClsMes BtEventMes0;

/** Message window the system script talks through second. */
extern ClsMes BtEventMes1;

/** File buffer the current floor's system script is read into. */
extern CDataAlloc2<1> BtSystemScriptFileBuffer;

/** Base address of the system script data currently loaded. */
extern s32 BtEventData;

/** Whether an enemy is locked on. */
extern s32 lockOnTargetFlag;

/** Which button fires an action. */
extern s32 PadInput_OK;

/** The debug text window the dungeon draws. */
extern CDebugFont DbgMsg;

/** One texture animation for every part of every character in the dungeon. */
extern CTexAnimeData BtNPCTexAnimeData[][32];

/** The model a trap circle draws with. */
extern CCharacter Trap_Circle;

/**
 * Bomb effect the thrown item is playing.
 */
extern CItemBombEffect *NowBombEffect;

/**
 * Shock wave the current attack is playing.
 */
extern CShockWave *NowShockWave;

/** Scratch arena used while loading the current map. */
extern CDataAlloc2<1> MapModelBuffer;

/** Texture animation state initialized after the ground texture archive loads. */
extern CTextureAnime BtTexAnime;

/** The texture animations every dungeon texture runs. */
extern CTexAnimeData BtTexAnimeData[];

/** Scene-wide values filled from model-definition directives. */
extern sceVu0FMATRIX main_light;

/** The lighting the dungeon draws the field and the models under. */
extern sceVu0FMATRIX main_lightcolor;

/** The lighting the dungeon draws the field and the models under. */
extern sceVu0FMATRIX sub_light;

/** The lighting the dungeon draws the field and the models under. */
extern sceVu0FMATRIX sub_lightcolor;

/** The lighting the dungeon draws the field and the models under. */
extern sceVu0FVECTOR main_ambientlight;

/** The lighting the dungeon draws the field and the models under. */
extern sceVu0FVECTOR sub_ambientlight;

/** How far the fog reaches and how it thickens, on the front and the back floors. */
extern float main_fogRate[4];

/** How far the fog reaches and how it thickens, on the front and the back floors. */
extern float sub_fogRate[4];

/** The colour the fog draws, on the front and the back floors. */
extern u8 main_fogColor[];

/** The colour the fog draws, on the front and the back floors. */
extern u8 sub_fogColor[];

/** The events of the floor the dungeon is drawing. */
extern u8 main_bgColor[];

/** The events of the floor the dungeon is drawing. */
extern u8 sub_bgColor[];

/** The buffers the dungeon's item cache and battle scripts run out of. */
extern CDataAlloc2<1> BtItemCashArea[6];

/**
 * Arena the steeb message file is read into.
 */
extern CDataAlloc2<1> BtSteebMesBuffer;

/**
 * Arena the monsters load into, after whatever the map used.
 */
extern CDataAlloc2<1> MonstorModelBuffer;

/**
 * Messages the dungeon's steeb shows.
 */
extern ClsMes DngMesStb;

/**
 * Motion information LoadPack fills for the small item-get box.
 */
extern MOTION_INFO itemOpenSmall_info;

/**
 * Motion information LoadPack fills for the big item-get box.
 */
extern MOTION_INFO itemOpenBig_info;

/**
 * The healing particles that play in water.
 */
extern CHealEffect HealEffect;

/**
 * Whether the water splash is active.
 */
extern int Water_Splash_actFlag;

/**
 * The splash shown where the party enters the water.
 */
extern CCharacter Water_Splash;

/**
 * The dungeon's monster units.
 */
extern CMonstorUnit MainMonstorUnit;

/**
 * Effects of the five elemental gems.
 */
extern CSHOT_EFFECT MasekiEffect[];
