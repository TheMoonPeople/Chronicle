#include "camera.hpp"

#include <libvu0.h>

#include <cmath>

#include "camera_port.hpp"
#include "mathutil.hpp"

// The mouse changes view pitch, independently of the collision-safe eye position.
PC_OVERRIDE void CCamera::GetCameraMatrix(float (*matrix)[4]) {
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
    float base = std::atan2(dir[1], flat_length);
    float pitch = MouseLookViewPitch(this, base);
    if (pitch != base) {
        cos_v = std::cos(pitch);
        sin_v = std::sin(pitch);
    }

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
