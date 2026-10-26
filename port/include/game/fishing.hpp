#pragma once

#include "collision.hpp"

template <int T>
class CDataAlloc2;
class CFish;
class CCharacter;
class CFrame;
class CFrameVu1;

/**
 * Whether the fish are drawn while none is being landed.
 */
extern int draw_under_water;

/**
 * The spacing between neighbouring points of the line.
 */
extern float distp;

/**
 * The water surface height used by the fishing simulation.
 */
extern float WaterLevel;

/**
 * The terrain height used by the fishing simulation.
 */
extern float GroundLevel;

/**
 * The ground height under the float.
 */
extern float UkiGroundLevel;

/**
 * The ground height under the hook.
 */
extern float HookGroundLevel;

/**
 * The ground height the whole line rests on, the lower of the float and hook heights.
 */
extern float LineGroundLevel;

/**
 * The model of the bait on the hook, or null when the hook is bare.
 */
extern CFrameVu1 *EsaFrame;

/**
 * The kind of bait on the hook, or -1 when the hook is bare.
 */
extern int esa_type;

/**
 * The line's hook model.
 */
extern CFrame *HookFrame;

/**
 * The line's float model.
 */
extern CFrame *UkiFrame;

/**
 * The texture slot of the rod, float and hook, or -99 before they are read.
 */
extern int fishing_texb;

/**
 * The texture slot of the fish, or -99 before they are read.
 */
extern int fish_texb;

/**
 * The texture slot of the bait, or -99 before it is read.
 */
extern int esa_texb;

/**
 * The fish of the fishing spot.
 */
extern CFish *Fish;

/**
 * The fish displayed after an angling battle, or null when none is displayed.
 */
extern CFish *AngleFish;

/**
 * The fish fighting the line, or null when none is.
 */
extern CFish *BattleFish;

/**
 * The number of fish read for the fishing spot.
 */
extern int FishNum;

/**
 * The collision polygons the fish move against.
 */
extern CCPoly *cpoly;

/**
 * The number of collision polygons in cpoly.
 */
extern int cpoly_num;

/**
 * Whether the hook is being pulled towards fishhook.
 */
extern int set_hook_pos;

/**
 * Whether the float is being pulled towards uki.
 */
extern int set_uki_pos;

/**
 * The current tension applied to the fishing hook.
 */
extern float pull_hook;

/**
 * The box the float and hook must stay within.
 */
extern CBoxVu0 fishing_rect;

/**
 * The box the fish swim within.
 */
extern CBoxVu0 fish_rect;

/**
 * The frame the float's model hangs from.
 */
extern CFrameVu1 UkiFrameTop;

/**
 * The fishing rod.
 */
extern CCharacter Rod;

/**
 * The points of the line from the rod tip to the hook.
 */
extern sceVu0FVECTOR point[24];

/**
 * The previous positions of the line's points.
 */
extern sceVu0FVECTOR old_p[24];

/**
 * The velocities of the line's points.
 */
extern sceVu0FVECTOR velo[24];

/**
 * The points of the hook's body.
 */
extern sceVu0FVECTOR hookp[3];

/**
 * The previous positions of the hook's points.
 */
extern sceVu0FVECTOR hookop[3];

/**
 * The velocities of the hook's points.
 */
extern sceVu0FVECTOR hookv[3];

/**
 * The rest lengths of the hook's links.
 */
extern float hook_dist[3];

/**
 * The points of the float's body.
 */
extern sceVu0FVECTOR ukip[4];

/**
 * The previous positions of the float's points.
 */
extern sceVu0FVECTOR ukiop[4];

/**
 * The velocities of the float's points.
 */
extern sceVu0FVECTOR ukiv[4];

/**
 * The rest lengths of the float's links.
 */
extern float uki_dist[6];

/**
 * The world position of the rod tip.
 */
extern sceVu0FVECTOR rod_top;

/**
 * The position the hook is being pulled to.
 */
extern sceVu0FVECTOR fishhook;

/**
 * The position the float is being pulled to.
 */
extern sceVu0FVECTOR uki;

/**
 * Initializes the fishing subsystem's runtime state.
 *
 * @mangled FishingInit__Fv
 * @address 0x1A9070
 * @size 0x7C
 */
void FishingInit();

/**
 * Sets the water surface and terrain heights used by the fishing simulation.
 *
 * @mangled FishingSetWaterLevel__Fff
 * @address 0x1A9190
 * @size 0x2C
 */
void FishingSetWaterLevel(float water_level, float ground_level);

/**
 * Sets the ground heights under the float and under the hook.
 *
 * @mangled FishingSetGroundLevel__Fff
 * @address 0x1A91C0
 * @size 0x10
 */
void FishingSetGroundLevel(float uki_height, float hook_height);

/**
 * Returns the water surface height used by the fishing simulation.
 *
 * @mangled FishingGetWaterLevel__Fv
 * @address 0x1A91D0
 * @size 0xC
 */
float FishingGetWaterLevel();

/**
 * Takes the bait off the hook.
 *
 * @mangled FishingDeleteEsa__Fv
 * @address 0x1A9010
 * @size 0x14
 */
void FishingDeleteEsa();

/**
 * Makes a fish that has bitten the hook the one fighting the line.
 *
 * @mangled FishingBattleFish__Fi
 * @address 0x1A9650
 * @size 0x68
 */
void FishingBattleFish(int fish_no);

/**
 * Returns the fish fighting the line, or null when none is.
 *
 * @mangled FishingGetBattleFish__Fv
 * @address 0x1A97A0
 * @size 0xC
 */
CFish *FishingGetBattleFish();

/**
 * Removes the fish displayed after an angling battle.
 *
 * @mangled FishingDeleteAngleFish__Fv
 * @address 0x1A9920
 * @size 0x1C
 */
void FishingDeleteAngleFish();

/**
 * Sets the current tension applied to the fishing hook.
 *
 * @mangled FishPullHook__Ff
 * @address 0x1AA170
 * @size 0xC
 */
void FishPullHook(float tension);

/**
 * Leaves the fishing minigame and releases its active state.
 *
 * @mangled FishingExit__Fv
 * @address 0x1A90F0
 * @size 0x98
 */
void FishingExit();

/**
 * Reads the hook and float models and the fishing sounds, and sets aside room for the collision polygons.
 *
 * @mangled FishingLoad__FP14CDataAlloc2_1_i
 * @address 0x1A87E0
 * @size 0x110
 */
void FishingLoad(CDataAlloc2<1> *alloc, int slot);

/**
 * Reads the fish of one fishing spot into an arena, choosing their kinds by the spot and the time of day.
 *
 * @mangled FishingLoadFish__FiP14CDataAlloc2_1_i
 * @address 0x1A88F0
 * @size 0x4CC
 */
void FishingLoadFish(int spot, CDataAlloc2<1> *alloc, int slot);

/**
 * Puts a bait item on the hook, giving any bait already there back to the inventory.
 *
 * @mangled FishingLoadEsa__FiP9CFrameVu1i
 * @address 0x1A8F50
 * @size 0xBC
 */
void FishingLoadEsa(int item_no, CFrameVu1 *frame, int slot);

/**
 * Returns the item the bait on the hook came from, or -1 when the hook is bare.
 *
 * @mangled FishingGetEsaItemNo__Fv
 * @address 0x1A9030
 * @size 0x38
 */
int FishingGetEsaItemNo();

/**
 * Copies the collision polygons the fish move against.
 *
 * @mangled FishingSetCPoly__FP6CCPolyi
 * @address 0x1A91E0
 * @size 0x7C
 */
void FishingSetCPoly(CCPoly *polys, int count);

/**
 * Sets the box the float and hook must stay within.
 *
 * @mangled FishingSetRect__F7CBoxVu0
 * @address 0x1A9260
 * @size 0x40
 */
void FishingSetRect(CBoxVu0 bounds);

/**
 * Builds the eight triangles of the fishing box's top and sides, and returns how many it built.
 *
 * @mangled FishingPickUpPoly__FP6CCPoly
 * @address 0x1A92A0
 * @size 0x1BC
 */
int FishingPickUpPoly(CCPoly *polys);

/**
 * Sets the box the fish swim within and puts every fish at its centre, under the water surface.
 *
 * @mangled FishingInitFish__F7CBoxVu0
 * @address 0x1A9460
 * @size 0x108
 */
void FishingInitFish(CBoxVu0 bounds);

/**
 * Returns whether a fish is fighting the line, biting the hook or eating the bait, and which fish it is.
 *
 * @mangled FishingFishStatus__FPi
 * @address 0x1A9570
 * @size 0xD8
 */
int FishingFishStatus(int *fish_no);

/**
 * Returns the kind of one of the six fish.
 *
 * @mangled FishingFishKind__Fi
 * @address 0x1A96C0
 * @size 0x4C
 */
int FishingFishKind(int fish_no);

/**
 * Turns the fish fighting the line into the one being landed, reading its landing model from a pack.
 *
 * @mangled FishingBattleToAngleFish__FPUiP14CDataAlloc2_1_
 * @address 0x1A9710
 * @size 0x88
 */
void FishingBattleToAngleFish(u_int *pack, CDataAlloc2<1> *alloc);

/**
 * Advances the line, float and hook one step from the rod's position.
 *
 * @mangled FishLineStep__FPfPf
 * @address 0x1AA340
 * @size 0xDA8
 */
void FishLineStep(float *rod_position, float *unused);

/**
 * Makes a fish that has bitten the hook the one being landed.
 *
 * @mangled FishingAngleFish__Fi
 * @address 0x1A97B0
 * @size 0x80
 */
void FishingAngleFish(int fish_no);

/**
 * Returns the kind of the fish being landed, and gives its size in tenths and its fishing points.
 *
 * @mangled FishingGetAngleFishSize__FPiPi
 * @address 0x1A9830
 * @size 0x84
 */
int FishingGetAngleFishSize(int *size, int *fp);

/**
 * Puts the six fish back to swimming with no interest in the bait.
 *
 * @mangled FishingInitFishStatus__Fv
 * @address 0x1A98C0
 * @size 0x5C
 */
void FishingInitFishStatus();

/**
 * Advances the fish one step, showing them the hook and the bait on it.
 *
 * @mangled FishingStepFish__Fv
 * @address 0x1A9940
 * @size 0x138
 */
void FishingStepFish();

/**
 * Draws the fish being landed on the hook, or the fish swimming when they show.
 *
 * @mangled FishingDrawFish__Fv
 * @address 0x1A9A80
 * @size 0x13C
 */
void FishingDrawFish();

/**
 * Casts the line straight down from the rod tip, with the float and the hook at rest on it.
 *
 * @mangled FishLineInit__FPf
 * @address 0x1A9BF0
 * @size 0x43C
 */
void FishLineInit(float *position);

/**
 * Pulls the float a share of the way towards a position, or stops pulling it when the share is negative.
 *
 * @mangled FishLineSetUki__FPff
 * @address 0x1AA030
 * @size 0x98
 */
void FishLineSetUki(float *position, float rate);

/**
 * Pulls the hook a share of the way towards a position, or stops pulling it when the share is negative.
 *
 * @mangled FishLineSetHook__FPff
 * @address 0x1AA0D0
 * @size 0x98
 */
void FishLineSetHook(float *position, float rate);

/**
 * Gives the position of the float.
 *
 * @mangled FishLineGetUki__FPf
 * @address 0x1AA180
 * @size 0x28
 */
void FishLineGetUki(float *position);

/**
 * Gives the position of the hook.
 *
 * @mangled FishLineGetHook__FPf
 * @address 0x1AA1B0
 * @size 0x28
 */
void FishLineGetHook(float *position);

/**
 * Returns whether the float or the hook has left the fishing box or risen above the water.
 *
 * @mangled FishingCheckUkiHook__Fv
 * @address 0x1AA1E0
 * @size 0x158
 */
int FishingCheckUkiHook();

/**
 * Draws the line, the float, the hook and its bait on one side of the water surface.
 *
 * @mangled FishLineDraw__Fi
 * @address 0x1AB0F0
 * @size 0x64C
 */
void FishLineDraw(int above_water);

/**
 * Format of the fishing error message.
 */
extern char fishing_err_format[];
