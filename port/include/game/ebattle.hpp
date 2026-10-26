#pragma once

/**
 * Steps of town fishing, as chara_fishing holds them.
 */
// clang-format off
enum EdFishingState {
    ED_FISHING_INIT       = 0,  /**< Starting. */
    ED_FISHING_STAND      = 1,  /**< Standing with the rod. */
    ED_FISHING_CAST_START = 2,  /**< Starting the cast. */
    ED_FISHING_CASTING    = 3,  /**< Casting. */
    ED_FISHING_WAIT_BITE  = 4,  /**< Watching the float. */
    ED_FISHING_REEL_IN    = 5,  /**< Reeling in. */
    ED_FISHING_BITE_CHECK = 6,  /**< Checking for a bite. */
    ED_FISHING_HOOKED     = 7,  /**< Hooked. */
    ED_FISHING_LANDED     = 8,  /**< Landed. */
    ED_FISHING_SINK       = 9,  /**< Float sunk and bait lost. */
    ED_FISHING_BATTLE     = 10, /**< Fighting the fish. */
    ED_FISHING_RESULT     = 11, /**< Showing the catch. */
    ED_FISHING_FINISH     = 12, /**< Finishing. */
};

// clang-format on

/**
 * Steps of the fishing catch message, as fishing_mes holds them.
 */
// clang-format off
enum EdFishingMessageStep {
    ED_FISHING_MES_EVALUATE = 0, /**< Measuring the catch. */
    ED_FISHING_MES_WAIT     = 1, /**< Waiting for the window to close. */
    ED_FISHING_MES_DONE     = 2, /**< Done. */
};

// clang-format on

class CCharacter;
class CCamera;
class CRect_i_;
struct ED_MOVE_CHARA_INFO;

/** Where the caution mark is taken from in the event-battle texture. */
extern CRect_i_ Caution;

/** Character-movement state the editor shares with EdMoveChara. */
extern ED_MOVE_CHARA_INFO EdMoveCharaInfo;

void EBInitialize();
void EBInit(float speed_mult);
/** Starts the enemy-battle introduction sequence. */
void EBInitIntro();
/**
 * Ends the enemy-battle sequence.
 *
 * @mangled EBExit__Fv
 * @address 0x168560
 * @size 0x58
 */
void EBExit();
/**
 * Selects the enemy-battle diagnostic display.
 *
 * @mangled EBDebug__Fi
 * @address 0x168420
 * @size 0xC
 */
void EBDebug(int mode);
/** Advances the enemy-battle introduction and returns its state. */
int EBIntroLoop();
/** Advances the active enemy-battle sequence and returns its state. */
int  EBLoop();
void EBFinishSound(int do_fade_bgm, int do_play_fanfare);

/**
 * Disables the editor camera-view mode.
 *
 * @mangled EdViewModeOff__Fv
 * @address 0x169D80
 * @size 0xC
 */
void EdViewModeOff();

/**
 * Aims the editor camera from the character's head position.
 *
 * @mangled EdEyeCamera__FP7CCameraP10CCharacter
 * @address 0x169FF0
 * @size 0x128
 */
void EdEyeCamera(CCamera *camera, CCharacter *character);

/**
 * Returns the horizontal editor camera angle.
 *
 * @mangled EdAGetViewAngleH__Fv
 * @address 0x16A130
 * @size 0xC
 */
float EdAGetViewAngleH();

/**
 * Returns the vertical editor camera angle.
 *
 * @mangled EdAGetViewAngleV__Fv
 * @address 0x16A140
 * @size 0xC
 */
float EdAGetViewAngleV();

/**
 * Sets both editor camera angles.
 *
 * @mangled EdASetViewAngle__Fff
 * @address 0x16A150
 * @size 0x10
 */
void EdASetViewAngle(float horizontal, float vertical);

/**
 * Assigns the enemy-battle motion sequence for a character.
 *
 * @mangled EBSetMotion__FP10CCharacterPi
 * @address 0x1682B0
 * @size 0x168
 */
void EBSetMotion(CCharacter *character, int *motions);

/**
 * Adds a timed key command to the enemy-battle sequence.
 *
 * @mangled EBSetKey__Ffii
 * @address 0x168430
 * @size 0x12C
 */
void EBSetKey(float time, int buttons, int mode);

/**
 * Selects the editor input modes that currently own controller input.
 *
 * @mangled EdSetKeyMode__Fi
 * @address 0x1697F0
 * @size 0x10
 */
int EdSetKeyMode(int mode);

/**
 * Returns the right stick's horizontal axis when the requested mode is active.
 * @mangled EdGetRXf__Fi @address 0x169800 @size 0x44
 */
float EdGetRXf(int mode);

/**
 * Returns the right stick's vertical axis when the requested mode is active.
 * @mangled EdGetRYf__Fi @address 0x169850 @size 0x44
 */
float EdGetRYf(int mode);

/**
 * Returns the left stick's horizontal axis when the requested mode is active.
 * @mangled EdGetLXf__Fi @address 0x1698A0 @size 0x44
 */
float EdGetLXf(int mode);

/**
 * Returns the left stick's vertical axis when the requested mode is active.
 * @mangled EdGetLYf__Fi @address 0x1698F0 @size 0x44
 */
float EdGetLYf(int mode);

/**
 * Tests held editor buttons when the requested mode is active.
 * @mangled EdPadOn__Fii @address 0x169940 @size 0x50
 */
int EdPadOn(int keys, int mode);

/**
 * Tests newly pressed editor buttons when the requested mode is active.
 * @mangled EdPadDown__Fii @address 0x169990 @size 0x50
 */
int EdPadDown(int keys, int mode);

/**
 * The part of the screen the event battle's opening wipe has reached.
 */
extern CRect_i_ draw_rect;
