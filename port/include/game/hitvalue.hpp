#pragma once

#include "common.h"

#include <libvu0.h>

#include "rect.hpp"

/**
 * Kinds of floating hit value, as CHitValue::kind holds them.
 */
// clang-format off
enum HitValueKind {
    HIT_VALUE_ZERO    = -1, /**< No damage. */
    HIT_VALUE_MONSTER = 0,  /**< Damage to a monster. */
    HIT_VALUE_GOLD    = 1,  /**< Money. */
    HIT_VALUE_PLAYER  = 2,  /**< Damage to the player. */
};

// clang-format on

class CFrame;

/**
 * Holds one damage number standing over whatever took the hit.
 */
class CHitValue {
public:
    sceVu0FVECTOR pos;   /**< World position the number is drawn above. */
    CFrame       *frame; /**< Frame the number follows, or zero to stand still. */

    union {
        float digit_angle[5]; /**< Hop angle of each place, rising to pi. */
        float phase[5];       /**< The hop angles, as the entry and draw routines name them. */
    };

    union {
        s32 digits[5]; /**< Value of each place; -1 skips its hop and -2 draws the special mark. */
        s32 digit[5];  /**< The place values, as the entry and draw routines name them. */
    };

    union {
        float alpha;   /**< Opacity of the number, from zero to 128. */
        float opacity; /**< The opacity, as the entry and draw routines name it. */
    };

    union {
        float alpha_speed;   /**< Amount the opacity changes by each step. */
        float opacity_speed; /**< The opacity change per step, as the entry routine names it. */
    };

    union {
        s32 last_digit; /**< Place whose finished hop starts the fade. */
        s32 unk_44;
    };

    s32  kind;   /**< What took the hit, which picks the digit sheet. @see HitValueKind. */
    RECT texel;  /**< Rectangle of the sheet one digit is cut from. */
    s32  active; /**< Indicates that the slot is showing a number. */

    /**
     * Puts one damage number on the screen over what took the hit.
     *
     * @mangled EntryValue__9CHitValueFPfiiP6CFrame
     * @address 0x1B6370
     * @size 0x1E4
     */
    void EntryValue(float *world, int amount, int kind, CFrame *frame);

    /**
     * Draws the damage numbers standing on the screen.
     *
     * @mangled Draw__9CHitValueFv
     * @address 0x1B6560
     * @size 0x2F0
     */
    void Draw();

    /**
     * Advances the damage numbers' rise and fade.
     *
     * @mangled Step__9CHitValueFv
     * @address 0x1B6850
     * @size 0x198
     */
    void Step();
};

/**
 * Puts one damage number on the screen over what took the hit.
 *
 * @mangled HitValueEntry__FP9CHitValuePfiiP6CFrame
 * @address 0x1B6300
 * @size 0x64
 * @unknownret
 */
void HitValueEntry(CHitValue *values, float *world, int amount, int kind, CFrame *frame);

STATIC_ASSERT(sizeof(CHitValue) == 0x60);

/** Hit attribute bit of each weapon element. */
extern int element_tbl[6];

/**
 * Default weapon of each character, as the hit-value unit holds it.
 */
extern int defWeapon__2[6];
