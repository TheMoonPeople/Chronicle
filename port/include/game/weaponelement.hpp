#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

/**
 * @file
 * Declares the sparks that an element-charged weapon throws off.
 */

/**
 * Number of sparks that one charged weapon can throw off.
 */
inline constexpr int WEAPON_ELEMENT_SPARK_MAX = 32;

/**
 * Number of bolts that the thunder element can arc between its sparks.
 */
inline constexpr int WEAPON_ELEMENT_BOLT_MAX = 16;

/**
 * Names the element that a weapon is charged with, which picks both the way
 * its sparks move and the part of the texture they draw.
 */
enum WEAPON_ELEMENT_KIND {
    WEAPON_ELEMENT_FIRE = 0,    /**< Sparks that drift upward and flicker. */
    WEAPON_ELEMENT_COLD = 1,    /**< Sparks that fall away from the blade. */
    WEAPON_ELEMENT_THUNDER = 2, /**< Sparks that fly apart, with bolts arcing between them. */
    WEAPON_ELEMENT_WIND = 3,    /**< Sparks that blow away on a rising wind, spinning. */
    WEAPON_ELEMENT_HOLY = 4,    /**< Sparks that rise slowly and fade. */
    WEAPON_ELEMENT_NONE = 5,    /**< No element. */
};

/**
 * Throws a cloud of sparks off a weapon for as long as its element is
 * charged, giving each spark the motion its element calls for and drawing
 * them all from the one "element" texture.
 */
class CWeaponElement {
public:
    sceVu0FVECTOR *origin; /**< Point the cloud is drawn around, held by the weapon. */
    u8             unk_04[0xC];
    sceVu0FVECTOR  fire_pos;                             /**< Point the fire element was started at, which its sparks stay around. */
    sceVu0FVECTOR  offset[WEAPON_ELEMENT_SPARK_MAX];     /**< Distance of each spark from the origin. */
    sceVu0FVECTOR  velocity[WEAPON_ELEMENT_SPARK_MAX];   /**< Distance each spark moves per step. */
    float          size[WEAPON_ELEMENT_SPARK_MAX];       /**< Width each spark draws at before it shrinks. */
    float          shrink[WEAPON_ELEMENT_SPARK_MAX];     /**< Share of the width left, from one down to nothing. */
    float          alpha[WEAPON_ELEMENT_SPARK_MAX];      /**< Blend each spark draws at; nothing means the slot is free. */
    float          spread;                               /**< Distance from the origin that a new spark can start. */
    s16            kind;                                 /**< Element the weapon is charged with. */
    float          power;                                /**< Strength of the charge, which sets the count and the size. */
    s16            on;                                   /**< 1 while any spark still draws. */
    s16            count;                                /**< Number of sparks the charge starts with. */
    float          scale;                                /**< Width every spark is drawn at, over the size it carries. */
    float          spin[WEAPON_ELEMENT_SPARK_MAX];       /**< Angle each spark is turned about the vertical. */
    float          spin_speed[WEAPON_ELEMENT_SPARK_MAX]; /**< Angle the spin turns each step. */
    s16            spawn_delay_max;                      /**< Longest wait between two sparks being started again. */
    s16            spawn_delay;                          /**< Steps left before the next spark is started again. */
    s16            spawn_budget;                         /**< Number of sparks the charge can still start again. */
    s16            fading[WEAPON_ELEMENT_SPARK_MAX];     /**< 0 while a spark brightens, 1 once it is fading out. */
    s16            frame[WEAPON_ELEMENT_SPARK_MAX];      /**< Row of the texture each spark draws. */
    s16            frame_timer;                          /**< Counts four steps down, and the sparks pick new rows on the fourth. */
    s16            bolt_head[WEAPON_ELEMENT_BOLT_MAX];   /**< Spark each bolt arcs from. */
    s16            bolt_tail[WEAPON_ELEMENT_BOLT_MAX];   /**< Spark each bolt arcs to. */
    s16            bolt_timer[WEAPON_ELEMENT_BOLT_MAX];  /**< Steps left before a bolt picks new ends. */
    s16            bolt_frame[WEAPON_ELEMENT_BOLT_MAX];  /**< Part of the texture each bolt draws. */
    s16            bolt_count;                           /**< Number of bolts the charge arcs between its sparks. */

    /**
     * Frees every spark slot, so that the cloud throws nothing.
     */
    void Initialize();

    /**
     * Charges the cloud with one element and starts its first sparks.
     */
    void Set(sceVu0FVECTOR *origin, float *position, float power, int kind, float spread);

    /**
     * Moves the cloud one step, by the way the charged element calls for.
     */
    void Step();

    /**
     * Draws the cloud, by the way the charged element calls for.
     */
    void Draw();

    /**
     * Starts a cloud of sparks that rise slowly away from the blade.
     */
    void Init_Holy(float *position);

    /**
     * Raises every holy spark, and starts free slots again while the budget lasts.
     */
    void Step_Holy();

    /**
     * Draws every holy spark that still carries a blend.
     */
    void Draw_Holy();

    /**
     * Starts a cloud of sparks that fall away from the blade.
     */
    void Init_Cold(float *position);

    /**
     * Drops every cold spark, and starts free slots again while the budget lasts.
     */
    void Step_Cold();

    /**
     * Draws every cold spark that still carries a blend.
     */
    void Draw_Cold();

    /**
     * Starts a cloud of sparks that blow away on a rising wind, each spinning.
     */
    void Init_Wind(float *position);

    /**
     * Blows every wind spark along, turning it, and starts free slots again.
     */
    void Step_Wind();

    /**
     * Draws every wind spark, turned about the vertical by the spin it carries.
     */
    void Draw_Wind();

    /**
     * Starts a cloud of sparks that drift upward off the blade.
     */
    void Init_Fire(float *position);

    /**
     * Raises every fire spark, and starts free slots again while the budget lasts.
     */
    void Step_Fire();

    /**
     * Draws every fire spark that still carries a blend.
     */
    void Draw_Fire();

    /**
     * Starts a cloud of sparks that fly apart, and the bolts that arc between them.
     */
    void Init_Thunder(float *position);

    /**
     * Carries every thunder spark along its own line, and gives the bolts new ends.
     */
    void Step_Thunder();

    /**
     * Draws every thunder spark, then a bolt across each pair the charge picked.
     */
    void Draw_Thunder();
};

STATIC_ASSERT(sizeof(CWeaponElement) == 0x7C0);

/**
 * Elemental weapon effects the dungeon can run.
 */
extern CWeaponElement CWeaponElFx[4];

} // namespace dc::game
