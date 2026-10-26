#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * How a bound follows its frame, as CBound::follow_mode holds it.
 */
// clang-format off
enum BoundFollowMode {
    BOUND_FOLLOW_NONE  = 0, /**< Stays put. */
    BOUND_FOLLOW_FRAME = 1, /**< Follows one frame. */
    BOUND_FOLLOW_SPAN  = 2, /**< Spans two frames. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrame;

/**
 * An oriented exclusion box used by the cloth simulation.
 */
class CBound {
public:
    CBound       *next;     /**< 0x00: Next exclusion box in the cloth list. */
    s32           mask_bit; /**< 0x04: Index of the bit a cloth vertex sets in its mask to be pushed by this box. */
    u8            unk_08[8];
    sceVu0FVECTOR extent;     /**< 0x10: Half extents of the box. */
    sceVu0FVECTOR reciprocal; /**< 0x20: Reciprocals used to normalize each axis. */
    float         friction;   /**< 0x30: Contact friction read by the cloth solver. */
    u8            unk_34[12];
    sceVu0FMATRIX matrix;      /**< 0x40: Local-to-world orientation. */
    sceVu0FMATRIX inverse;     /**< 0x80: World-to-local orientation. */
    sceVu0FVECTOR position;    /**< 0xC0: World-space center. */
    sceVu0FVECTOR direction;   /**< Unit facing direction the box's orientation was last built from. */
    s32           follow_mode; /**< How UpDate re-places the box. @see BoundFollowMode. */
    CFrame       *from_frame;  /**< Frame the box follows; in span mode, the frame its start point follows. */
    CFrame       *to_frame;    /**< Frame the end point follows in span mode. */
    u8            unk_EC[4];
    sceVu0FVECTOR from;         /**< Start point of the span, in the space of the frame it follows. */
    sceVu0FVECTOR to;           /**< End point of the span, in the space of the frame it follows. */
    sceVu0FVECTOR up;           /**< Up vector, in the followed frame's space, that settles the box's roll. */
    float         to_stretch;   /**< Factor the span is stretched by past its end point. */
    float         from_stretch; /**< Factor the span is stretched by before its start point. */

    /**
     * Returns the storage required for this cloth exclusion box.
     *
     * @mangled Sizeof__6CBoundFv
     * @address 0x13DE90
     * @size 0xC
     */
    virtual int Sizeof();

    /**
     * Tests a point against the box and writes the corrected contact point.
     *
     * @mangled InCheck__6CBoundFPfPf
     * @address 0x13D250
     * @size 0x268
     */
    virtual int InCheck(float *point, float *result);

    /**
     * Attaches the box to one frame, stores the span it follows, and sets its
     * horizontal half extents.
     *
     * @mangled SetDir__6CBoundFP6CFramePfPfPfff
     * @address 0x13D4C0
     * @size 0x100
     */
    void SetDir(CFrame *frame, float *from_position, float *to_position, float *up_direction, float half_width, float half_height);

    /**
     * Stores the endpoints and up vector used to orient the box.
     *
     * @mangled ChangeDir__6CBoundFPfPfPf
     * @address 0x13D5C0
     * @size 0x68
     */
    void ChangeDir(float *from_position, float *to_position, float *up_direction);

    /**
     * Centres the box between its two endpoints in world space and orients it
     * along the span between them.
     *
     * @mangled UpDateDir__6CBoundFv
     * @address 0x13D630
     * @size 0x208
     */
    void UpDateDir();

    /**
     * Orients the box along a facing direction, using an up vector to settle
     * the roll.
     *
     * @mangled SetDir__6CBoundFPfPf
     * @address 0x13D840
     * @size 0x60
     */
    void SetDir(float *direction, float *up_direction);

    /**
     * Normalizes the given direction into the box's facing direction and
     * rebuilds the box's orientation from it.
     *
     * @mangled SetDir__6CBoundFPf
     * @address 0x13D8A0
     * @size 0x148
     */
    void SetDir(float *direction);

    /**
     * Centres the box between the two frames its endpoints follow and stretches
     * its span by the two length scales.
     *
     * @mangled UpDateDirPos__6CBoundFv
     * @address 0x13D9F0
     * @size 0x27C
     */
    void UpDateDirPos();

    /**
     * Updates this box and the remaining boxes in its linked list.
     *
     * @mangled UpDate__6CBoundFv
     * @address 0x13DC70
     * @size 0x74
     */
    void UpDate();

    /**
     * Initializes the box's links, transform inputs, friction, and extension scales.
     *
     * @mangled InitParam__6CBoundFv
     * @address 0x13DCF0
     * @size 0x7C
     */
    void InitParam();

    /**
     * Initializes the box and gives it the half extents it excludes cloth
     * within.
     *
     * @mangled __ct__6CBoundFfff
     * @address 0x13DD70
     * @size 0x11C
     */
    CBound(float half_width, float half_height, float half_depth);
};

STATIC_ASSERT(sizeof(CBound) == 0x130);
