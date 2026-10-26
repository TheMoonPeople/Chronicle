#pragma once

#include "common.h"

#include "object.hpp"

/**
 * @file
 * Declares the object that one or more frames draw.
 */

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrameVu1;

/**
 * Draws an object with up to four frames, one per level of detail, and keeps
 * each frame on the position, the angle and the scale that the object has.
 */
class CObjectFrame : public CObject {
public:
    CFrameVu1 *frame[4];         /**< Frame of each level of detail; zero where the object has none. */
    s32        rotation_changed; /**< Set whenever the angle, its motion or the moment changes. */
    s32        draw_on;          /**< 1 while the object draws; 0 leaves it out of the scene. */

    /**
     * Sets the moment of inertia about each axis.
     *
     * @mangled SetMoment__12CObjectFrameF11CVector3_f_
     * @address 0x1579E0
     * @size 0x38
     */
    virtual void SetMoment(CVector3_f_ moment);

    /**
     * Turns the object to an angle about each axis.
     *
     * @mangled SetRotation__12CObjectFrameF11CVector3_f_
     * @address 0x157A40
     * @size 0x38
     */
    virtual void SetRotation(CVector3_f_ rotation);

    /**
     * Turns the object to an angle about each axis.
     *
     * @mangled SetRotation__12CObjectFrameFfff
     * @address 0x157A20
     * @size 0x1C
     */
    virtual void SetRotation(float x, float y, float z);

    /**
     * Sets the angle that the rotation turns each step.
     *
     * @mangled SetRotVelocity__12CObjectFrameF11CVector3_f_
     * @address 0x157A80
     * @size 0x38
     */
    virtual void SetRotVelocity(CVector3_f_ rot_velocity);

    /**
     * Sets the angle that the rotational velocity gains each step.
     *
     * @mangled SetRotAcceleration__12CObjectFrameF11CVector3_f_
     * @address 0x157AC0
     * @size 0x38
     */
    virtual void SetRotAcceleration(CVector3_f_ rot_acceleration);

    /**
     * Turns a named part of every frame on or off.
     *
     * @mangled FrameObjectOnOff__12CObjectFrameFPci
     * @address 0x1570F0
     * @size 0x94
     */
    virtual void FrameObjectOnOff(char *name, int on);

    /**
     * Puts the first frame where the object is and draws it.
     *
     * @mangled Draw__12CObjectFrameFv
     * @address 0x157190
     * @size 0x84
     */
    virtual void Draw();

    /**
     * Draws the frame whose level of detail suits the distance from the eye,
     * and gives back the level that it drew.
     *
     * @mangled DrawLOD__12CObjectFrameFPfiiPi
     * @address 0x157220
     * @size 0x240
     */
    virtual void DrawLOD(float *distance, int lowest, int highest, int *out_level);

    /**
     * Puts a frame at a level of detail.
     *
     * @mangled SetFrame__12CObjectFrameFP9CFrameVu1i
     * @address 0x1570C0
     * @size 0x28
     */
    void SetFrame(CFrameVu1 *frame, int level);

    /**
     * Puts the object at rest at the world origin and gives it one frame.
     *
     * @mangled Initialize__12CObjectFrameFP9CFrameVu1
     * @address 0x157460
     * @size 0x84
     */
    void Initialize(CFrameVu1 *frame);

    /**
     * Makes an object of unit mass that no frame draws yet.
     */
    CObjectFrame() : CObject(1.0f) {
        Initialize(NULL);
    }
};

STATIC_ASSERT(sizeof(CObjectFrame) == 0xD0);
