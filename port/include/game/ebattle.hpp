#pragma once

namespace dc::game {

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
class CCPoly;
struct ED_MOVE_CHARA_INFO;
struct MoveCheckInfo;

/** Where the caution mark is taken from in the event-battle texture. */
extern CRect_i_ Caution;

/** Character-movement state the editor shares with EdMoveChara. */
extern ED_MOVE_CHARA_INFO EdMoveCharaInfo;

/**
 * Sets up the per-language texture rectangles of the event-battle prompts.
 */
void EBInitialize();

/**
 * Prepares an event battle, advancing its prompts at speed_mult times the normal rate.
 */
void EBInit(float speed_mult);

/**
 * Starts the enemy-battle introduction sequence.
 */
void EBInitIntro();

/**
 * Ends the enemy-battle sequence.
 */
void EBExit();

/**
 * Selects the enemy-battle diagnostic display.
 */
void EBDebug(int mode);

/**
 * Draws the event battle's own overlay.
 */
void EBDraw();

/**
 * Advances the enemy-battle introduction and returns its state.
 */
int EBIntroLoop();

/**
 * Advances the active enemy-battle sequence and returns its state.
 */
int EBLoop();

/**
 * Chooses whether the music fades and the fanfare plays when the battle ends.
 */
void EBFinishSound(int do_fade_bgm, int do_play_fanfare);

/**
 * Disables the editor camera-view mode.
 */
void EdViewModeOff();

/**
 * Aims the editor camera from the character's head position.
 */
void EdEyeCamera(CCamera *camera, CCharacter *character);

/**
 * Returns the horizontal editor camera angle.
 */
float EdAGetViewAngleH();

/**
 * Returns the vertical editor camera angle.
 */
float EdAGetViewAngleV();

/**
 * Sets both editor camera angles.
 */
void EdASetViewAngle(float horizontal, float vertical);

/**
 * Assigns the enemy-battle motion sequence for a character.
 */
void EBSetMotion(CCharacter *character, int *motions);

/**
 * Adds a timed key command to the enemy-battle sequence.
 */
void EBSetKey(float time, int buttons, int mode);

/**
 * Selects the editor input modes that currently own controller input.
 */
int EdSetKeyMode(int mode);

/**
 * Returns the right stick's horizontal axis when the requested mode is active.
 */
float EdGetRXf(int mode);

/**
 * Returns the right stick's vertical axis when the requested mode is active.
 */
float EdGetRYf(int mode);

/**
 * Returns the left stick's horizontal axis when the requested mode is active.
 */
float EdGetLXf(int mode);

/**
 * Returns the left stick's vertical axis when the requested mode is active.
 */
float EdGetLYf(int mode);

/**
 * Tests held editor buttons when the requested mode is active.
 */
int EdPadOn(int keys, int mode);

/**
 * Tests newly pressed editor buttons when the requested mode is active.
 */
int EdPadDown(int keys, int mode);

/**
 * Town-walk collision step: adds keyboard first-person velocity, then runs the retail
 * floor, wall and event checks, without treating the town as an interior.
 */
int PortEdMoveCheck(float *pos, float *velocity, float *out_pos, MoveCheckInfo *out_info, CCPoly *polys, int poly_num, int mode);

/**
 * The part of the screen the event battle's opening wipe has reached.
 */
extern CRect_i_ draw_rect;

/**
 * Whether the editor camera looks from the character's eyes.
 */
extern int viewMode;

/**
 * Movement mode of the editor character.
 */
extern int chara_mode;

/**
 * Step of the editor character's fishing. @see EdFishingState
 */
extern int chara_fishing;

/**
 * Horizontal angle of the first-person view.
 */
extern float viewAngleH;

/**
 * Vertical angle of the first-person view.
 */
extern float viewAngleV;

/**
 * Closest the editor camera comes to the character.
 */
extern float camera_near_dist;

/**
 * Farthest the editor camera goes from the character.
 */
extern float camera_far_dist;

/**
 * Aims the town camera from the character's eyes, turned by the right stick and, while it is
 * shown, the mouse.
 */
void EyeCamera(CCamera *camera, CCharacter *character, int right_stick);

} // namespace dc::game
