#pragma helper_mask_fpr 0x1000

#include "camerafollow.hpp"

#include <libvu0.h>

#include <cmath>

#include "mathutil.hpp"

void CCameraFollow::Step(int steps) {
    if (CCamera::StopCamera) {
        return;
    }

    // A step count below zero turns the eye onto its angle without any of the
    // steps in between.
    if (steps < 0) {
        if (this->follow_on) {
            this->angle = this->next_angle;
            this->SetNextPos(NULL, this->follow[0] + this->distance * sinf(this->angle), this->follow[1] + this->height, this->follow[2] + this->distance * cosf(this->angle));
            this->SetNextRef(NULL, this->follow[0], this->follow[1], this->follow[2]);
        }

        this->CCamera::Step(steps);
    } else {
        int i;

        // The angle that the eye turns to stays inside one turn.
        if (this->next_angle > TWO_PI_D) {
            this->next_angle -= TWO_PI;
        }

        if (this->next_angle < 0.0f) {
            this->next_angle += TWO_PI;
        }

        for (i = 0; i < steps; i++) {
            if (this->follow_on) {
                float turn = this->speed / 2.0f;

                if (turn < 1.0f) {
                    turn = 1.0f;
                }

                this->angle = AngleInterpolate(this->angle, this->next_angle, turn, INTERPOLATE_FRACTION);

                // An eye that reaches its position in about one step turns at
                // once as well.
                if (this->speed < 1.1f) {
                    this->angle = this->next_angle;
                }

                this->SetNextPos(NULL, this->follow[0] + this->distance * sinf(this->angle), this->follow[1] + this->height, this->follow[2] + this->distance * cosf(this->angle));
                this->SetNextRef(NULL, this->follow[0], this->follow[1], this->follow[2]);
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

CCameraFollow::CCameraFollow(float distance, float height, float angle, float speed) : CCamera(speed) {
    this->follow[0] = 0.0f;
    this->follow[1] = 0.0f;
    this->follow[2] = 0.0f;
    this->next_angle = angle;
    this->angle = angle;
    this->distance = distance;
    this->height = height;
    this->follow_on = 1;
}

CCameraFollow::~CCameraFollow() {
}
