#include "camerafollow.hpp"

#include <cmath>
#include <numbers>

#include "camera_port.hpp"

// Retail's AddAngle, which sets only where the camera is turning to, plus the mouse's share of the
// delta (camera_port.hpp) applied to where it is as well: the angle and the eye turn about the point
// the eye looks at at once. The angle stays within half a turn, so Step's interpolation still sees
// only the stick's share as the way left to go.

PC_OVERRIDE void CCameraFollow::AddAngle(float delta) {
    float mouse = MouseLookTakeTurn(this, delta);

    if (mouse == 0.0f || !this->follow_on || CCamera::StopCamera) {
        this->next_angle += delta;
        return;
    }

    constexpr float kTurn = 2.0f * std::numbers::pi_v<float>;
    float           turn = std::remainder(mouse, kTurn);
    float           c = std::cos(turn);
    float           s = std::sin(turn);
    float           x = this->pos[0] - this->ref[0];
    float           z = this->pos[2] - this->ref[2];

    this->next_angle += delta - mouse + turn;
    this->angle = std::remainder(this->angle + turn, kTurn);
    this->pos[0] = this->ref[0] + x * c + z * s;
    this->pos[2] = this->ref[2] + z * c - x * s;
}
