#pragma once

#include "common.h"

#include <libvu0.h>

#include "character.hpp"
#include "dataalloc_fwd.hpp"

namespace dc::game {

/**
 * Number of bait kinds for which each fish records an attraction value.
 */
inline constexpr int FISH_FOOD_KIND_COUNT = 13;

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
enum FishMoveMode : s32 {
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
enum FishAction : s32 {
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
     */
    CFish();

    /**
     * Tests whether the fish notices the current bait.
     */
    int FindFood();

    /**
     * Tests whether the fish eats the bait it has reached.
     */
    int EatFood();

    /**
     * Tests whether the fish takes the hook it has reached.
     */
    int BiteHook();

    /**
     * Tests whether the fish abandons the bait.
     */
    int LeaveFood();

    /**
     * Tests whether the fish abandons the hook.
     */
    int LeaveHook();

    /**
     * Sets the collision polygons that constrain the fish.
     */
    void SetCPoly(CCPoly *polys, int count);

    /**
     * Advances movement, interaction, animation, and collision.
     */
    void Step();

    /**
     * Draws the active character form.
     */
    void Draw();

    /**
     * Sets the bait kind, position, and attraction radius.
     */
    void SetFoodPos(int kind, float *position, float radius);

    /**
     * Puts the fish in the static angling-display mode.
     */
    void SetAngleMode();

    /**
     * Puts the fish in the hooked battle mode.
     */
    void SetBattleMode();

    /**
     * Generates the fish size and the scales of both models.
     */
    void SetScale();

    /**
     * Calculates the fishing points awarded for this fish.
     */
    int GetFP();

    /**
     * Selects the motion played by both character forms.
     */
    void SetMotion(int motion_no, int flags);

    /**
     * Sets the position of both character forms.
     */
    void SetPosition(float *position);

    /**
     * Gets the position of the angling character form.
     */
    void GetPosition(float *position);

    /**
     * Sets the rotation of both character forms.
     */
    void SetRotation(float *rotation);

    /**
     * Gets the rotation of the angling character form.
     */
    void GetRotation(float *rotation);

    /**
     * Attaches both character frames to a reference frame.
     */
    void SetReference(CFrame *reference);

    /**
     * Detaches both character frames from their reference frames.
     */
    void DeleteReference();

    /**
     * Resets the fish's runtime state and both character forms.
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CFish) == 0x2410);

/**
 * Loads one fish kind and its character data.
 */
void LoadFish(CFish *fish, int fish_kind, int texture_slot, CDataAlloc2<1> *alloc, int loaded_before);

/**
 * Returns the character archive name for one fish kind.
 */
char *GetFishFileName(int fish_kind);

// Port additions.

/**
 * Exponent of the size lift that fixes retail's size roll (CFish::SetScale). Retail clamps every
 * roll past a kind's largest size to the largest, so the top display centimetre holds the whole
 * tail of the roll (8.5 fish in a million) and stands out against the centimetre below it, where a
 * bigger fish should be the rarer: 7.9 times as many for Baron Garayan, 2.1 for Mardan Garayan. A
 * fish rolled above its kind's usual size is moved up the range by a share
 * t^kFishSizeLiftExponent * (1 - t) of it, t being how far up it rolled, so that the centimetres
 * approaching the largest fill in to about the largest's own share and the sizes ramp into it. The
 * largest keeps exactly its retail rarity: only a roll that retail clamps lands on it.
 */
inline constexpr float kFishSizeLiftExponent = 1.2f;

/**
 * The lift's strength for a kind whose sizes span range (largest less usual, in size units): the
 * strength that leaves the centimetre below the largest holding as many fish as the largest. Wide
 * ranges are lifted more, since there the clamp stands out more; a range of 4 cm or less, none.
 */
float FishSizeLift(float range);

/**
 * Applies the size lift to a rolled size strictly between base_size (the kind's usual size) and
 * max_size; any other size is returned unchanged.
 */
float SmoothFishSize(float size, float base_size, float max_size);

} // namespace dc::game
