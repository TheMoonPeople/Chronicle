#pragma once

#include "common.h"

#include "camera.hpp"

namespace dc::game {

/**
 * @file
 * Declares the camera that circles a point in the world.
 */

/**
 * Follows a point in the world, keeping the eye on that point. The eye
 * sits on a circle about the point, at a set height and distance.
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
     */
    CCameraFollow &operator=(const CCameraFollow &src);

    /**
     * Turns the eye one or more steps towards the angle that it turns to, and
     * puts the eye and the look-at point where the circle wants them. A step
     * count below zero turns the eye at once.
     */
    void Step(int steps) override;

    /**
     * Puts the position that the eye circles onto the point that the eye
     * looks at, and the angle that the eye turns to onto its angle, so that
     * nothing moves.
     */
    void Stay() override;

    /**
     * Sets the world position that the eye circles.
     */
    void SetFollow(float x, float y, float z);

    /**
     * Makes the eye circle the position again.
     */
    void FollowOn();

    /**
     * Leaves the eye where it is, and makes it take the angle that the base
     * camera measures.
     */
    void FollowOff();

    /**
     * Sets the angle that the eye turns to, which it reaches over several
     * steps.
     */
    void SetAngle(float angle);

    /**
     * Sets the angle of the eye, and the angle that it turns to, so that the
     * eye moves at once.
     */
    void SetAngleSoon(float angle);

    /**
     * Returns the angle that the eye is at now.
     */
    float GetAngle();

    /**
     * Adds to the angle that the eye turns to.
     */
    void AddAngle(float delta);

    /**
     * Sets the distance from the eye to the position that it circles.
     */
    void SetDistance(float distance);

    /**
     * Returns the distance from the eye to the position that it circles.
     */
    float GetDistance();

    /**
     * Adds to the distance from the eye to the position that it circles.
     */
    void AddDistance(float delta);

    /**
     * Sets the height of the eye above the position that it circles.
     */
    void SetHeight(float height);

    /**
     * Returns the height of the eye above the position that it circles.
     */
    float GetHeight();

    /**
     * Adds to the height of the eye above the position that it circles.
     */
    void AddHeight(float delta);

    /**
     * Puts the eye on the circle that the given distance, height and angle
     * describe, about the origin.
     */
    CCameraFollow(float distance, float height, float angle, float speed);

    /**
     * Destroys the camera.
     */
    ~CCameraFollow() override;
};

} // namespace dc::game
