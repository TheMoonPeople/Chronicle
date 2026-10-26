#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

/**
 * @file
 * Declares the opcodes a monster's battle script can call.
 *
 * Every opcode reads its arguments off the script's argument stack and reports
 * whether the script may carry on. The monster it acts on is the one
 * `NowMonstorUnit` currently names, except where the opcode takes a monster
 * index of its own.
 */

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CRunScript;
struct RS_STACKDATA;

/**
 * Integers each monster's script pushes and pops, eight per monster.
 */
extern int PUSH_INT_DATA[16][8];

/**
 * Integers the monster scripts share, set and read by index.
 */
extern int GL_INT[10];

/**
 * Opcode table the monster scripts dispatch through, indexed by operation number.
 */
extern int (*ext_func[256])(RS_STACKDATA *, int);

/**
 * Palette colour each element tints a hit monster with, as red, green and blue.
 */
extern int elmColor[6][3];

/**
 * Starts a motion on the monster and every part linked to it, optionally at a given speed and repeat count.
 *
 * @mangled _SET_MOTION__FP12RS_STACKDATAi
 * @address 0x1E1710
 * @size 0x478
 */
int _SET_MOTION(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the monster's motion has reached its last frame.
 *
 * @mangled _CHK_MOTION_FRM__FP12RS_STACKDATAi
 * @address 0x1E1B90
 * @size 0xCC
 */
int _CHK_MOTION_FRM(RS_STACKDATA *stack, int argc);

/**
 * Reads the frame the monster's motion currently stands on.
 *
 * @mangled _GET_MOTION_FRM__FP12RS_STACKDATAi
 * @address 0x1E1C60
 * @size 0x44
 */
int _GET_MOTION_FRM(RS_STACKDATA *stack, int argc);

/**
 * Moves the monster's motion to a given frame.
 *
 * @mangled _SET_MOTION_FRM__FP12RS_STACKDATAi
 * @address 0x1E1CB0
 * @size 0x48
 */
int _SET_MOTION_FRM(RS_STACKDATA *stack, int argc);

/**
 * Measures the distance from the monster to the player or to a given point.
 *
 * @mangled _GET_DISTANCE__FP12RS_STACKDATAi
 * @address 0x1E1D00
 * @size 0xEC
 */
int _GET_DISTANCE(RS_STACKDATA *stack, int argc);

/**
 * Reads the world position of the monster or of the player.
 *
 * @mangled _GET_POSITION__FP12RS_STACKDATAi
 * @address 0x1E1DF0
 * @size 0xEC
 */
int _GET_POSITION(RS_STACKDATA *stack, int argc);

/**
 * Turns the monster towards a point, immediately where no turn rate is given.
 *
 * @mangled _SET_ROTATION__FP12RS_STACKDATAi
 * @address 0x1E1EE0
 * @size 0x1C4
 */
int _SET_ROTATION(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the monster has finished turning.
 *
 * @mangled _CHK_ROTATION__FP12RS_STACKDATAi
 * @address 0x1E20B0
 * @size 0x70
 */
int _CHK_ROTATION(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the monster has finished moving, and how far it still has to go.
 *
 * @mangled _CHK_MOVE__FP12RS_STACKDATAi
 * @address 0x1E2120
 * @size 0xFC
 */
int _CHK_MOVE(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the player stands within a cone in front of the monster and near enough to it.
 *
 * @mangled _CHK_USER_INNER_PRODUCT__FP12RS_STACKDATAi
 * @address 0x1E2220
 * @size 0x210
 */
int _CHK_USER_INNER_PRODUCT(RS_STACKDATA *stack, int argc);

/**
 * Gives the unit vector from the monster to a point, optionally turned about the vertical axis.
 *
 * @mangled _GET_VECTOR__FP12RS_STACKDATAi
 * @address 0x1E2430
 * @size 0x210
 */
int _GET_VECTOR(RS_STACKDATA *stack, int argc);

/**
 * Gives the unit vector the monster faces, optionally turned about the vertical axis.
 *
 * @mangled _GET_DIRECTION__FP12RS_STACKDATAi
 * @address 0x1E2640
 * @size 0x17C
 */
int _GET_DIRECTION(RS_STACKDATA *stack, int argc);

/**
 * Sends the monster towards a point at a given speed.
 *
 * @mangled _SET_MOVE__FP12RS_STACKDATAi
 * @address 0x1E27C0
 * @size 0x15C
 */
int _SET_MOVE(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the floor carries the monster all the way to a point.
 *
 * @mangled _CHK_MOVE_INFO__FP12RS_STACKDATAi
 * @address 0x1E2920
 * @size 0x140
 */
int _CHK_MOVE_INFO(RS_STACKDATA *stack, int argc);

/**
 * Stops the monster where it stands.
 *
 * @mangled _SET_MOVE_CANSEL__FP12RS_STACKDATAi
 * @address 0x1E2A60
 * @size 0x38
 */
int _SET_MOVE_CANSEL(RS_STACKDATA *stack, int argc);

/**
 * Stops the monster's turn at its current angle.
 *
 * @mangled _SET_ROT_CANSEL__FP12RS_STACKDATAi
 * @address 0x1E2AA0
 * @size 0x38
 */
int _SET_ROT_CANSEL(RS_STACKDATA *stack, int argc);

/**
 * Puts the monster at a world position.
 *
 * @mangled _SET_POSITION__FP12RS_STACKDATAi
 * @address 0x1E2AE0
 * @size 0x84
 */
int _SET_POSITION(RS_STACKDATA *stack, int argc);

/**
 * Sets whether gravity pulls the monster down.
 *
 * @mangled _STATUS_SET_FALL__FP12RS_STACKDATAi
 * @address 0x1E2B70
 * @size 0x5C
 */
int _STATUS_SET_FALL(RS_STACKDATA *stack, int argc);

/**
 * Makes the monster proof against damage, unless it is already dying.
 *
 * @mangled _STATUS_SET_MUTEKI__FP12RS_STACKDATAi
 * @address 0x1E2BD0
 * @size 0x8C
 */
int _STATUS_SET_MUTEKI(RS_STACKDATA *stack, int argc);

/**
 * Fades the monster towards an opacity, or sets that opacity outright.
 *
 * @mangled _STATUS_SET_ALPHA__FP12RS_STACKDATAi
 * @address 0x1E2C60
 * @size 0xB4
 */
int _STATUS_SET_ALPHA(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the monster has finished fading out, or in.
 *
 * @mangled _STATUS_CHK_ALPHA__FP12RS_STACKDATAi
 * @address 0x1E2D20
 * @size 0x10C
 */
int _STATUS_CHK_ALPHA(RS_STACKDATA *stack, int argc);

/**
 * Marks the monster dead and takes it off the live count.
 *
 * @mangled _STATUS_SET_DEAD__FP12RS_STACKDATAi
 * @address 0x1E2E30
 * @size 0x4C
 */
int _STATUS_SET_DEAD(RS_STACKDATA *stack, int argc);

/**
 * Chooses the script the monster runs next.
 *
 * @mangled _STATUS_SET_EVENT__FP12RS_STACKDATAi
 * @address 0x1E2E80
 * @size 0x2C
 */
int _STATUS_SET_EVENT(RS_STACKDATA *stack, int argc);

/**
 * Chooses the script the monster runs next and reports the change.
 *
 * @mangled _RUN_SCRIPT__FP12RS_STACKDATAi
 * @address 0x1E2EB0
 * @size 0x3C
 */
int _RUN_SCRIPT(RS_STACKDATA *stack, int argc);

/**
 * Sets whether the monster takes part in collision.
 *
 * @mangled _STATUS_SET_COL_OFF__FP12RS_STACKDATAi
 * @address 0x1E2EF0
 * @size 0x5C
 */
int _STATUS_SET_COL_OFF(RS_STACKDATA *stack, int argc);

/**
 * Gives the monster's remaining life as a percentage of its maximum.
 *
 * @mangled _STATUS_GET_LIFE_RATE__FP12RS_STACKDATAi
 * @address 0x1E2F50
 * @size 0x88
 */
int _STATUS_GET_LIFE_RATE(RS_STACKDATA *stack, int argc);

/**
 * Gives the vector the player faces, turned by an angle and optionally normalized.
 *
 * @mangled _STATUS_GET_USER_VECTOR__FP12RS_STACKDATAi
 * @address 0x1E2FE0
 * @size 0x104
 */
int _STATUS_GET_USER_VECTOR(RS_STACKDATA *stack, int argc);

/**
 * Gives the monster's height.
 *
 * @mangled _STATUS_GET_HEIGHT__FP12RS_STACKDATAi
 * @address 0x1E30F0
 * @size 0x50
 */
int _STATUS_GET_HEIGHT(RS_STACKDATA *stack, int argc);

/**
 * Gives a random integer below a bound.
 *
 * @mangled _GET_RAND__FP12RS_STACKDATAi
 * @address 0x1E3140
 * @size 0x88
 */
int _GET_RAND(RS_STACKDATA *stack, int argc);

/**
 * Gives a random whole number below a bound, as a float.
 *
 * @mangled _GET_RANDF__FP12RS_STACKDATAi
 * @address 0x1E31D0
 * @size 0x84
 */
int _GET_RANDF(RS_STACKDATA *stack, int argc);

/**
 * Gives the sine of an angle in degrees.
 *
 * @mangled _SIN_DEG__FP12RS_STACKDATAi
 * @address 0x1E3260
 * @size 0x54
 */
int _SIN_DEG(RS_STACKDATA *stack, int argc);

/**
 * Gives the cosine of an angle in degrees.
 *
 * @mangled _COS_DEG__FP12RS_STACKDATAi
 * @address 0x1E32C0
 * @size 0x54
 */
int _COS_DEG(RS_STACKDATA *stack, int argc);

/**
 * Tints the monster, either with a given colour or with the colour of its element.
 *
 * @mangled _STATUS_SET_PALLET__FP12RS_STACKDATAi
 * @address 0x1E3320
 * @size 0x1D4
 */
int _STATUS_SET_PALLET(RS_STACKDATA *stack, int argc);

/**
 * Sets the distance at which the monster stops being drawn.
 *
 * @mangled _STATUS_SET_CLIPLEVEL__FP12RS_STACKDATAi
 * @address 0x1E3500
 * @size 0x54
 */
int _STATUS_SET_CLIPLEVEL(RS_STACKDATA *stack, int argc);

/**
 * Gives the damage the monster took from the last hit.
 *
 * @mangled _STATUS_GET_HITDMG_VOL__FP12RS_STACKDATAi
 * @address 0x1E3560
 * @size 0x50
 */
int _STATUS_GET_HITDMG_VOL(RS_STACKDATA *stack, int argc);

/**
 * Gives the motion the monster is playing.
 *
 * @mangled _STATUS_GET_MOTION_ID__FP12RS_STACKDATAi
 * @address 0x1E35B0
 * @size 0x44
 */
int _STATUS_GET_MOTION_ID(RS_STACKDATA *stack, int argc);

/**
 * Gives the damage collision that last struck the monster.
 *
 * @mangled _STATUS_GET_DMG_ID__FP12RS_STACKDATAi
 * @address 0x1E3600
 * @size 0x50
 */
int _STATUS_GET_DMG_ID(RS_STACKDATA *stack, int argc);

/**
 * Sets how far away the player may lock on to the monster.
 *
 * @mangled _STATUS_SET_LOCKON_DIST__FP12RS_STACKDATAi
 * @address 0x1E3650
 * @size 0x54
 */
int _STATUS_SET_LOCKON_DIST(RS_STACKDATA *stack, int argc);

/**
 * Sets the length of the monster's shadow.
 *
 * @mangled _STATUS_SET_SHADOW_LEN__FP12RS_STACKDATAi
 * @address 0x1E36B0
 * @size 0x54
 */
int _STATUS_SET_SHADOW_LEN(RS_STACKDATA *stack, int argc);

/**
 * Puts the lock-on marker on a named frame of the monster's model.
 *
 * @mangled _STATUS_SET_LOCKON_TRG__FP12RS_STACKDATAi
 * @address 0x1E3710
 * @size 0x130
 */
int _STATUS_SET_LOCKON_TRG(RS_STACKDATA *stack, int argc);

/**
 * Gives the monster a movement collision sphere on a named frame.
 *
 * @mangled _SET_MOV_COL__FP12RS_STACKDATAi
 * @address 0x1E3840
 * @size 0x1A8
 */
int _SET_MOV_COL(RS_STACKDATA *stack, int argc);

/**
 * Gives the monster a body collision sphere on a named frame, and clears its parameters.
 *
 * @mangled _SET_BODY_COL__FP12RS_STACKDATAi
 * @address 0x1E39F0
 * @size 0x2A8
 */
int _SET_BODY_COL(RS_STACKDATA *stack, int argc);

/**
 * Sets one parameter of the body collision sphere the last opcode created.
 *
 * @mangled _SET_BODY_COL_PARA__FP12RS_STACKDATAi
 * @address 0x1E3CA0
 * @size 0x104
 */
int _SET_BODY_COL_PARA(RS_STACKDATA *stack, int argc);

/**
 * Gives the monster a damage collision sphere on a named frame.
 *
 * @mangled _SET_DMG_COL__FP12RS_STACKDATAi
 * @address 0x1E3DB0
 * @size 0x214
 */
int _SET_DMG_COL(RS_STACKDATA *stack, int argc);

/**
 * Sets the damage, kind and effect of the damage collision the last opcode created.
 *
 * @mangled _SET_DMG_PARA__FP12RS_STACKDATAi
 * @address 0x1E3FD0
 * @size 0x150
 */
int _SET_DMG_PARA(RS_STACKDATA *stack, int argc);

/**
 * Fires the monster's first projectile from a named frame.
 *
 * @mangled _SET_SHOT__FP12RS_STACKDATAi
 * @address 0x1E4120
 * @size 0x1EC
 */
int _SET_SHOT(RS_STACKDATA *stack, int argc);

/**
 * Fires the monster's second projectile from a named frame.
 *
 * @mangled _SET_SHOT2__FP12RS_STACKDATAi
 * @address 0x1E4310
 * @size 0x20C
 */
int _SET_SHOT2(RS_STACKDATA *stack, int argc);

/**
 * Schedules a sound to play when the motion reaches a frame.
 *
 * @mangled _SET_SND_FRM__FP12RS_STACKDATAi
 * @address 0x1E4520
 * @size 0x144
 */
int _SET_SND_FRM(RS_STACKDATA *stack, int argc);

/**
 * Starts a sound that keeps playing until it is stopped.
 *
 * @mangled _SET_LOOP_SND__FP12RS_STACKDATAi
 * @address 0x1E4670
 * @size 0xC8
 */
int _SET_LOOP_SND(RS_STACKDATA *stack, int argc);

/**
 * Stops the monster's looping sound.
 *
 * @mangled _STOP_LOOP_SND__FP12RS_STACKDATAi
 * @address 0x1E4740
 * @size 0x6C
 */
int _STOP_LOOP_SND(RS_STACKDATA *stack, int argc);

/**
 * Forgets the monster's looping sound without stopping it.
 *
 * @mangled _DEL_LOOP_SND__FP12RS_STACKDATAi
 * @address 0x1E47B0
 * @size 0x34
 */
int _DEL_LOOP_SND(RS_STACKDATA *stack, int argc);

/**
 * Plays a sound at once.
 *
 * @mangled _SET_SND_NOW__FP12RS_STACKDATAi
 * @address 0x1E47F0
 * @size 0x38
 */
int _SET_SND_NOW(RS_STACKDATA *stack, int argc);

/**
 * Stops a sound at once.
 *
 * @mangled _STOP_SND_NOW__FP12RS_STACKDATAi
 * @address 0x1E4830
 * @size 0x34
 */
int _STOP_SND_NOW(RS_STACKDATA *stack, int argc);

/**
 * Gives the character the player is controlling.
 *
 * @mangled _GET_CHR_ID__FP12RS_STACKDATAi
 * @address 0x1E4870
 * @size 0x2C
 */
int _GET_CHR_ID(RS_STACKDATA *stack, int argc);

/**
 * Gives the body collision that was last struck.
 *
 * @mangled _GET_COL_HIT_ID__FP12RS_STACKDATAi
 * @address 0x1E48A0
 * @size 0x50
 */
int _GET_COL_HIT_ID(RS_STACKDATA *stack, int argc);

/**
 * Gives the index of the monster the script is running for.
 *
 * @mangled _GET_SCRIPT_ID__FP12RS_STACKDATAi
 * @address 0x1E48F0
 * @size 0x2C
 */
int _GET_SCRIPT_ID(RS_STACKDATA *stack, int argc);

/**
 * Reads the world position of another monster.
 *
 * @mangled _GET_MONSTOR_POS__FP12RS_STACKDATAi
 * @address 0x1E4920
 * @size 0x98
 */
int _GET_MONSTOR_POS(RS_STACKDATA *stack, int argc);

/**
 * Reads the motion frame another monster stands on.
 *
 * @mangled _GET_MONSTOR_FRM__FP12RS_STACKDATAi
 * @address 0x1E49C0
 * @size 0x58
 */
int _GET_MONSTOR_FRM(RS_STACKDATA *stack, int argc);

/**
 * Puts another monster at a world position.
 *
 * @mangled _SET_MONSTOR_POS__FP12RS_STACKDATAi
 * @address 0x1E4A20
 * @size 0x9C
 */
int _SET_MONSTOR_POS(RS_STACKDATA *stack, int argc);

/**
 * Sends another monster towards a point at a given speed.
 *
 * @mangled _SET_MONSTOR_MOVE__FP12RS_STACKDATAi
 * @address 0x1E4AC0
 * @size 0x13C
 */
int _SET_MONSTOR_MOVE(RS_STACKDATA *stack, int argc);

/**
 * Gives one monster the heading and speed of another.
 *
 * @mangled _SET_MONSTOR_LINK_MOVE__FP12RS_STACKDATAi
 * @address 0x1E4C00
 * @size 0xBC
 */
int _SET_MONSTOR_LINK_MOVE(RS_STACKDATA *stack, int argc);

/**
 * Stops another monster where it stands.
 *
 * @mangled _SET_MONSTOR_MOVE_CANSEL__FP12RS_STACKDATAi
 * @address 0x1E4CC0
 * @size 0x4C
 */
int _SET_MONSTOR_MOVE_CANSEL(RS_STACKDATA *stack, int argc);

/**
 * Sets how far away the player may lock on to the monster.
 *
 * @mangled _SET_LOCKON_DIST__FP12RS_STACKDATAi
 * @address 0x1E4D10
 * @size 0x54
 */
int _SET_LOCKON_DIST(RS_STACKDATA *stack, int argc);

/**
 * Sets whether the player may lock on to the monster at all.
 *
 * @mangled _SET_LOCKON_SW__FP12RS_STACKDATAi
 * @address 0x1E4D70
 * @size 0x5C
 */
int _SET_LOCKON_SW(RS_STACKDATA *stack, int argc);

/**
 * Starts a motion on another monster and every part linked to it.
 *
 * @mangled _SET_MONSTOR_MOTION__FP12RS_STACKDATAi
 * @address 0x1E4DD0
 * @size 0x3B4
 */
int _SET_MONSTOR_MOTION(RS_STACKDATA *stack, int argc);

/**
 * Stores an integer in a slot every monster script shares.
 *
 * @mangled _SET_GLOBAL_INT__FP12RS_STACKDATAi
 * @address 0x1E5190
 * @size 0x5C
 */
int _SET_GLOBAL_INT(RS_STACKDATA *stack, int argc);

/**
 * Reads an integer out of a slot every monster script shares.
 *
 * @mangled _GET_GLOBAL_INT__FP12RS_STACKDATAi
 * @address 0x1E51F0
 * @size 0x50
 */
int _GET_GLOBAL_INT(RS_STACKDATA *stack, int argc);

/**
 * Pitches the monster towards a point.
 *
 * @mangled _SET_ROTATION_X__FP12RS_STACKDATAi
 * @address 0x1E5310
 * @size 0x168
 */
int _SET_ROTATION_X(RS_STACKDATA *stack, int argc);

/**
 * Aims one of the monster's three axes at a point.
 *
 * @mangled _LOOKAT__FP12RS_STACKDATAi
 * @address 0x1E5480
 * @size 0x338
 */
int _LOOKAT(RS_STACKDATA *stack, int argc);

/**
 * Sets how quickly the monster and its linked parts blend between motions.
 *
 * @mangled _SET_MOTION_CHANGE_STEP__FP12RS_STACKDATAi
 * @address 0x1E57C0
 * @size 0xA8
 */
int _SET_MOTION_CHANGE_STEP(RS_STACKDATA *stack, int argc);

/**
 * Gives the unit vector from another monster to a point, optionally turned about the vertical axis.
 *
 * @mangled _GET_MONSTOR_VECTOR__FP12RS_STACKDATAi
 * @address 0x1E5870
 * @size 0x214
 */
int _GET_MONSTOR_VECTOR(RS_STACKDATA *stack, int argc);

/**
 * Sets the monster's life to a fraction of its maximum.
 *
 * @mangled _STATUS_SET_LIFE__FP12RS_STACKDATAi
 * @address 0x1E5A90
 * @size 0x84
 */
int _STATUS_SET_LIFE(RS_STACKDATA *stack, int argc);

/**
 * Stores the monster's script flag word.
 *
 * @mangled _SET_BIN2__FP12RS_STACKDATAi
 * @address 0x1E5B20
 * @size 0x5C
 */
int _SET_BIN2(RS_STACKDATA *stack, int argc);

/**
 * Sets one of a monster's five status-change resistances.
 *
 * @mangled _SET_STATUS_CHANGE__FP12RS_STACKDATAi
 * @address 0x1E5B80
 * @size 0xCC
 */
int _SET_STATUS_CHANGE(RS_STACKDATA *stack, int argc);

/**
 * Turns one of a monster's texture animations on or off.
 *
 * @mangled _SET_TEX_ANIME_SW__FP12RS_STACKDATAi
 * @address 0x1E5C50
 * @size 0x10C
 */
int _SET_TEX_ANIME_SW(RS_STACKDATA *stack, int argc);

/**
 * Reads the monster's script flag word.
 *
 * @mangled _GET_STATUS_BIN2__FP12RS_STACKDATAi
 * @address 0x1E5D60
 * @size 0x58
 */
int _GET_STATUS_BIN2(RS_STACKDATA *stack, int argc);

/**
 * Sets the radius the monster occupies for collision.
 *
 * @mangled _SET_COLLISION_WIDTH__FP12RS_STACKDATAi
 * @address 0x1E5DC0
 * @size 0x54
 */
int _SET_COLLISION_WIDTH(RS_STACKDATA *stack, int argc);

/**
 * Finds the nearest other live monster within two hundred and forty units.
 *
 * @mangled _GET_NEAR_MONSTER__FP12RS_STACKDATAi
 * @address 0x1E5E20
 * @size 0x198
 */
int _GET_NEAR_MONSTER(RS_STACKDATA *stack, int argc);

/**
 * Starts the fade to black that ends a boss fight.
 *
 * @mangled _BOSS_FADE_OUT__FP12RS_STACKDATAi
 * @address 0x1E5FC0
 * @size 0x50
 */
int _BOSS_FADE_OUT(RS_STACKDATA *stack, int argc);

/**
 * Reports whether the boss fade has finished.
 *
 * @mangled _CHEKC_FADE_OUT__FP12RS_STACKDATAi
 * @address 0x1E6010
 * @size 0x40
 */
int _CHEKC_FADE_OUT(RS_STACKDATA *stack, int argc);

/**
 * Sets the gravity applied to the monster.
 *
 * @mangled _SET_GRAVITY__FP12RS_STACKDATAi
 * @address 0x1E6050
 * @size 0x5C
 */
int _SET_GRAVITY(RS_STACKDATA *stack, int argc);

/**
 * Opens a window, in motion frames, during which the monster guards.
 *
 * @mangled _SET_GUARD_FRAME__FP12RS_STACKDATAi
 * @address 0x1E60B0
 * @size 0x14C
 */
int _SET_GUARD_FRAME(RS_STACKDATA *stack, int argc);

/**
 * Reports the player's guard state, distance and character while the monster is guarding.
 *
 * @mangled _GUARD_SEARCH__FP12RS_STACKDATAi
 * @address 0x1E6200
 * @size 0x158
 */
int _GUARD_SEARCH(RS_STACKDATA *stack, int argc);

/**
 * Gives the direction the player is moving in.
 *
 * @mangled _GET_MOVE_VEC__FP12RS_STACKDATAi
 * @address 0x1E6360
 * @size 0x5C
 */
int _GET_MOVE_VEC(RS_STACKDATA *stack, int argc);

/**
 * Stores an integer in one of the monster's own eight slots.
 *
 * @mangled _PUSH_IGLOBAL__FP12RS_STACKDATAi
 * @address 0x1E63C0
 * @size 0x94
 */
int _PUSH_IGLOBAL(RS_STACKDATA *stack, int argc);

/**
 * Reads an integer out of one of the monster's own eight slots.
 *
 * @mangled _POP_IGLOBAL__FP12RS_STACKDATAi
 * @address 0x1E6460
 * @size 0x88
 */
int _POP_IGLOBAL(RS_STACKDATA *stack, int argc);

/**
 * Gives the status ailment the player is suffering.
 *
 * @mangled _GET_USER_STATUS__FP12RS_STACKDATAi
 * @address 0x1E64F0
 * @size 0x74
 */
int _GET_USER_STATUS(RS_STACKDATA *stack, int argc);

/**
 * Attaches the monster's model to a named frame of its own hierarchy.
 *
 * @mangled _SET_REFERENCE__FP12RS_STACKDATAi
 * @address 0x1E6570
 * @size 0xC0
 */
int _SET_REFERENCE(RS_STACKDATA *stack, int argc);

/**
 * Detaches another monster's model from whatever it was attached to.
 *
 * @mangled _DEL_REFERENCE__FP12RS_STACKDATAi
 * @address 0x1E6630
 * @size 0x5C
 */
int _DEL_REFERENCE(RS_STACKDATA *stack, int argc);

/**
 * Sets whether the monster casts a shadow.
 *
 * @mangled _SET_SHADOW_FLAG__FP12RS_STACKDATAi
 * @address 0x1E6690
 * @size 0x5C
 */
int _SET_SHADOW_FLAG(RS_STACKDATA *stack, int argc);

/**
 * Loads a monster script, giving it its argument stack, call stack and opcode table.
 *
 * @mangled BtSetEventScript__FP10CRunScriptPcP14CDataAlloc2_1_
 * @address 0x1E66F0
 * @size 0xA4
 */
int BtSetEventScript(CRunScript *script, char *program, CDataAlloc2<1> *arena);

/**
 * Builds the opcode table out of the extension list, refusing a list that names one twice.
 *
 * @mangled BtSetEventExtendTable__Fv
 * @address 0x1E67A0
 * @size 0x12C
 */
void BtSetEventExtendTable();
