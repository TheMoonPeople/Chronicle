#include "camera.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cmath>

#include "mathutil.hpp"

namespace dc::game {

s32 CCamera::StopCamera;

void CCamera::Step(int steps) {
    float dir[4];
    float flat[4];

    if (CCamera::StopCamera) {
        return;
    }

    if (this->speed <= 0.0f) {
        this->speed = 1.0f;
    }

    // A step count below zero puts the eye and the look-at point onto their
    // positions without any of the steps in between.
    if (steps < 0) {
        for (int axis = 0; axis < 3; axis++) {
            this->pos[axis] = this->next_pos[axis];
            this->ref[axis] = this->next_ref[axis];
        }
    } else {
        for (int step = 0; step < steps; step++) {
            for (int axis = 0; axis < 3; axis++) {
                float speed = this->speed;

                if (speed == 1.0f) {
                    this->pos[axis] = this->next_pos[axis];
                    this->ref[axis] = this->next_ref[axis];
                } else {
                    float move_pos = (this->next_pos[axis] - this->pos[axis]) / speed;
                    speed = std::max(speed, 1.0f);
                    const float move_ref = (this->next_ref[axis] - this->ref[axis]) / speed;

                    if (this->limit_move == 1) {
                        move_pos = std::min(move_pos, 2.0f);
                        move_pos = std::max(move_pos, -2.0f);
                    }

                    this->pos[axis] += move_pos;
                    this->ref[axis] += move_ref;

                    // The last step of the movement is not exact, so the eye
                    // and the look-at point snap on once they are near enough.
                    const float left_pos = this->pos[axis] - this->next_pos[axis];
                    const float left_ref = this->ref[axis] - this->next_ref[axis];

                    if (left_pos < this->snap_range && left_pos > -this->snap_range) {
                        this->pos[axis] = this->next_pos[axis];
                    }

                    if (left_ref < this->snap_range && left_ref > -this->snap_range) {
                        this->ref[axis] = this->next_ref[axis];
                    }
                }
            }
        }
    }

    // The angles of the view direction; the horizontal one comes from the
    // direction with its height taken out.
    this->GetDir(dir);
    flat[0] = dir[0];
    flat[1] = 0.0f;
    flat[2] = dir[2];
    flat[3] = 0.0f;
    sceVu0Normalize(flat, flat);
    this->angle_h = atan2f(-flat[0], -flat[2]);
    this->angle_v = -atan2f(dir[1], sqrtf(dir[0] * dir[0] + dir[2] * dir[2]));
}

void CCamera::Stay() {
    sceVu0CopyVector(this->next_pos, this->pos);
    sceVu0CopyVector(this->next_ref, this->ref);
}

void CCamera::SetPos(CFrame * /*parent*/, float x, float y, float z) {
    this->next_pos[0] = x;
    this->pos[0] = x;
    this->next_pos[1] = y;
    this->pos[1] = y;
    this->next_pos[2] = z;
    this->pos[2] = z;
    this->next_pos[3] = 1.0f;
    this->pos[3] = 1.0f;
}

void CCamera::SetPos(float *pos) {
    this->SetPos(nullptr, pos[0], pos[1], pos[2]);
}

void CCamera::SetNextPos(CFrame * /*parent*/, float x, float y, float z) {
    this->next_pos[0] = x;
    this->next_pos[1] = y;
    this->next_pos[2] = z;
}

void CCamera::SetRef(CFrame * /*parent*/, float x, float y, float z) {
    this->ref[0] = x;
    this->next_ref[0] = x;
    this->ref[1] = y;
    this->next_ref[1] = y;
    this->ref[2] = z;
    this->next_ref[2] = z;
}

void CCamera::SetRef(float *ref) {
    this->SetRef(nullptr, ref[0], ref[1], ref[2]);
}

void CCamera::SetRef(float x, float y, float z) {
    this->SetRef(nullptr, x, y, z);
}

void CCamera::SetNextRef(CFrame * /*parent*/, float x, float y, float z) {
    this->next_ref[0] = x;
    this->next_ref[1] = y;
    this->next_ref[2] = z;
}

void CCamera::GetDir(float *dir) {
    dir[0] = this->ref[0] - this->pos[0];
    dir[1] = this->ref[1] - this->pos[1];
    dir[2] = this->ref[2] - this->pos[2];
}

void CCamera::GetCameraMatrix(float (*matrix)[4]) {
    float dir[4];
    float roll_matrix[4][4];

    // The view looks down the negative Z axis, so the direction turns around.
    this->GetDir(dir);
    dir[1] = -dir[1];
    dir[2] = -dir[2];

    float length = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    float flat_length = sqrtf(dir[0] * dir[0] + dir[2] * dir[2]);

    if (flat_length == 0.0f) {
        flat_length = 1.0f;
    }

    if (length == 0.0f) {
        length = 1.0f;
    }

    const float sin_h = dir[2] / flat_length;
    const float cos_h = dir[0] / flat_length;
    const float cos_v = flat_length / length;
    const float sin_v = dir[1] / length;

    matrix[0][0] = sin_h;
    matrix[0][1] = -cos_h * sin_v;
    matrix[0][2] = cos_h * cos_v;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = -cos_v;
    matrix[1][2] = -sin_v;
    matrix[1][3] = 0.0f;
    matrix[2][0] = cos_h;
    matrix[2][1] = sin_h * sin_v;
    matrix[2][2] = -sin_h * cos_v;
    matrix[2][3] = 0.0f;
    matrix[3][0] = -this->pos[0] * sin_h - this->pos[2] * cos_h;
    matrix[3][1] = sin_v * (this->pos[0] * cos_h) + this->pos[1] * cos_v - sin_v * (this->pos[2] * sin_h);
    matrix[3][2] = cos_v * (-this->pos[0] * cos_h) + this->pos[1] * sin_v + cos_v * (this->pos[2] * sin_h);
    matrix[3][3] = 1.0f;

    // The roll turns the view about the direction that it looks along.
    if (this->roll > PI) {
        this->roll -= TWO_PI;
    }

    if (this->roll < -PI) {
        this->roll += TWO_PI;
    }

    sceVu0UnitMatrix(roll_matrix);
    const float sin_roll = sinf(this->roll);
    const float cos_roll = cosf(this->roll);
    roll_matrix[0][0] = cos_roll;
    roll_matrix[1][0] = sin_roll;
    roll_matrix[0][1] = -sin_roll;
    roll_matrix[1][1] = cos_roll;
    MulMatrix(matrix, roll_matrix, matrix);
}

void CCamera::SetSpeed(float speed) {
    this->speed = speed;
}

void CCamera::SetRoll(float roll) {
    this->roll = roll;
}

void CCamera::GetPos(float *pos) {
    sceVu0CopyVector(pos, this->pos);
}

void CCamera::GetRef(float *ref) {
    sceVu0CopyVector(ref, this->ref);
}

float CCamera::GetAngleH() {
    return this->angle_h;
}

float CCamera::GetRoll() {
    return this->roll;
}

CCamera::CCamera(float speed) : speed(speed) {
    if (this->speed <= 0.0f) {
        this->speed = 1.0f;
    }

    this->limit_move = 0;
    this->roll = 0.0f;
    this->snap_range = 0.01f;
}

CCamera::~CCamera() = default;

} // namespace dc::game
