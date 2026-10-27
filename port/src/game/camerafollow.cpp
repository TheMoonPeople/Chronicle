#include "camerafollow.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "input/mouse_look.hpp"
#include "mathutil.hpp"

namespace dc::game {

using namespace dc::input;

void CCameraFollow::Step(int steps) {
    if (CCamera::StopCamera) {
        return;
    }

    // A step count below zero turns the eye onto its angle without any of the
    // steps in between.
    if (steps < 0) {
        if (this->follow_on) {
            this->angle = this->next_angle;
            this->SetNextPos(nullptr, this->follow[0] + this->distance * sinf(this->angle), this->follow[1] + this->height, this->follow[2] + this->distance * cosf(this->angle));
            this->SetNextRef(nullptr, this->follow[0], this->follow[1], this->follow[2]);
        }

        this->CCamera::Step(steps);
    } else {
        // Wrapped once only, so an angle more than a turn out stays out of range.
        if (this->next_angle > TWO_PI_D) {
            this->next_angle -= TWO_PI;
        }

        if (this->next_angle < 0.0f) {
            this->next_angle += TWO_PI;
        }

        for (int step = 0; step < steps; step++) {
            if (this->follow_on) {
                const float turn = std::max(this->speed / 2.0f, 1.0f);

                this->angle = AngleInterpolate(this->angle, this->next_angle, turn, INTERPOLATE_FRACTION);

                // An eye that reaches its position in about one step turns at
                // once as well.
                if (this->speed < 1.1f) {
                    this->angle = this->next_angle;
                }

                this->SetNextPos(nullptr, this->follow[0] + this->distance * sinf(this->angle), this->follow[1] + this->height, this->follow[2] + this->distance * cosf(this->angle));
                this->SetNextRef(nullptr, this->follow[0], this->follow[1], this->follow[2]);
            }

            this->CCamera::Step(1);
        }

        // An eye that no longer circles the position takes the angle that the
        // base camera measured for it.
        if (!this->follow_on) {
            this->next_angle = this->angle_h;
            this->angle = this->angle_h;
        }
    }
}

void CCameraFollow::Stay() {
    this->CCamera::Stay();

    if (this->follow_on) {
        sceVu0CopyVector(this->follow, this->next_ref);
        this->next_angle = this->angle;
    }
}

CCameraFollow &CCameraFollow::operator=(const CCameraFollow &src) {
    this->follow[0] = src.follow[0];
    this->follow[1] = src.follow[1];
    this->follow[2] = src.follow[2];
    this->follow[3] = src.follow[3];
    this->distance = src.distance;
    this->height = src.height;
    return *this;
}

void CCameraFollow::SetFollow(float x, float y, float z) {
    this->follow[0] = x;
    this->follow[1] = y;
    this->follow[2] = z;
}

void CCameraFollow::FollowOn() {
    this->follow_on = 1;
}

void CCameraFollow::FollowOff() {
    this->follow_on = 0;
}

void CCameraFollow::SetAngle(float angle) {
    this->next_angle = angle;
}

void CCameraFollow::SetAngleSoon(float angle) {
    this->next_angle = angle;
    this->angle = angle;
}

float CCameraFollow::GetAngle() {
    return this->angle;
}

// Sets where the camera is turning to and also applies the mouse's share of the delta
// (input/mouse_look.hpp) to where it is, turning the angle and the eye about the look-at point at once.
// The angle stays within half a turn, so Step's interpolation sees only the stick's share as the way
// left to go.
void CCameraFollow::AddAngle(float delta) {
    const float mouse = MouseLookTakeTurn(this, delta);

    if (mouse == 0.0f || !this->follow_on || CCamera::StopCamera) {
        this->next_angle += delta;
        return;
    }

    constexpr float k_turn = 2.0f * std::numbers::pi_v<float>;
    const float     turn = std::remainder(mouse, k_turn);
    const float     cos_turn = std::cos(turn);
    const float     sin_turn = std::sin(turn);
    const float     offset_x = this->pos[0] - this->ref[0];
    const float     offset_z = this->pos[2] - this->ref[2];

    this->next_angle += delta - mouse + turn;
    this->angle = std::remainder(this->angle + turn, k_turn);
    this->pos[0] = this->ref[0] + offset_x * cos_turn + offset_z * sin_turn;
    this->pos[2] = this->ref[2] + offset_z * cos_turn - offset_x * sin_turn;
}

void CCameraFollow::SetDistance(float distance) {
    this->distance = distance;
}

float CCameraFollow::GetDistance() {
    return this->distance;
}

void CCameraFollow::AddDistance(float delta) {
    this->distance += delta;
}

void CCameraFollow::SetHeight(float height) {
    this->height = height;
}

float CCameraFollow::GetHeight() {
    return this->height;
}

void CCameraFollow::AddHeight(float delta) {
    this->height += MouseLookHeightDelta(this, delta);
}

CCameraFollow::CCameraFollow(float distance, float height, float angle, float speed)
    : CCamera(speed), follow{}, distance(distance), height(height), next_angle(angle), angle(angle), follow_on(1) {
}

CCameraFollow::~CCameraFollow() = default;

} // namespace dc::game
