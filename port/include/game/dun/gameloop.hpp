#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc.hpp"
#include "hit_machingun_effect.hpp"
#include "hitmark.hpp"
#include "runscript.hpp"
#include "textureanime.hpp"

namespace dc::game {

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
 * What DunMoveChara is doing, as gameTask holds it; each INIT task is followed by its LOOP task.
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

class CCamera;
class CCharacter;
class CCameraFollow;
class CFrameVu1;
class CRunEffect;
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

/**
 * Player character.
 */
extern "C" CCharacter CharaMain;

/**
 * Character currently controlled by the player.
 */
extern s32 nowUnitNow;

/**
 * Debug switches shared by dungeon rendering.
 */
extern "C" s32 DebugStatus[21];

/**
 * Message window the dungeon shows.
 */
extern "C" CDngMessageMan DngMessMan;

/**
 * Item the player is stealing back from a monster.
 */
extern "C" CStealItem StealItem;

/**
 * Item count animation of the dungeon's menus.
 */
extern CMenuItemStep ItemVolumeStep;

/**
 * Model used to draw the monster cursor.
 */
extern CFrame *cursorFrame;

/**
 * Dungeon the player is in.
 */
extern s32 selectMapNo;

/**
 * Items lying on the floor.
 */
extern CRandomItem *RandomItem;

/**
 * Whether the player is currently using the movement speed boost.
 */
extern s32 BtBySpeedFlag;

/**
 * Whether the party is in the back area of the current dungeon floor.
 */
extern s32 BtUraDongeon;

/**
 * Whether the player has the current floor map.
 */
extern s32 BtEquipMap;

/**
 * Whether the player has the magical crystal that reveals monsters on the minimap.
 */
extern s32 BtEquipMasuisyou;

/**
 * Map of the floor the player is on.
 */
extern CDungeonMap *NowDngMap;

/**
 * Drainage-field set used by the current dungeon floor.
 */
extern CDranMapField *NowDranMapField;

/**
 * Damage numbers the dungeon shows.
 */
extern CHitValue *NowHitValue;

/**
 * Stores the machine-gun hit flashes used by dungeon projectile effects.
 */
extern "C" CHIT_MACHINGUN_EFFECT OzumondShotEffect;

/**
 * Projectile effects the dungeon has running.
 */
extern CSHOT_EFFECT_PACK *NowShotEffect;

/**
 * Hits the collision test found this frame.
 */
extern CCollisionData *NowColData;

/**
 * Weapon currently equipped by the player character.
 */
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
 * Loads the textures every dungeon floor draws with.
 */
void LoadBaseTexture();

/**
 * Reads a null-terminated list of texture files one after another into a buffer and loads them into a
 * texture block, returning the bytes read.
 */
int LoadTempTexture(char **files, int block, char *buffer);

/**
 * Loads one texture image already in memory into a texture block, replacing what the block held.
 */
void SetTempTexture(int block, char *name);

/**
 * Prints how much of each dungeon model and data arena is in use.
 */
void MemoryMapDump();

/**
 * Sets up the dungeon: its arenas, cameras, message windows, the player's models and the floor's system
 * script.
 */
void GameInit();

/**
 * Runs the dungeon for one frame and returns nonzero once the player leaves it.
 */
int GameLoop();

/**
 * Draws the shadows of the player, the monsters and, during events, the walkers, under a dimmed light.
 */
void Draw_MainUnitShadow();

/**
 * Draws the player's model and weapon under the lighting their status and the Atlamillia give them.
 */
void Draw_MainUnit();

/**
 * Draws one frame of the dungeon.
 */
void DunMainDraw();

/**
 * Runs the dungeon for one frame. gameTask holds the GameTask being run; every task ends by stepping
 * the models.
 */
void DunMoveChara();

/**
 * Steps the dungeon's models, effects and gauges once the player's input has been handled.
 */
void motionDrive();

/**
 * Builds a new random floor and its back floor, clearing what the last floor left behind.
 */
void BtCleatRandomMap();

/**
 * Sets up the events and items of the fixed floor that is loaded.
 */
void BtCleatFreeMap();

/**
 * Clears the monsters that the player can see and spreads them over the floor again.
 */
void BtArrengeMonstor();

/**
 * Loads the monsters assigned to the current floor, or to its back floor when ura is nonzero.
 */
void BtLoadMonstor(int ura);

/**
 * Hangs a model off the player's weapon hand, or off the raised hand when held_out is nonzero.
 */
void EquipReAttach(CCharacter *equipment, int held_out);

/**
 * Makes a weapon model the one the player holds, hangs it off their hand and resets its trail.
 */
void EquipWeaponFrame(CCharacter *weapon, int chara, int held_out);

/**
 * Loads a character's broken, default and equipped weapon models, releasing the old weapon textures
 * first on a reload.
 */
void LoadWeapon2(unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data, int chara, int reload);

/**
 * Wears the player's weapon down after a hit, swapping in the broken or default model if it breaks.
 */
void SwordDmgCheck1(float amount, int kind);

/**
 * Colours the trail of the player's weapon from the element that the weapon leans on.
 */
void SetWeaponColor();

/**
 * Gives back the shot effect a character fires, picked by Ruby's form or Osmond's weapon (which also
 * sets his gun type), or nullptr for an unknown character.
 */
BT_SHOT_EFFECT *Get_Main_EffectPtr(int chara, int form);

/**
 * Loads the player's shot effect, and the crash effect Osmond and Ruby fall back on, from effect data.
 */
void MainChara_Effect(BT_SHOT_EFFECT *effect, unsigned int *data, int reload);

/**
 * Loads a character and their weapons; unless keep_place is nonzero, the model stays where the old one
 * stood.
 */
void LoadChara2(int chara, int keep_place, unsigned int *chara_data, unsigned int *crash_data, unsigned int *default_data, unsigned int *main_data);

/**
 * Applies the damage that reached the player this frame; returns nonzero when they took any.
 */
int BtCheckDamageProc();

/**
 * Runs the throw of an active item: the player walks its landing point around, then throws it.
 */
void BattleActionThlow();

/**
 * Runs the drinking of an active item, spending it once the motion ends.
 */
void BattleActionDrink();

/**
 * Starts Xiao's slingshot action, if the player is free and the gauge is full.
 */
void BattleActionOn_Jinn();

/**
 * Runs Xiao's slingshot action; aimed is nonzero in the first-person view.
 */
void BattleActionPlay_Jinn(CCharacter *chara, int aimed);

/**
 * Starts Ruby's magic action, if the player is free and the gauge is full.
 */
void BattleActionOn_Ruby();

/**
 * Fires one of Ruby's magic shots at the given strength; a repeat follows the last shot's path.
 */
void BattleActionShotRuby(CCharacter *chara, int aimed, float scale, int repeat);

/**
 * Runs Ruby's magic action, which charges while the button stays held.
 */
void BattleActionPlay_Ruby(CCharacter *chara, int aimed);

/**
 * Starts Osmond's machine gun, if the player is free and nothing blocks it.
 */
void BattleActionOn_Ozumond();

/**
 * Starts Osmond's charged action, if the player is free and the gauge is full.
 */
void BattleActionOn_Ozumond_H();

/**
 * Starts Osmond's flame action, if the player is free and nothing blocks it.
 */
void BattleActionOn_Ozumond_F();

/**
 * Runs Osmond's machine gun while the button is held and the gauge lasts.
 */
void BattleActionPlay_Ozumond(int aimed);

/**
 * Runs Osmond's charged shot.
 */
void BattleActionPlay_Ozumond_H(int aimed);

/**
 * Runs Osmond's flame while the button is held and the gauge lasts.
 */
void BattleActionPlay_Ozumond_F(int aimed);

/**
 * Keeps the camera clear of the floor's walls and turns it towards a locked-on monster.
 */
void autoCamTrial();

/**
 * Spends one use of an active item slot, dropping its model on the last use.
 */
void DelActiveItem(int slot);

/**
 * Applies the trap selected by a dungeon-floor trap circle.
 */
int Run_TrapCircle(MAP_TRAP_CIRCLE *trap);

/**
 * Drops the lock-on, and with it the cursor and the life gauge.
 */
void LockOffTargte();

/**
 * Draws the life gauge of the enemy that the player has locked on to.
 */
void DrawTargetLife();

/**
 * Draws the four corners of the lock-on cursor around a point, then the target's life gauge.
 */
void DrawtargetCursor(float *world, float width, float height, float alpha);

/**
 * Locks on to a monster in front of the player, the nearest or the next one after from, and returns how
 * many could be locked on to.
 */
int SetNearLockOnTarget(int from, int nearest_only);

/**
 * Keeps the lock-on cursor on its monster while on is nonzero, dropping it once the monster is out of
 * reach; otherwise looks for the nearest monster.
 */
void setTargetCursor(int on);

/**
 * Turns the player's model to face the way a blow throws them.
 */
void unitBlowActionRot(float *velocity);

/**
 * Steps the flash a hit gives the player's model and returns its brightness; a nonzero hit restarts it.
 */
float setUnitDamageColor(int hit);

/**
 * Stops the ambient-colour animation on the player's model.
 */
void RsetUnitAmbientAnime();

/**
 * Starts an ambient-colour animation on the player's model.
 */
void setUnitAmbientAnime(float speed, float time, float red, float green, float blue);

/**
 * Steps the ambient-colour animation and writes the colour it reaches.
 */
int unitAmbientAnime(float *colour);

/**
 * Loads the dungeon's title texture and finds its floor number texture, returning the quadwords used.
 */
int LoadStartLogo(int map);

/**
 * Brings the floor title on screen.
 */
void FloorTitleOn();

/**
 * Takes the mini-map away, or gives back what it drew before.
 */
void SetMIniMapStatus(int on);

/**
 * Puts the player back into the state a floor starts in.
 */
void ResetStatusInfo();

/**
 * Takes away the push that the player's action gives them.
 */
void ResetMovePower();

/**
 * Sets up the debug map loader screen.
 */
void LoaderInit();

/**
 * Runs the debug map loader screen for one frame and returns nonzero once a map is chosen.
 */
int LoaderLoop();

/**
 * Points the first-person camera the way the player faces.
 */
void InitEyeCamera();

/**
 * Runs the first-person camera for one frame.
 */
void EyeCamera();

/**
 * Carries the Georama editor's own fade on by one step.
 */
void EdFadeInOut();

/**
 * Blurs whatever lies outside the depth the focus names.
 */
void DepthOfField(float *focus, int level, int alpha, int blur);

/**
 * Draws the bee that follows one of the walkers.
 */
void DrawBee(CFrame *frame, int count);

/**
 * Draws the event battle's own overlay.
 */
void EBDraw();

/**
 * Draws the Georama editor's debug text.
 */
void EdDDrawFont();

/**
 * Draws the debug overlay.
 */
void DebugInfomationDraw();

/**
 * Carries the system message on by one step.
 */
void SystemMesStep();

/**
 * Draws the system message.
 */
void SystemMesDraw();

/**
 * Gives back the floor number that one of the deeper dungeons shows.
 */
int BtGetFloorLevel(int floor);

/**
 * Copies the textures the item list holds into the frame it draws from.
 */
int DngActiveItemTextureCopy();

/**
 * Copies the texture of the weapon the player holds into the frame the item list draws from.
 */
int DngActiveWeaponTextureCopy();

/**
 * The model of the character's default weapon.
 */
extern CCharacter DefaultWeapon;

/**
 * The model of the weapon the player has equipped.
 */
extern CCharacter MainWeapon;

/**
 * Effect the player's weapon is running.
 */
extern CSHOT_EFFECT *NowMainEffect;

/**
 * The effect the player's own character breaks things with.
 */
extern CSHOT_EFFECT CharaMainEffectCrash;

/**
 * Whether the player's character holds an item out.
 */
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

/**
 * The trail that the player's weapon leaves as it swings.
 */
extern CWeaponEffect CWeaponFx;

/**
 * Weapon model the player currently holds.
 */
extern CCharacter *NowWeapon;

/**
 * Whether the item list the menu caches is still good.
 */
extern s32 BtItemListCashFlag;

/**
 * Model a treasure box's contents are drawn with.
 */
extern CFrame *itemBoxModel;

/**
 * Whether the large treasure box presentation is running.
 */
extern s32 itemOpenBigFlag;

/**
 * Whether the small treasure box presentation is running.
 */
extern s32 itemOpenSmallFlag;

/**
 * Scale a weapon shown by a treasure box presentation draws at.
 */
extern float itemWeponScale;

/**
 * Scale any other item shown by a treasure box presentation draws at.
 */
extern float itemNormalScale;

/**
 * Which event the player is standing on, or -1 for none.
 */
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

/**
 * Which short Atlamillia use is running.
 */
extern s32 atraShortGetType;

/**
 * Whether the Atlamillia pickup is lighting the player.
 */
extern s32 atraGetStatus;

/**
 * Whether the Atlamillia message board is up.
 */
extern s32 atraGetMsgBord;

/**
 * How far the Atlamillia message board has faded in.
 */
extern float atraGetMsgBordRate;

/**
 * Where the player took the Atlamillia they last picked up.
 */
extern sceVu0FVECTOR atraGetPos;

/**
 * Which way the player faced as they took it.
 */
extern sceVu0FVECTOR atraGetRot;

/**
 * The fade the dungeon draws over the picture.
 */
extern CDispCtrl DispFade__3;

/**
 * Whether the player's model skips interpolation on its next drive step.
 */
extern s32 driveNoInterpolate;

/**
 * Whether the player is escaping the dungeon.
 */
extern s32 EscapeFlag;

/**
 * Model data of the item a pickup presentation is showing.
 */
extern u_int *itemOpenItemChr;

/**
 * Model data of the short Atlamillia effect.
 */
extern u_int *shortAtraEffectPtr;

/**
 * The models of the items the player is running.
 */
extern CActiveItemPack activeItem;

/**
 * The model a large treasure box opens with.
 */
extern CMotionModel itemOpenBig;

/**
 * The effect that plays over a large treasure box as it opens.
 */
extern CMotionModel itemOpenBigFx;

/**
 * The model a small treasure box opens with.
 */
extern CMotionModel itemOpenSmall;

/**
 * The effect that plays over a small treasure box as it opens.
 */
extern CMotionModel itemOpenSmallFx;

/**
 * The effect that a short Atlamillia use lights the player with.
 */
extern CCharacter shortAtraEffect;

/**
 * The effect that plays as the player escapes the dungeon.
 */
extern CCharacter EscapeEffect;

/**
 * Randomized element the current floor's sealed door wants.
 */
extern int BtRubyDoorKey;

/**
 * Nonzero while the dungeon floor is drawn at all.
 */
extern s32 BtAllDrawFlag;

/**
 * The dungeon floor that the player is on.
 */
extern CDungeonMap MainDungeonMap;

/**
 * The events of the floor that the player is on.
 */
extern CDungeonEventMan DngEventMan;

/**
 * The events of the back dungeon's floor.
 */
extern CDungeonEventMan UraEventMan;

/**
 * The characters that walk the dungeon alongside the player.
 */
extern CNPCharacter NPCUnit[];

/**
 * Nonzero while a system event script holds the dungeon.
 */
extern s32 BtEventMode;

/**
 * Nonzero while a system event script clears the dungeon's usual work.
 */
extern s32 BtAllClear;

/**
 * Message window the system script talks through first.
 */
extern ClsMes BtEventMes0;

/**
 * Message window the system script talks through second.
 */
extern ClsMes BtEventMes1;

/**
 * File buffer the current floor's system script is read into.
 */
extern CDataAlloc2<1> BtSystemScriptFileBuffer;

/**
 * Whether an enemy is locked on.
 */
extern s32 lockOnTargetFlag;

/**
 * Which button fires an action.
 */
extern s32 PadInput_OK;

/**
 * The debug text window the dungeon draws.
 */
extern CDebugFont DbgMsg;

/**
 * The texture animations of each character that walks the dungeon.
 */
extern CTexAnimeData BtNPCTexAnimeData[][32];

/**
 * The model a trap circle draws with.
 */
extern CCharacter Trap_Circle;

/**
 * Bomb effect the thrown item is playing.
 */
extern CItemBombEffect *NowBombEffect;

/**
 * Shock wave the current attack is playing.
 */
extern CShockWave *NowShockWave;

/**
 * Scratch arena used while loading the current map.
 */
extern CDataAlloc2<1> MapModelBuffer;

/**
 * Texture animation state initialized after the ground texture archive loads.
 */
extern CTextureAnime BtTexAnime;

/**
 * The texture animations every dungeon texture runs.
 */
extern CTexAnimeData BtTexAnimeData[];

/**
 * The light directions of the front floor.
 */
extern sceVu0FMATRIX main_light;

/**
 * The light colours of the front floor.
 */
extern sceVu0FMATRIX main_lightcolor;

/**
 * The light directions of the back floor.
 */
extern sceVu0FMATRIX sub_light;

/**
 * The light colours of the back floor.
 */
extern sceVu0FMATRIX sub_lightcolor;

/**
 * The ambient light of the front floor.
 */
extern sceVu0FVECTOR main_ambientlight;

/**
 * The ambient light of the back floor.
 */
extern sceVu0FVECTOR sub_ambientlight;

/**
 * How far the fog reaches and how it thickens on the front floor.
 */
extern float main_fogRate[4];

/**
 * How far the fog reaches and how it thickens on the back floor.
 */
extern float sub_fogRate[4];

/**
 * The colour the fog draws on the front floor.
 */
extern u8 main_fogColor[];

/**
 * The colour the fog draws on the back floor.
 */
extern u8 sub_fogColor[];

/**
 * The background colour of the front floor.
 */
extern u8 main_bgColor[];

/**
 * The background colour of the back floor.
 */
extern u8 sub_bgColor[];

/**
 * The arenas the dungeon's item cache runs out of.
 */
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

// Port additions.

/**
 * Defines the life gauge that the locked-on enemy draws above itself.
 */
struct ENEMY_LIFE_GAGE {
    s32 life_max; /**< Specifies the life the enemy has at full health. */
    s32 life;     /**< Specifies the life the enemy has now. */
    s32 x;        /**< Specifies the left edge of the gauge, in screen pixels. */
    s32 y;        /**< Specifies the top edge of the gauge, in screen pixels. */
    s32 on;       /**< Whether an enemy is locked on. */
    s32 draw;     /**< Whether the gauge draws this frame. */
};

STATIC_ASSERT(sizeof(ENEMY_LIFE_GAGE) == 0x18);

/**
 * Names where the item the player is about to throw will land.
 */
struct BOMB_INFO {
    sceVu0FVECTOR pos; /**< Where the thrown item lands. */
    s32           unk_10;
    s32           aiming;     /**< Whether the player is still walking the landing point around. */
    s32           throw_step; /**< Step of the throw: 1 until the item leaves the hand, 2 while the motion ends, -1 when idle. */
    s32           unk_1C;
};

STATIC_ASSERT(sizeof(BOMB_INFO) == 0x20);

/**
 * Names what the dungeon hands the Georama editor to follow.
 */
struct GAME_ENV {
    CFrame  *frame;  /**< Frame that the editor follows. */
    CCamera *camera; /**< Camera that the editor draws through. */
};


/**
 * State of the dungeon game loop that DunMoveChara (dun/movechara.cpp) shares, defined in
 * dun/gameloop.cpp.
 */
extern BOMB_INFO       BombInfo;
extern s32             BtEventItemNo;
extern CDataAlloc2<1>  BtScriptWorkBuffer;
extern CRunEffect      CRunFx__2;
extern CFrameVu1      *CharaFrame;
extern CCharacter      CharaHand;
extern s32             DeadKeyStartWait;
extern s32             DeadKeyWait;
extern ClsMes          DngMes1;
extern ClsMes          DngMes2;
extern ENEMY_LIFE_GAGE EnemyLifeGage;
extern GAME_ENV        GameEnv;
extern CRandomItem     MainRandomItem;
extern s32             MonstorNameOff;
extern CDataAlloc2<1>  MonstorScriptBuffer[16];
extern CHitPointMark   MyHitPointMark[16];
extern CCharacter      NewChangeFx;
extern s32             NewChangeFxFlag;
extern s32             PadInput_NO;
extern CRandomItem     SubRandomItem;
extern float           camera_up_near_dist;
extern CDungeonMap     UraDungeonMap;
extern s32             battleMenuWait;
extern s32             cameraAuto;
extern s32             cameraAutoOld;
extern s32             colPolyNum;
extern s32             defCameraWait;
extern s32             existFlag;
extern s32             exitMenuFlag;
extern s32             gameTask;
extern s32             infoMap;
extern s32             infoMapOld;
extern float           inputH1;
extern s32             itemNowSel;
extern s32             iventMarker;
extern s32             lightingMode;
extern s32             lockOnTargetNo;
extern float           oldCameraAngle;
extern float           oldCameraHeight;
extern s32             oldMsgNo;
extern s32             oldMsgNo2;
extern s32             oldRogoY3;
extern s32             oldUnitNow;
extern sceVu0FVECTOR   ref_off;
extern s32             rogoAlphaA[3];
extern s32             rogoAlphaW[3];
extern s32             rogoSwitch2;
extern s32             rogoY3;
extern s32             startCnt2;
extern float           stickVector2;
extern s32             targetCursorShiftNo;
extern s32             targetCursorShiftRot;
extern s32             tryalExit;
extern sceVu0FVECTOR   velo2;
extern sceVu0FVECTOR   veloOld;
extern s32             viewMode__2;

/**
 * State shared by the dungeon movement and game-loop replacements, defined in the dungeon
 * translation unit.
 */
extern sceVu0FVECTOR blowVelo;
extern int           ruby_effect_id;
extern float         stickVector;
extern sceVu0FVECTOR velo__2;
extern float         viewAngleH__2;
extern float         viewAngleV__2;

/**
 * Writes into velocity the velocity collision hit hit_no pushes the player with; a blowing hit
 * (ready phase 10) pushes straight away from its position.
 */
void HitBlowVelocity(int hit_no, float *velocity);

} // namespace dc::game
