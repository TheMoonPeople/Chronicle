#pragma once

#include "common.h"

#include "camera.hpp"

/**
 * @file
 * Declares the camera that circles a point in the world.
 */

/**
 * Follows a point in the world, keeping the eye on that point. The eye sits on a circle about the point, at a set height and distance.
 */
class CCameraFollow : public CCamera {
public:
    float follow[4];  /**< World position that the eye circles and looks at. */
    float distance;   /**< Distance from the eye to the position, on the horizontal plane. */
    float height;     /**< Height of the eye above the position. */
    float next_angle; /**< Angle, in radians, that the eye turns to. */
    float angle;      /**< Angle, in radians, of the eye about the position. */
    s32   follow_on;  /**< 1 while the eye circles the position; 0 leaves the eye where it is. */

    /**
     * Copies the position, the distance and the height of another camera.
     *
     * @mangled __as__13CCameraFollowFRC13CCameraFollow
     * @address 0x124AA0
     * @size 0x3C
     */
    CCameraFollow &operator=(const CCameraFollow &src);

    /**
     * Turns the eye one or more steps towards the angle that it turns to, and
     * puts the eye and the look-at point where the circle wants them. A step
     * count below zero turns the eye at once.
     *
     * @mangled Step__13CCameraFollowFi
     * @address 0x1247D0
     * @size 0x27C
     */
    virtual void Step(int steps);

    /**
     * Puts the position that the eye circles onto the point that the eye
     * looks at, and the angle that the eye turns to onto its angle, so that
     * nothing moves.
     *
     * @mangled Stay__13CCameraFollowFv
     * @address 0x124A50
     * @size 0x50
     */
    virtual void Stay();

    /**
     * Sets the world position that the eye circles.
     *
     * @mangled SetFollow__13CCameraFollowFfff
     * @address 0x124AE0
     * @size 0x14
     */
    void SetFollow(float x, float y, float z);

    /**
     * Makes the eye circle the position again.
     *
     * @mangled FollowOn__13CCameraFollowFv
     * @address 0x124B00
     * @size 0x10
     */
    void FollowOn();

    /**
     * Leaves the eye where it is, and makes it take the angle that the base
     * camera measures.
     *
     * @mangled FollowOff__13CCameraFollowFv
     * @address 0x124B10
     * @size 0xC
     */
    void FollowOff();

    /**
     * Sets the angle that the eye turns to, which it reaches over several
     * steps.
     *
     * @mangled SetAngle__13CCameraFollowFf
     * @address 0x124B20
     * @size 0xC
     */
    void SetAngle(float angle);

    /**
     * Sets the angle of the eye, and the angle that it turns to, so that the
     * eye moves at once.
     *
     * @mangled SetAngleSoon__13CCameraFollowFf
     * @address 0x124B30
     * @size 0x10
     */
    void SetAngleSoon(float angle);

    /**
     * Returns the angle that the eye is at now.
     *
     * @mangled GetAngle__13CCameraFollowFv
     * @address 0x124B40
     * @size 0xC
     */
    float GetAngle();

    /**
     * Adds to the angle that the eye turns to.
     *
     * @mangled AddAngle__13CCameraFollowFf
     * @address 0x124B50
     * @size 0x14
     */
    void AddAngle(float delta);

    /**
     * Sets the distance from the eye to the position that it circles.
     *
     * @mangled SetDistance__13CCameraFollowFf
     * @address 0x124B70
     * @size 0xC
     */
    void SetDistance(float distance);

    /**
     * Returns the distance from the eye to the position that it circles.
     *
     * @mangled GetDistance__13CCameraFollowFv
     * @address 0x124B80
     * @size 0xC
     */
    float GetDistance();

    /**
     * Adds to the distance from the eye to the position that it circles.
     *
     * @mangled AddDistance__13CCameraFollowFf
     * @address 0x124B90
     * @size 0x14
     */
    void AddDistance(float delta);

    /**
     * Sets the height of the eye above the position that it circles.
     *
     * @mangled SetHeight__13CCameraFollowFf
     * @address 0x124BB0
     * @size 0xC
     */
    void SetHeight(float height);

    /**
     * Returns the height of the eye above the position that it circles.
     *
     * @mangled GetHeight__13CCameraFollowFv
     * @address 0x124BC0
     * @size 0xC
     */
    float GetHeight();

    /**
     * Adds to the height of the eye above the position that it circles.
     *
     * @mangled AddHeight__13CCameraFollowFf
     * @address 0x124BD0
     * @size 0x14
     */
    void AddHeight(float delta);

    /**
     * Puts the eye on the circle that the given distance, height and angle
     * describe, about the origin.
     *
     * @mangled __ct__13CCameraFollowFffff
     * @address 0x124BF0
     * @size 0x88
     */
    CCameraFollow(float distance, float height, float angle, float speed);

    /**
     * @mangled __dt__13CCameraFollowFv
     * @address 0x124C80
     * @size 0x70
     */
    virtual ~CCameraFollow();
};
