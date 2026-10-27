#include "objectframe.hpp"

namespace dc::game {

void CObjectFrame::SetMoment(CVector3_f_ moment) {
    this->moment = moment;
    this->rotation_changed = true;
}

void CObjectFrame::SetRotation(float x, float y, float z) {
    this->rotation.x = x;
    this->rotation.y = y;
    this->rotation.z = z;
    this->rotation_changed = true;
}

void CObjectFrame::SetRotation(CVector3_f_ rotation) {
    this->rotation = rotation;
    this->rotation_changed = true;
}

void CObjectFrame::SetRotVelocity(CVector3_f_ rot_velocity) {
    this->rot_velocity = rot_velocity;
    this->rotation_changed = true;
}

void CObjectFrame::SetRotAcceleration(CVector3_f_ rot_acceleration) {
    this->rot_acceleration = rot_acceleration;
    this->rotation_changed = true;
}

} // namespace dc::game
