#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

class CFrame;

/**
 * How a bound follows its frame, as CBound::follow_mode holds it.
 */
enum class BoundFollowMode : s32 {
    None = 0,  /**< Stays put. */
    Frame = 1, /**< Follows one frame. */
    Span = 2,  /**< Spans two frames. */
};

/**
 * An oriented exclusion box used by the cloth simulation.
 */
class CBound {
public:
    CBound         *next;     /**< 0x00: Next exclusion box in the cloth list. */
    s32             mask_bit; /**< 0x04: Index of the bit a cloth vertex sets in its mask to be pushed by this box. */
    u8              unk_08[8];
    sceVu0FVECTOR   extent;     /**< 0x10: Half extents of the box. */
    sceVu0FVECTOR   reciprocal; /**< 0x20: Reciprocals used to normalize each axis. */
    float           friction;   /**< 0x30: Contact friction read by the cloth solver. */
    u8              unk_34[12];
    sceVu0FMATRIX   matrix;      /**< 0x40: Local-to-world orientation. */
    sceVu0FMATRIX   inverse;     /**< 0x80: World-to-local orientation. */
    sceVu0FVECTOR   position;    /**< 0xC0: World-space center. */
    sceVu0FVECTOR   direction;   /**< Unit facing direction the box's orientation was last built from. */
    BoundFollowMode follow_mode; /**< How UpDate re-places the box. */
    CFrame         *from_frame;  /**< Frame the box follows; in span mode, the frame its start point follows. */
    CFrame         *to_frame;    /**< Frame the end point follows in span mode. */
    u8              unk_EC[4];
    sceVu0FVECTOR   from;         /**< Start point of the span, in the space of the frame it follows. */
    sceVu0FVECTOR   to;           /**< End point of the span, in the space of the frame it follows. */
    sceVu0FVECTOR   up;           /**< Up vector, in the followed frame's space, that settles the box's roll. */
    float           to_stretch;   /**< Factor the span is stretched by past its end point. */
    float           from_stretch; /**< Factor the span is stretched by before its start point. */

    /**
     * Returns the storage required for this cloth exclusion box.
     */
    virtual int Sizeof();

    /**
     * Tests a point against the box and writes the corrected contact point.
     */
    virtual int InCheck(float *point, float *result);

    /**
     * Attaches the box to one frame, stores the span it follows, and sets its
     * horizontal half extents.
     */
    void SetDir(CFrame *frame, float *from_position, float *to_position, float *up_direction, float half_width, float half_height);

    /**
     * Stores the endpoints and up vector used to orient the box.
     */
    void ChangeDir(float *from_position, float *to_position, float *up_direction);

    /**
     * Centres the box between its two endpoints in world space and orients it
     * along the span between them.
     */
    void UpDateDir();

    /**
     * Orients the box along a facing direction, using an up vector to settle
     * the roll.
     */
    void SetDir(float *direction, float *up_direction);

    /**
     * Normalizes the given direction into the box's facing direction and
     * rebuilds the box's orientation from it.
     */
    void SetDir(float *direction);

    /**
     * Centres the box between the two frames its endpoints follow and stretches
     * its span by the two length scales.
     */
    void UpDateDirPos();

    /**
     * Updates this box and the remaining boxes in its linked list.
     */
    void UpDate();

    /**
     * Initializes the box's links, transform inputs, friction, and extension scales.
     */
    void InitParam();

    /**
     * Initializes the box and gives it the half extents it excludes cloth
     * within.
     */
    CBound(float half_width, float half_height, float half_depth);
};

STATIC_ASSERT(sizeof(CBound) == 0x130);

} // namespace dc::game
