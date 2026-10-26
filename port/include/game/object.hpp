#pragma once

#include "common.h"

#include <libvu0.h>

#include "vector3.hpp"

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
     *
     * @mangled __as__7CObjectFRC7CObject
     * @address 0x1431E0
     * @size 0x74
     */
    CObject &operator=(const CObject &source);

    /**
     * Steps the object. The base object does nothing.
     *
     * @mangled Step__7CObjectFv
     * @address 0x156BA0
     * @size 0x8
     */
    virtual void Step();

    /**
     * Gets the distance between this object and another.
     *
     * @mangled GetDistance__7CObjectFR7CObject
     * @address 0x156BB0
     * @size 0x54
     */
    float GetDistance(CObject &other);

    /**
     * Gets the direction from this object to another.
     *
     * @mangled GetDir__7CObjectFR7CObjectPf
     * @address 0x156C10
     * @size 0x64
     */
    void GetDir(CObject &other, float *out_direction);

    /**
     * Sets the mass of the object.
     *
     * @mangled SetMass__7CObjectFf
     * @address 0x156C80
     * @size 0xC
     */
    virtual void SetMass(float mass);

    /**
     * Puts the object at a world position.
     *
     * @mangled SetPosition__7CObjectF11CVector3_f_
     * @address 0x156C90
     * @size 0x34
     */
    virtual void SetPosition(CVector3_f_ position);

    /**
     * Puts the object at a world position.
     *
     * @mangled SetPosition__7CObjectFPf
     * @address 0x157B00
     * @size 0x24
     * @unknownret
     */
    virtual void SetPosition(float *position);

    /**
     * Puts the object at a world position.
     *
     * @mangled SetPosition__7CObjectFfff
     * @address 0x156CD0
     * @size 0x14
     */
    virtual void SetPosition(float x, float y, float z);

    /**
     * Sets the distance that the position moves each step.
     *
     * @mangled SetVelocity__7CObjectF11CVector3_f_
     * @address 0x156CF0
     * @size 0x30
     */
    virtual void SetVelocity(CVector3_f_ velocity);

    /**
     * Sets the distance that the velocity gains each step.
     *
     * @mangled SetAcceleration__7CObjectF11CVector3_f_
     * @address 0x156D20
     * @size 0x30
     */
    virtual void SetAcceleration(CVector3_f_ acceleration);

    /**
     * Sets the part of the velocity that gravity gives.
     *
     * @mangled SetGravity__7CObjectF11CVector3_f_
     * @address 0x156D50
     * @size 0x30
     */
    virtual void SetGravity(CVector3_f_ gravity);

    /**
     * Sets the moment of inertia about each axis.
     *
     * @mangled SetMoment__7CObjectF11CVector3_f_
     * @address 0x156D80
     * @size 0x30
     */
    virtual void SetMoment(CVector3_f_ moment);

    /**
     * Turns the object to an angle about each axis.
     *
     * @mangled SetRotation__7CObjectF11CVector3_f_
     * @address 0x156DB0
     * @size 0x30
     */
    virtual void SetRotation(CVector3_f_ rotation);

    /**
     * Turns the object to an angle about each axis.
     *
     * @mangled SetRotation__7CObjectFfff
     * @address 0x156DE0
     * @size 0x14
     */
    virtual void SetRotation(float x, float y, float z);

    /**
     * Turns the object to an angle about each axis.
     *
     * @mangled SetRotation__7CObjectFPf
     * @address 0x156E00
     * @size 0x20
     */
    virtual void SetRotation(float *rotation);

    /**
     * Sets the angle that the rotation turns each step.
     *
     * @mangled SetRotVelocity__7CObjectF11CVector3_f_
     * @address 0x156E20
     * @size 0x30
     */
    virtual void SetRotVelocity(CVector3_f_ rot_velocity);

    /**
     * Sets the angle that the rotational velocity gains each step.
     *
     * @mangled SetRotAcceleration__7CObjectF11CVector3_f_
     * @address 0x156E50
     * @size 0x30
     */
    virtual void SetRotAcceleration(CVector3_f_ rot_acceleration);

    /**
     * Gets the mass of the object.
     *
     * @mangled GetMass__7CObjectFv
     * @address 0x156E80
     * @size 0xC
     */
    virtual float GetMass();

    /**
     * Gets the world position of the object.
     *
     * @mangled GetPosition__7CObjectFP11CVector3_f_
     * @address 0x156E90
     * @size 0x20
     */
    virtual void GetPosition(CVector3_f_ *out_position);

    /**
     * Gets the distance that the position moves each step.
     *
     * @mangled GetVelocity__7CObjectFP11CVector3_f_
     * @address 0x156EB0
     * @size 0x10
     */
    virtual void GetVelocity(CVector3_f_ *out_velocity);

    /**
     * Gets the distance that the velocity gains each step.
     *
     * @mangled GetAcceleration__7CObjectFP11CVector3_f_
     * @address 0x156EC0
     * @size 0x10
     */
    virtual void GetAcceleration(CVector3_f_ *out_acceleration);

    /**
     * Gets the part of the velocity that gravity gives.
     *
     * @mangled GetGravity__7CObjectFP11CVector3_f_
     * @address 0x156ED0
     * @size 0x10
     */
    virtual void GetGravity(CVector3_f_ *out_gravity);

    /**
     * Gets the moment of inertia about each axis.
     *
     * @mangled GetMoment__7CObjectFP11CVector3_f_
     * @address 0x156EE0
     * @size 0x10
     */
    virtual void GetMoment(CVector3_f_ *out_moment);

    /**
     * Gets the angle of the object about each axis.
     *
     * @mangled GetRotation__7CObjectFPf
     * @address 0x156EF0
     * @size 0x20
     */
    virtual void GetRotation(float *out_rotation);

    /**
     * Gets the angle of the object about each axis.
     *
     * @mangled GetRotation__7CObjectFP11CVector3_f_
     * @address 0x156F10
     * @size 0x10
     */
    virtual void GetRotation(CVector3_f_ *out_rotation);

    /**
     * Gets the angle that the rotation turns each step.
     *
     * @mangled GetRotVelocity__7CObjectFP11CVector3_f_
     * @address 0x156F20
     * @size 0x10
     */
    virtual void GetRotVelocity(CVector3_f_ *out_rot_velocity);

    /**
     * Gets the angle that the rotational velocity gains each step.
     *
     * @mangled GetRotAcceleration__7CObjectFP11CVector3_f_
     * @address 0x156F30
     * @size 0x10
     */
    virtual void GetRotAcceleration(CVector3_f_ *out_rot_acceleration);

    /**
     * Gets the distance that the position moves each step.
     *
     * @mangled GetVelocity__7CObjectFv
     * @address 0x156F40
     * @size 0xC
     */
    virtual CVector3_f_ *GetVelocity();

    /**
     * Gets the distance that the velocity gains each step.
     *
     * @mangled GetAcceleration__7CObjectFv
     * @address 0x156F50
     * @size 0xC
     */
    virtual CVector3_f_ *GetAcceleration();

    /**
     * Gets the part of the velocity that gravity gives.
     *
     * @mangled GetGravity__7CObjectFv
     * @address 0x156F60
     * @size 0xC
     */
    virtual CVector3_f_ *GetGravity();

    /**
     * Gets the moment of inertia about each axis.
     *
     * @mangled GetMoment__7CObjectFv
     * @address 0x156F70
     * @size 0xC
     */
    virtual CVector3_f_ *GetMoment();

    /**
     * Gets the angle of the object about each axis.
     *
     * @mangled GetRotation__7CObjectFv
     * @address 0x156F80
     * @size 0xC
     */
    virtual CVector3_f_ *GetRotation();

    /**
     * Gets the angle that the rotation turns each step.
     *
     * @mangled GetRotVelocity__7CObjectFv
     * @address 0x156F90
     * @size 0xC
     */
    virtual CVector3_f_ *GetRotVelocity();

    /**
     * Gets the angle that the rotational velocity gains each step.
     *
     * @mangled GetRotAcceleration__7CObjectFv
     * @address 0x156FA0
     * @size 0xC
     */
    virtual CVector3_f_ *GetRotAcceleration();

    /**
     * Sets the scale of the object along each axis.
     *
     * @mangled SetScale__7CObjectFfff
     * @address 0x156FB0
     * @size 0x14
     */
    virtual void SetScale(float x, float y, float z);

    /**
     * Sets the scale of the object along each axis.
     *
     * @mangled SetScale__7CObjectFPf
     * @address 0x156FD0
     * @size 0x20
     */
    virtual void SetScale(float *scale);

    /**
     * Gets the scale of the object along each axis.
     *
     * @mangled GetScale__7CObjectFPf
     * @address 0x156FF0
     * @size 0x20
     */
    virtual void GetScale(float *out_scale);

    /**
     * Makes an object of a mass, and puts it at rest at the world origin.
     *
     * @mangled __ct__7CObjectFf
     * @address 0x157010
     * @size 0x3C
     */
    CObject(float mass = 1.0f);

    /**
     * Sets the mass of the object, puts it at the world origin at rest, and
     * gives it a moment and a scale of one.
     *
     * @mangled Initialize__7CObjectFf
     * @address 0x157050
     * @size 0x68
     */
    void Initialize(float mass);
};

STATIC_ASSERT(sizeof(CObject) == 0xB0);

/**
 * Shared frame attribute block, constructed at start-up.
 */
extern CFrameAttr FrameAttr;

/**
 * Constructor of CObject under the name main's array constructors call it by.
 *
 * @mangled __ct__7CObjectFf
 * @address 0x157010
 * @size 0x3c
 */
extern "C" void *GeneratedObjectCtor(void *self, float mass);
