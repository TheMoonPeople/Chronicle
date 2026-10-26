#pragma once

#include "common.h"

#include "frame.hpp"

/**
 * @file
 * Declares the camera that gives the view of the world.
 */

/**
 * Moves an eye point and a look-at point towards the positions that
 * the game gives, and makes the matrix that puts the world in front
 * of the eye.
 */
class CCamera {
public:
    CFrame frame;       /**< Puts the camera in the frame hierarchy. */
    float  pos[4];      /**< World position of the eye. */
    float  ref[4];      /**< World position of the point that the eye looks at. */
    float  next_pos[4]; /**< World position that the eye moves to. */
    float  next_ref[4]; /**< World position that the look-at point moves to. */
    float  roll;        /**< Angle, in radians, that turns the view about the view direction. */
    s32    limit_move;  /**< 1 to move the eye no more than two units in one step. */
    float  speed;       /**< Number of steps that the eye needs to reach its position; 1.0 moves it at once. */
    float  angle_h;     /**< Angle, in radians, of the view direction about the vertical axis. */
    float  angle_v;     /**< Angle, in radians, of the view direction above the horizontal plane. */
    float  snap_range;  /**< Distance at which the eye and the look-at point snap onto their positions. */

    /**
     * Holds every camera still while it is not zero.
     *
     * @mangled StopCamera__7CCamera
     * @address 0x2A2380
     * @size 0x4
     */
    static s32 StopCamera;

    /**
     * Moves the eye and the look-at point one or more steps towards their
     * positions, and measures the angles of the view direction. A step count
     * below zero puts both onto their positions at once.
     *
     * @mangled Step__7CCameraFi
     * @address 0x123F30
     * @size 0x2D8
     */
    virtual void Step(int steps);

    /**
     * Puts the positions that the eye and the look-at point move to back onto
     * where they are, so that neither moves.
     *
     * @mangled Stay__7CCameraFv
     * @address 0x124210
     * @size 0x44
     */
    virtual void Stay();

    /**
     * Puts the eye onto a position at once.
     *
     * @mangled SetPos__7CCameraFP6CFramefff
     * @address 0x124260
     * @size 0x2C
     */
    void SetPos(CFrame *parent, float x, float y, float z);

    /**
     * Puts the eye onto a position at once.
     *
     * @mangled SetPos__7CCameraFPf
     * @address 0x124290
     * @size 0x34
     */
    void SetPos(float *pos);

    /**
     * Gives the eye the position that it moves to.
     *
     * @mangled SetNextPos__7CCameraFP6CFramefff
     * @address 0x1242D0
     * @size 0x14
     */
    void SetNextPos(CFrame *parent, float x, float y, float z);

    /**
     * Puts the look-at point onto a position at once.
     *
     * @mangled SetRef__7CCameraFP6CFramefff
     * @address 0x1242F0
     * @size 0x20
     */
    void SetRef(CFrame *parent, float x, float y, float z);

    /**
     * Puts the look-at point onto a position at once.
     *
     * @mangled SetRef__7CCameraFPf
     * @address 0x124310
     * @size 0x34
     */
    void SetRef(float *ref);

    /**
     * Puts the look-at point onto a position at once.
     *
     * @mangled SetRef__7CCameraFfff
     * @address 0x124350
     * @size 0x24
     */
    void SetRef(float x, float y, float z);

    /**
     * Gives the look-at point the position that it moves to.
     *
     * @mangled SetNextRef__7CCameraFP6CFramefff
     * @address 0x124380
     * @size 0x14
     */
    void SetNextRef(CFrame *parent, float x, float y, float z);

    /**
     * Gives the vector that goes from the eye to the look-at point.
     *
     * @mangled GetDir__7CCameraFPf
     * @address 0x1243A0
     * @size 0x38
     */
    void GetDir(float *dir);

    /**
     * Makes the matrix that moves the world into the space of the eye, and
     * turns it by the roll angle.
     *
     * @mangled GetCameraMatrix__7CCameraFPA4_f
     * @address 0x1243E0
     * @size 0x26C
     */
    void GetCameraMatrix(float (*matrix)[4]);

    /**
     * Sets the number of steps that the eye needs to reach its position.
     *
     * @mangled SetSpeed__7CCameraFf
     * @address 0x124650
     * @size 0xC
     */
    void SetSpeed(float speed);

    /**
     * Sets the angle that turns the view about the view direction.
     *
     * @mangled SetRoll__7CCameraFf
     * @address 0x124660
     * @size 0xC
     */
    void SetRoll(float roll);

    /**
     * Gives the world position of the eye.
     *
     * @mangled GetPos__7CCameraFPf
     * @address 0x124670
     * @size 0x2C
     */
    void GetPos(float *pos);

    /**
     * Gives the world position of the point that the eye looks at.
     *
     * @mangled GetRef__7CCameraFPf
     * @address 0x1246A0
     * @size 0x2C
     */
    void GetRef(float *ref);

    /**
     * Returns the angle of the view direction about the vertical axis.
     *
     * @mangled GetAngleH__7CCameraFv
     * @address 0x1246D0
     * @size 0xC
     */
    float GetAngleH();

    /**
     * Returns the angle that turns the view about the view direction.
     *
     * @mangled GetRoll__7CCameraFv
     * @address 0x1246E0
     * @size 0xC
     */
    float GetRoll();

    /**
     * Puts the eye at rest and gives it the number of steps that it needs to
     * reach a position.
     *
     * @mangled __ct__7CCameraFf
     * @address 0x1246F0
     * @size 0x80
     */
    CCamera(float speed);

    /**
     * @mangled __dt__7CCameraFv
     * @address 0x124770
     * @size 0x54
     */
    virtual ~CCamera();
};

STATIC_ASSERT(sizeof(CCamera) == 0x2C0);
