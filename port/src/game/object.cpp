#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "object.hpp"

#include <libvu0.h>

#include "frame.hpp"
#include "mathutil.hpp"

CFrameAttr FrameAttr;

void CObject::Step() {
}

float CObject::GetDistance(CObject &other) {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR other_pos;

    sceVu0CopyVector(other_pos, other.pos);
    sceVu0CopyVector(pos, this->pos);
    return DistVector(pos, other_pos);
}

void CObject::GetDir(CObject &other, float *out_direction) {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR other_pos;

    sceVu0CopyVector(other_pos, other.pos);
    sceVu0CopyVector(pos, this->pos);
    sceVu0SubVector(out_direction, other_pos, pos);
}

void CObject::SetMass(float mass) {
    this->mass = mass;
}

void CObject::SetPosition(CVector3_f_ position) {
    this->pos[0] = position.x;
    this->pos[1] = position.y;
    this->pos[2] = position.z;
}

void CObject::SetPosition(float x, float y, float z) {
    this->pos[0] = x;
    this->pos[1] = y;
    this->pos[2] = z;
}

void CObject::SetVelocity(CVector3_f_ velocity) {
    this->velocity = velocity;
}

void CObject::SetAcceleration(CVector3_f_ acceleration) {
    this->acceleration = acceleration;
}

void CObject::SetGravity(CVector3_f_ gravity) {
    this->gravity = gravity;
}

void CObject::SetMoment(CVector3_f_ moment) {
    this->moment = moment;
}

void CObject::SetRotation(CVector3_f_ rotation) {
    this->rotation = rotation;
}

void CObject::SetRotation(float x, float y, float z) {
    this->rotation.x = x;
    this->rotation.y = y;
    this->rotation.z = z;
}

void CObject::SetRotation(float *rotation) {
    this->rotation.x = rotation[0];
    this->rotation.y = rotation[1];
    this->rotation.z = rotation[2];
}

void CObject::SetRotVelocity(CVector3_f_ rot_velocity) {
    this->rot_velocity = rot_velocity;
}

void CObject::SetRotAcceleration(CVector3_f_ rot_acceleration) {
    this->rot_acceleration = rot_acceleration;
}

float CObject::GetMass() {
    return this->mass;
}

void CObject::GetPosition(CVector3_f_ *out_position) {
    out_position->x = this->pos[0];
    out_position->y = this->pos[1];
    out_position->z = this->pos[2];
}

void CObject::GetVelocity(CVector3_f_ *out_velocity) {
    *out_velocity = this->velocity;
}

void CObject::GetAcceleration(CVector3_f_ *out_acceleration) {
    *out_acceleration = this->acceleration;
}

void CObject::GetGravity(CVector3_f_ *out_gravity) {
    *out_gravity = this->gravity;
}

void CObject::GetMoment(CVector3_f_ *out_moment) {
    *out_moment = this->moment;
}

void CObject::GetRotation(float *out_rotation) {
    out_rotation[0] = this->rotation.x;
    out_rotation[1] = this->rotation.y;
    out_rotation[2] = this->rotation.z;
}

void CObject::GetRotation(CVector3_f_ *out_rotation) {
    *out_rotation = this->rotation;
}

void CObject::GetRotVelocity(CVector3_f_ *out_rot_velocity) {
    *out_rot_velocity = this->rot_velocity;
}

void CObject::GetRotAcceleration(CVector3_f_ *out_rot_acceleration) {
    *out_rot_acceleration = this->rot_acceleration;
}

CVector3_f_ *CObject::GetVelocity() {
    return &this->velocity;
}

CVector3_f_ *CObject::GetAcceleration() {
    return &this->acceleration;
}

CVector3_f_ *CObject::GetGravity() {
    return &this->gravity;
}

CVector3_f_ *CObject::GetMoment() {
    return &this->moment;
}

CVector3_f_ *CObject::GetRotation() {
    return &this->rotation;
}

CVector3_f_ *CObject::GetRotVelocity() {
    return &this->rot_velocity;
}

CVector3_f_ *CObject::GetRotAcceleration() {
    return &this->rot_acceleration;
}

void CObject::SetScale(float x, float y, float z) {
    this->scale[0] = x;
    this->scale[1] = y;
    this->scale[2] = z;
}

void CObject::SetScale(float *scale) {
    this->scale[0] = scale[0];
    this->scale[1] = scale[1];
    this->scale[2] = scale[2];
}

void CObject::GetScale(float *out_scale) {
    out_scale[0] = this->scale[0];
    out_scale[1] = this->scale[1];
    out_scale[2] = this->scale[2];
}

CObject::CObject(float mass) {
    Initialize(mass);
}

void CObject::Initialize(float mass) {
    CVector3_f_ zero;

    this->mass = mass;
    this->moment.x = this->moment.y = this->moment.z = 1.0f;
    this->scale[0] = this->scale[1] = this->scale[2] = 1.0f;
    this->pos[0] = this->pos[1] = this->pos[2] = 0.0f;

    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    this->rotation = this->rot_velocity = this->rot_acceleration = this->velocity = this->acceleration = this->gravity = zero;
}
