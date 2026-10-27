#pragma once

#include "common.h"

#include "object.hpp"

namespace dc::game {

/**
 * @file
 * Declares the object that one or more frames draw.
 */

class CFrameVu1;

/**
 * Draws an object with up to four frames, one per level of detail, and keeps
 * each frame on the position, the angle and the scale that the object has.
 */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; null where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    /**
     * Sets the moment of inertia about each axis.
     */
    virtual void SetMoment(CVector3_f_ moment);

    /**
     * Turns the object to an angle about each axis.
     */
    virtual void SetRotation(CVector3_f_ rotation);

    /**
     * Turns the object to an angle about each axis.
     */
    virtual void SetRotation(float x, float y, float z);

    /**
     * Sets the angle that the rotation turns each step.
     */
    virtual void SetRotVelocity(CVector3_f_ rot_velocity);

    /**
     * Sets the angle that the rotational velocity gains each step.
     */
    virtual void SetRotAcceleration(CVector3_f_ rot_acceleration);

    /**
     * Turns a named part of every frame on or off.
     */
    virtual void FrameObjectOnOff(char *name, int on);

    /**
     * Puts the first frame where the object is and draws it.
     */
    virtual void Draw();

    /**
     * Draws the frame whose level of detail suits the distance from the eye,
     * and gives back the level that it drew.
     */
    virtual void DrawLOD(float *distance, int lowest, int highest, int *out_level);

    /**
     * Puts a frame at a level of detail.
     */
    void SetFrame(CFrameVu1 *frame, int level);

    /**
     * Puts the object at rest at the world origin and gives it one frame.
     */
    void Initialize(CFrameVu1 *frame);

    /**
     * Makes an object of unit mass that no frame draws yet.
     */
    CObjectFrame() : CObject(1.0f) {
        Initialize(nullptr);
    }
};

STATIC_ASSERT(sizeof(CObjectFrame) == 0xD0);

} // namespace dc::game
