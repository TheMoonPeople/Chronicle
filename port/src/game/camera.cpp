#pragma helper_mask_fpr 0x1000

#include "camera.hpp"

#include <libvu0.h>

#include <cmath>

#include "mathutil.hpp"

s32 CCamera::StopCamera;

void CCamera::Step(int steps) {
    float dir[4];
    float flat[4];
    int   i;
    int   step;
    int   axis;

    if (CCamera::StopCamera) {
        return;
    }

    if (this->speed <= 0.0f) {
        this->speed = 1.0f;
    }

    // A step count below zero puts the eye and the look-at point onto their
    // positions without any of the steps in between.
    if (steps < 0) {
        for (i = 0; i < 3; i++) {
            this->pos[i] = this->next_pos[i];
            this->ref[i] = this->next_ref[i];
        }
    } else {
        for (step = 0; step < steps; step++) {
            for (axis = 0; axis < 3; axis++) {
                float move_pos;
                float move_ref;
                float left_pos;
                float left_ref;
                float speed = this->speed;

                if (speed == 1.0f) {
                    this->pos[axis] = this->next_pos[axis];
                    this->ref[axis] = this->next_ref[axis];
                } else {
                    move_pos = (this->next_pos[axis] - this->pos[axis]) / speed;

                    if (speed < 1.0f) {
                        speed = 1.0f;
                    }

                    move_ref = (this->next_ref[axis] - this->ref[axis]) / speed;

                    if (this->limit_move == 1) {
                        if (move_pos > 2.0f) {
                            move_pos = 2.0f;
                        }

                        if (move_pos < -2.0f) {
                            move_pos = -2.0f;
                        }
                    }

                    this->pos[axis] += move_pos;
                    this->ref[axis] += move_ref;

                    // The last step of the movement is not exact, so the eye
                    // and the look-at point snap on once they are near enough.
                    left_pos = this->pos[axis] - this->next_pos[axis];
                    left_ref = this->ref[axis] - this->next_ref[axis];

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

void CCamera::SetPos(CFrame *parent, float x, float y, float z) {
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
    this->SetPos(NULL, pos[0], pos[1], pos[2]);
}

void CCamera::SetNextPos(CFrame *parent, float x, float y, float z) {
    this->next_pos[0] = x;
    this->next_pos[1] = y;
    this->next_pos[2] = z;
}

void CCamera::SetRef(CFrame *parent, float x, float y, float z) {
    this->ref[0] = x;
    this->next_ref[0] = x;
    this->ref[1] = y;
    this->next_ref[1] = y;
    this->ref[2] = z;
    this->next_ref[2] = z;
}

void CCamera::SetRef(float *ref) {
    this->SetRef(NULL, ref[0], ref[1], ref[2]);
}

void CCamera::SetRef(float x, float y, float z) {
    this->SetRef(NULL, x, y, z);
}

void CCamera::SetNextRef(CFrame *parent, float x, float y, float z) {
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
    float length;
    float flat_length;
    float sin_h;
    float cos_h;
    float cos_v;
    float sin_v;
    float sin_roll;
    float cos_roll;

    // The view looks down the negative Z axis, so the direction turns around.
    this->GetDir(dir);
    dir[1] = -dir[1];
    dir[2] = -dir[2];

    length = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    flat_length = sqrtf(dir[0] * dir[0] + dir[2] * dir[2]);

    if (flat_length == 0.0f) {
        flat_length = 1.0f;
    }

    if (length == 0.0f) {
        length = 1.0f;
    }

    sin_h = dir[2] / flat_length;
    cos_h = dir[0] / flat_length;
    cos_v = flat_length / length;
    sin_v = dir[1] / length;

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
    sin_roll = sinf(this->roll);
    cos_roll = cosf(this->roll);
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

CCamera::CCamera(float speed) {
    this->speed = speed;

    float zero = 0.0f;

    if (this->speed <= zero) {
        this->speed = 1.0f;
    }

    this->limit_move = 0;
    this->roll = zero;
    this->snap_range = 0.01f;
}

CCamera::~CCamera() {
}
