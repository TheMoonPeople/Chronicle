#include "camerafollow.hpp"

#include <cmath>
#include <numbers>

#include "input/mouse_look.hpp"

void CCameraFollow::AddHeight(float delta) {
    this->height += MouseLookHeightDelta(this, delta);
}

// Sets where the camera is turning to and also applies the mouse's share of the delta
// (camera_port.hpp) to where it is, turning the angle and the eye about the look-at point at once.
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
