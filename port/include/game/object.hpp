#pragma once

#include "common.h"

#include <libvu0.h>

#include "vector3.hpp"

namespace dc::game {

class CFrameAttr;

/**
 * Places one object in the world.
 */
class CObject {
public:
    float       mass; /**< Mass of the object. */
    float       unk_004[3];
    float       pos[4];           /**< World position of the object. */
    CVector3_f_ velocity;         /**< Distance that the position moves each step. */
    CVector3_f_ acceleration;     /**< Distance that the velocity gains each step. */
    CVector3_f_ gravity;          /**< Part of the velocity that gravity gives. */
    CVector3_f_ moment;           /**< Moment of inertia about each axis. */
    CVector3_f_ rotation;         /**< Angle, in radians, about each axis. */
    CVector3_f_ rot_velocity;     /**< Angle that the rotation turns each step. */
    CVector3_f_ rot_acceleration; /**< Angle that the rotational velocity gains each step. */
    float       scale[4];         /**< Scale of the object along each axis. */

    /**
     * Copies one object over another, field by field.
     */
    CObject &operator=(const CObject &source);

    /**
     * Steps the object. The base object does nothing.
     */
    virtual void Step();

    /**
     * Gets the distance between this object and another.
     */
    float GetDistance(CObject &other);

    /**
     * Gets the direction from this object to another.
     */
    void GetDir(CObject &other, float *out_direction);

    /**
     * Sets the mass of the object.
     */
    virtual void SetMass(float mass);

    /**
     * Puts the object at a world position.
     */
    virtual void SetPosition(CVector3_f_ position);

    /**
     * Puts the object at a world position.
     */
    virtual void SetPosition(float *position);

    /**
     * Puts the object at a world position.
     */
    virtual void SetPosition(float x, float y, float z);

    /**
     * Sets the distance that the position moves each step.
     */
    virtual void SetVelocity(CVector3_f_ velocity);

    /**
     * Sets the distance that the velocity gains each step.
     */
    virtual void SetAcceleration(CVector3_f_ acceleration);

    /**
     * Sets the part of the velocity that gravity gives.
     */
    virtual void SetGravity(CVector3_f_ gravity);

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
     * Turns the object to an angle about each axis.
     */
    virtual void SetRotation(float *rotation);

    /**
     * Sets the angle that the rotation turns each step.
     */
    virtual void SetRotVelocity(CVector3_f_ rot_velocity);

    /**
     * Sets the angle that the rotational velocity gains each step.
     */
    virtual void SetRotAcceleration(CVector3_f_ rot_acceleration);

    /**
     * Gets the mass of the object.
     */
    virtual float GetMass();

    /**
     * Gets the world position of the object.
     */
    virtual void GetPosition(CVector3_f_ *out_position);

    /**
     * Gets the distance that the position moves each step.
     */
    virtual void GetVelocity(CVector3_f_ *out_velocity);

    /**
     * Gets the distance that the velocity gains each step.
     */
    virtual void GetAcceleration(CVector3_f_ *out_acceleration);

    /**
     * Gets the part of the velocity that gravity gives.
     */
    virtual void GetGravity(CVector3_f_ *out_gravity);

    /**
     * Gets the moment of inertia about each axis.
     */
    virtual void GetMoment(CVector3_f_ *out_moment);

    /**
     * Gets the angle of the object about each axis.
     */
    virtual void GetRotation(float *out_rotation);

    /**
     * Gets the angle of the object about each axis.
     */
    virtual void GetRotation(CVector3_f_ *out_rotation);

    /**
     * Gets the angle that the rotation turns each step.
     */
    virtual void GetRotVelocity(CVector3_f_ *out_rot_velocity);

    /**
     * Gets the angle that the rotational velocity gains each step.
     */
    virtual void GetRotAcceleration(CVector3_f_ *out_rot_acceleration);

    /**
     * Gets the distance that the position moves each step.
     */
    virtual CVector3_f_ *GetVelocity();

    /**
     * Gets the distance that the velocity gains each step.
     */
    virtual CVector3_f_ *GetAcceleration();

    /**
     * Gets the part of the velocity that gravity gives.
     */
    virtual CVector3_f_ *GetGravity();

    /**
     * Gets the moment of inertia about each axis.
     */
    virtual CVector3_f_ *GetMoment();

    /**
     * Gets the angle of the object about each axis.
     */
    virtual CVector3_f_ *GetRotation();

    /**
     * Gets the angle that the rotation turns each step.
     */
    virtual CVector3_f_ *GetRotVelocity();

    /**
     * Gets the angle that the rotational velocity gains each step.
     */
    virtual CVector3_f_ *GetRotAcceleration();

    /**
     * Sets the scale of the object along each axis.
     */
    virtual void SetScale(float x, float y, float z);

    /**
     * Sets the scale of the object along each axis.
     */
    virtual void SetScale(float *scale);

    /**
     * Gets the scale of the object along each axis.
     */
    virtual void GetScale(float *out_scale);

    /**
     * Makes an object of a mass, and puts it at rest at the world origin.
     */
    CObject(float mass = 1.0f);

    /**
     * Sets the mass of the object, puts it at the world origin at rest, and
     * gives it a moment and a scale of one.
     */
    void Initialize(float mass);
};

STATIC_ASSERT(sizeof(CObject) == 0xB0);

/**
 * Shared frame attribute block, constructed at start-up.
 */
extern CFrameAttr FrameAttr;

} // namespace dc::game
