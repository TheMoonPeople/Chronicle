#pragma once

#include "common.h"

#include <libvu0.h>

#include "object.hpp"

/**
 * @file
 * Declares the marks that a hit throws off.
 */

/**
 * Kinds of hit mark, as CHitMark::kind holds them.
 */
// clang-format off
enum HitMarkKind {
    HIT_MARK_HIT   = 0, /**< Hit. */
    HIT_MARK_UNK_1 = 1, /**< Never set. */
    HIT_MARK_GUARD = 2, /**< Guard. */
    HIT_MARK_UNK_3 = 3, /**< 32x32 half-transparent mark; never set. */
    HIT_MARK_UNK_4 = 4, /**< 24x24 mark; never set. */
};

// clang-format on

/** Number of marks that one hit can throw off. */
#define HIT_MARK_MAX 32

/**
 * Throws a burst of sprites off the point of a hit and carries each one until
 * it shrinks away.
 */
class CHitMark : public CObject {
public:
    sceVu0FVECTOR offset[HIT_MARK_MAX];   /**< Distance of each mark from the point of the hit. */
    sceVu0FVECTOR velocity[HIT_MARK_MAX]; /**< Distance that each mark moves each step. */
    float         size[HIT_MARK_MAX];     /**< Width of each mark on the screen. */
    u8            unk_530[144];
    float         shrink;             /**< Size each mark loses each step. */
    float         spread;             /**< Scatter added to each mark's launch direction; narrows each step. */
    float         gravity;            /**< Downward speed that a mark gains each step. */
    float         speed;              /**< Speed that the burst throws its marks at. */
    s32           capacity;           /**< Number of mark slots, set when the burst is emptied. */
    s32           used[HIT_MARK_MAX]; /**< 1 while the mark of the slot still draws. */
    s32           count;              /**< Number of marks that still draw. */
    s32           kind;               /**< Part of the texture that every mark draws. @see HitMarkKind. */
    float         floor_y;            /**< Height that a mark bounces off. */

    /**
     * Moves every mark, and drops the ones that have shrunk away.
     *
     * @mangled Step__8CHitMarkFv
     * @address 0x1B3490
     * @size 0x13C
     */
    virtual void Step();

    /**
     * Throws a burst of marks off a point, each one in its own direction.
     *
     * @mangled Set__8CHitMarkFPfPfiffffif
     * @address 0x1B2E90
     * @size 0x310
     */
    void Set(float *position, float *direction, int kind, float spread, float shrink, float gravity, float speed, int count, float floor_y);

    /**
     * Draws every mark that the burst still holds.
     *
     * @mangled Draw__8CHitMarkFv
     * @address 0x1B31A0
     * @size 0x2EC
     */
    void Draw();

    /**
     * Empties every slot, so that the burst throws nothing.
     *
     * @mangled Initialize__8CHitMarkFv
     * @address 0x1B35D0
     * @size 0x3C
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CHitMark) == 0x660);

/**
 * Blinks one mark over the point that the player last hit.
 */
class CHitPointMark {
public:
    float pos[4]; /**< World position of the mark. */
    s32   timer;  /**< Steps that the mark still draws for. */
    s32   blink;  /**< 1 while the mark shows; 0 while it is hidden. */
    s32   on;     /**< 1 while the mark is in use. */
    s32   unk_1C;

    /**
     * Starts the mark over a point.
     */
    void Set(float *position) {
        sceVu0CopyVector(pos, position);
        on = 1;
        blink = 0;
        timer = 16;
    }

    /**
     * Draws the mark, unless the blink hides it.
     *
     * @mangled Draw__13CHitPointMarkFv
     * @address 0x1B3610
     * @size 0xFC
     */
    void Draw();

    /**
     * Counts the mark down, and turns the blink over every fourth step.
     *
     * @mangled Step__13CHitPointMarkFv
     * @address 0x1B3710
     * @size 0x64
     */
    void Step();
};

STATIC_ASSERT(sizeof(CHitPointMark) == 0x20);

/** Marks drawn where the player's hits land. */
extern "C" CHitMark HitMark[16];

/** Marks blinked where the player's hits land. */
extern "C" CHitPointMark HitPointMark[16];

/** Mark that the next hit takes. */
extern "C" int hitCnt;

/**
 * Debug overlay line format of the mini-map view entry.
 */
extern char DebugInfoMsgMiniMapView[];

/**
 * Debug overlay line format of the collision display entry.
 */
extern char DebugInfoMsgCollision[];

/**
 * Debug overlay line format of the BGM play entry.
 */
extern char DebugInfoMsgBgmPlay[];

/**
 * Debug overlay line format of the parameter entry.
 */
extern char DebugInfoMsgParameter[];

/**
 * Debug overlay line format of the view info entry.
 */
extern char DebugInfoMsgViewInfo[];

/**
 * Debug overlay line format of the invincibility entry.
 */
extern char DebugInfoMsgUltraMan[];

/**
 * Debug overlay line format of the enemy reload entry.
 */
extern char DebugInfoMsgReloadEnemy[];

/**
 * Debug overlay line format of the item put zone entry.
 */
extern char DebugInfoMsgItemPutZone[];

/**
 * Debug overlay line format of the light mode entry.
 */
extern char DebugInfoMsgLightMode[];

/**
 * Debug overlay line format of the floor Atlamillia get entry.
 */
extern char DebugInfoMsgFloorAtraGet[];

/**
 * Debug overlay line format of the event test entry.
 */
extern char DebugInfoMsgEventTest[];

/**
 * Debug overlay line format of the set status entry.
 */
extern char DebugInfoMsgSetStatus[];

/**
 * Debug overlay line format of the SE play entry.
 */
extern char DebugInfoMsgSePlay[];

/**
 * Debug overlay line format of the set character key entry.
 */
extern char DebugInfoMsgSetChrKey[];

/**
 * Name of the heal effect's texture.
 */
extern char HealEffectTextureName[];

/**
 * Virtual table of CHitMark under the name main's array constructors install it by.
 */
extern "C" void *GeneratedHitMarkVtable[];
