#pragma once

#include "common.h"

#include <libvu0.h>

#include "character.hpp"
#include "dataalloc_fwd.hpp"

/** Number of bait kinds for which each fish records an attraction value. */
enum {
    FISH_FOOD_KIND_COUNT = 13
};

/**
 * Stores the fixed size, fishing-point, and bait attributes of one fish kind.
 */
struct FishInfo {
    float model_size;                          /**< Size represented by the unscaled fish model. */
    float min_size;                            /**< Smallest generated size of this fish kind. */
    float max_size;                            /**< Largest generated size of this fish kind. */
    s32   min_fp;                              /**< Fishing points awarded at the minimum size. */
    s32   max_fp;                              /**< Fishing points awarded at the maximum size. */
    float food_affinity[FISH_FOOD_KIND_COUNT]; /**< Attraction to each kind of bait. */
};

STATIC_ASSERT(sizeof(FishInfo) == 0x48);

/**
 * Names the fish's current movement behaviour.
 */
enum FishMoveMode {
    FISH_MOVE_UNSET = -1,
    FISH_MOVE_STOP = 0,
    FISH_MOVE_SWIM = 1,
    FISH_MOVE_FAST = 2,
    FISH_MOVE_ANGLE = 10,
    FISH_MOVE_BATTLE = 11
};

/**
 * Names the fish's current interaction with bait and the hook.
 */
enum FishAction {
    FISH_ACTION_NONE = -1,
    FISH_ACTION_APPROACH_FOOD = 6,
    FISH_ACTION_EAT_FOOD = 7,
    FISH_ACTION_BITE_HOOK = 8,
    FISH_ACTION_LEAVE = 9,
    FISH_ACTION_ANGLE = 10,
    FISH_ACTION_BATTLE = 11
};

/**
 * Simulates and draws one fish in normal and angling forms.
 */
class CFish {
public:
    s32           fish_kind;         /**< Index of the loaded fish kind, or -1 when unused. */
    FishInfo      info;              /**< Fixed attributes copied from the fish-kind table. */
    FishMoveMode  move_mode;         /**< Current autonomous movement behaviour. */
    FishAction    action;            /**< Current bait or hook interaction. */
    s32           move_timer;        /**< Steps remaining before choosing another movement. */
    s32           action_timer;      /**< Steps remaining before updating the interaction. */
    s32           use_angle_model;   /**< Nonzero while the angling character form is active. */
    float         size;              /**< Generated size of this fish. */
    float         angle_model_scale; /**< Scale applied to the angling character form. */
    float         model_scale;       /**< Scale applied to the normal character form. */
    s32           unk_6c;
    s32           unk_70;
    float         target_yaw; /**< Yaw toward which the fish turns. */
    s32           unk_78;
    s32           unk_7c;
    float         target_speed;         /**< Forward speed toward which the fish accelerates. */
    float         speed;                /**< Current forward speed. */
    s32           food_kind;            /**< Kind of bait currently presented, or -1 for none. */
    float         food_radius;          /**< Radius within which the fish notices the bait. */
    sceVu0FVECTOR food_position;        /**< World position of the current bait or hook. */
    CCharacter    model;                /**< Character used for normal swimming. */
    CCharacter    angle_model;          /**< Character used for angling and battle. */
    CCPoly       *collision_polys;      /**< Polygons that constrain fish movement. */
    s32           collision_poly_count; /**< Number of collision polygons. */
    u8            unk_2408[8];

    /**
     * Constructs both character forms and resets the fish.
     * @mangled __ct__5CFishFv
     * @address 0x1A8DC0
     * @size 0x184
     */
    CFish();

    /** Tests whether the fish notices the current bait. */
    int FindFood();
    /** Tests whether the fish eats the bait it has reached. */
    int EatFood();
    /** Tests whether the fish takes the hook it has reached. */
    int BiteHook();
    /** Tests whether the fish abandons the bait. */
    int LeaveFood();
    /** Tests whether the fish abandons the hook. */
    int LeaveHook();
    /** Sets the collision polygons that constrain the fish. */
    void SetCPoly(CCPoly *polys, int count);
    /** Advances movement, interaction, animation, and collision. */
    void Step();
    /** Draws the active character form. */
    void Draw();
    /** Sets the bait kind, position, and attraction radius. */
    void SetFoodPos(int kind, float *position, float radius);
    /** Puts the fish in the static angling-display mode. */
    void SetAngleMode();
    /** Puts the fish in the hooked battle mode. */
    void SetBattleMode();
    /** Generates the fish size and the scales of both models. */
    void SetScale();
    /** Calculates the fishing points awarded for this fish. */
    int GetFP();
    /** Selects the motion played by both character forms. */
    void SetMotion(int motion_no, int flags);
    /** Sets the position of both character forms. */
    void SetPosition(float *position);
    /** Gets the position of the angling character form. */
    void GetPosition(float *position);
    /** Sets the rotation of both character forms. */
    void SetRotation(float *rotation);
    /** Gets the rotation of the angling character form. */
    void GetRotation(float *rotation);
    /** Attaches both character frames to a reference frame. */
    void SetReference(CFrame *reference);
    /** Detaches both character frames from their reference frames. */
    void DeleteReference();
    /** Resets the fish's runtime state and both character forms. */
    void Initialize();
};

STATIC_ASSERT(sizeof(CFish) == 0x2410);

/** Returns whether a random sample succeeds at the supplied probability. */
int rand_check(float probability);
/** Returns a randomized ordinary movement duration. */
int GetActCnt();
/** Returns a randomized interaction duration. */
int GetActCnt2();
/** Loads one fish kind and its character data. */
void LoadFish(CFish *fish, int fish_kind, int texture_slot, CDataAlloc2<1> *alloc, int loaded_before);
/** Returns the character archive name for one fish kind. */
char *GetFishFileName(int fish_kind);
