#pragma once

#include "common.h"

#include "frame.hpp"

namespace dc::game {

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
     */
    static s32 StopCamera;

    /**
     * Moves the eye and the look-at point one or more steps towards their
     * positions, and measures the angles of the view direction. A step count
     * below zero puts both onto their positions at once.
     */
    virtual void Step(int steps);

    /**
     * Puts the positions that the eye and the look-at point move to back onto
     * where they are, so that neither moves.
     */
    virtual void Stay();

    /**
     * Puts the eye onto a position at once.
     */
    void SetPos(CFrame *parent, float x, float y, float z);

    /**
     * Puts the eye onto a position at once.
     */
    void SetPos(float *pos);

    /**
     * Gives the eye the position that it moves to.
     */
    void SetNextPos(CFrame *parent, float x, float y, float z);

    /**
     * Puts the look-at point onto a position at once.
     */
    void SetRef(CFrame *parent, float x, float y, float z);

    /**
     * Puts the look-at point onto a position at once.
     */
    void SetRef(float *ref);

    /**
     * Puts the look-at point onto a position at once.
     */
    void SetRef(float x, float y, float z);

    /**
     * Gives the look-at point the position that it moves to.
     */
    void SetNextRef(CFrame *parent, float x, float y, float z);

    /**
     * Gives the vector that goes from the eye to the look-at point.
     */
    void GetDir(float *dir);

    /**
     * Makes the matrix that moves the world into the space of the eye, and
     * turns it by the roll angle.
     */
    void GetCameraMatrix(float (*matrix)[4]);

    /**
     * Sets the number of steps that the eye needs to reach its position.
     */
    void SetSpeed(float speed);

    /**
     * Sets the angle that turns the view about the view direction.
     */
    void SetRoll(float roll);

    /**
     * Gives the world position of the eye.
     */
    void GetPos(float *pos);

    /**
     * Gives the world position of the point that the eye looks at.
     */
    void GetRef(float *ref);

    /**
     * Returns the angle of the view direction about the vertical axis.
     */
    float GetAngleH();

    /**
     * Returns the angle that turns the view about the view direction.
     */
    float GetRoll();

    /**
     * Puts the eye at rest and gives it the number of steps that it needs to
     * reach a position.
     */
    CCamera(float speed);

    /**
     * Destroys the camera.
     */
    virtual ~CCamera();
};

STATIC_ASSERT(sizeof(CCamera) == 0x2C0);

} // namespace dc::game
