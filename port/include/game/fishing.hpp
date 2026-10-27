#pragma once

#include <array>

#include "collision.hpp"

namespace dc::game {

template <int T>
class CDataAlloc2;
class CFish;
class CCharacter;
class CFrame;
class CFrameVu1;
struct RS_STACKDATA;

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
 * Describes one kind of bait that can be put on the hook.
 */
struct EsaInfo {
    int   item_no; /**< Item the bait is made from. */
    float radius;  /**< Distance from which fish notice the bait. */
};

/**
 * The kinds of bait, indexed by esa_type.
 */
extern std::array<EsaInfo, 13> esa_info;

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
 */
void FishingInit();

/**
 * Sets the water surface and terrain heights used by the fishing simulation.
 */
void FishingSetWaterLevel(float water_level, float ground_level);

/**
 * Sets the ground heights under the float and under the hook.
 */
void FishingSetGroundLevel(float uki_height, float hook_height);

/**
 * Returns the water surface height used by the fishing simulation.
 */
float FishingGetWaterLevel();

/**
 * Takes the bait off the hook.
 */
void FishingDeleteEsa();

/**
 * Makes a fish that has bitten the hook the one fighting the line.
 */
void FishingBattleFish(int fish_no);

/**
 * Returns the fish fighting the line, or null when none is.
 */
CFish *FishingGetBattleFish();

/**
 * Removes the fish displayed after an angling battle.
 */
void FishingDeleteAngleFish();

/**
 * Sets the current tension applied to the fishing hook.
 */
void FishPullHook(float tension);

/**
 * Leaves the fishing minigame and releases its active state.
 */
void FishingExit();

/**
 * Reads the hook and float models and the fishing sounds, and sets aside room for the collision polygons.
 */
void FishingLoad(CDataAlloc2<1> *alloc, int slot);

/**
 * Reads the fish of one fishing spot into an arena, choosing their kinds by the spot and the time of day.
 */
void FishingLoadFish(int spot, CDataAlloc2<1> *alloc, int slot);

/**
 * Puts a bait item on the hook, giving any bait already there back to the inventory.
 */
void FishingLoadEsa(int item_no, CFrameVu1 *frame, int slot);

/**
 * Returns the item the bait on the hook came from, or -1 when the hook is bare.
 */
int FishingGetEsaItemNo();

/**
 * Copies the collision polygons the fish move against.
 */
void FishingSetCPoly(CCPoly *polys, int count);

/**
 * Sets the box the float and hook must stay within.
 */
void FishingSetRect(CBoxVu0 bounds);

/**
 * Builds the eight triangles of the fishing box's top and sides, and returns how many it built.
 */
int FishingPickUpPoly(CCPoly *polys);

/**
 * Sets the box the fish swim within and puts every fish at its centre, under the water surface.
 */
void FishingInitFish(CBoxVu0 bounds);

/**
 * Returns whether a fish is fighting the line, biting the hook or eating the bait, and which fish it is.
 */
int FishingFishStatus(int *fish_no);

/**
 * Returns the kind of one of the six fish.
 */
int FishingFishKind(int fish_no);

/**
 * Turns the fish fighting the line into the one being landed, reading its landing model from a pack.
 */
void FishingBattleToAngleFish(u_int *pack, CDataAlloc2<1> *alloc);

/**
 * Advances the line, float and hook one step from the rod's position.
 */
void FishLineStep(float *rod_position, float *unused);

/**
 * Makes a fish that has bitten the hook the one being landed.
 */
void FishingAngleFish(int fish_no);

/**
 * Returns the kind of the fish being landed, and gives its size in tenths and its fishing points.
 */
int FishingGetAngleFishSize(int *size, int *fp);

/**
 * Puts the six fish back to swimming with no interest in the bait.
 */
void FishingInitFishStatus();

/**
 * Advances the fish one step, showing them the hook and the bait on it.
 */
void FishingStepFish();

/**
 * Draws the fish being landed on the hook, or the fish swimming when they show.
 */
void FishingDrawFish();

/**
 * Casts the line straight down from the rod tip, with the float and the hook at rest on it.
 */
void FishLineInit(float *position);

/**
 * Pulls the float a share of the way towards a position, or stops pulling it when the share is negative.
 */
void FishLineSetUki(float *position, float rate);

/**
 * Pulls the hook a share of the way towards a position, or stops pulling it when the share is negative.
 */
void FishLineSetHook(float *position, float rate);

/**
 * Gives the position of the float.
 */
void FishLineGetUki(float *position);

/**
 * Gives the position of the hook.
 */
void FishLineGetHook(float *position);

/**
 * Returns whether the float or the hook has left the fishing box or risen above the water.
 */
int FishingCheckUkiHook();

/**
 * Draws the line, the float, the hook and its bait on one side of the water surface.
 */
void FishLineDraw(int above_water);

/**
 * Format of the fishing error message.
 */
extern char fishing_err_format[];

// Port additions.

/**
 * The towns' bait fix. Each town's bait-changing event (program 134 of gedit/e01, e02, e04 and s09
 * in PAL retail) opens the bait picker (_GOTO_CHANGE_ESA), which offers all thirteen baits and takes
 * the chosen one out of the bag, then looks the choice up in a list of eight baits the script keeps
 * in the eight variables after the one the picker writes to. Only a bait on that list is attached
 * (_LOAD_ITEM_FILE, _LOAD_ITEM, _SET_FISHING_ESA): Carrot, Potato Cake, Minon, Battan and Petite
 * Fish are not on it, so they were eaten and the hook left empty.
 *
 * Puts bait into the list's first place after choice, the variable the picker writes to, if the
 * script would not attach it otherwise, and returns whether it did. Nothing is changed when bait is
 * not one of the thirteen (a cancelled pick), when it is on the list already, or when the eight
 * variables do not all hold baits (a script laid out otherwise).
 */
bool ListBaitForScript(RS_STACKDATA *choice, int bait);

} // namespace dc::game
