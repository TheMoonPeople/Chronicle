#include "camera_port.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "camerafollow.hpp"
#include "collision.hpp"
#include "editground.hpp"
#include "gameutil.hpp"
#include "mouse_collision.hpp"
#include "platform/input.hpp"

namespace {

// About 86 degrees: the tangent stays finite.
constexpr float kMaxTilt = 1.5f;

// What a reading left with its camera: the AddAngle delta it makes, the stick's share of that
// delta, and the pad read it belongs to.
struct Share {
    CCameraFollow *camera = nullptr;
    float          delta = 0.0f;
    float          stick_delta = 0.0f;
    std::uint64_t  read = 0;
};

Share g_turn;

struct Pitch {
    CCamera      *camera = nullptr;
    std::uint64_t read = 0;
    std::uint64_t applied = 0;
    float         offset = 0.0f;
    bool          enabled = false;
};

Pitch g_pitch;

struct TownRequest {
    bool           open = false;
    CCameraFollow *camera = nullptr;
    CEditGround   *ground = nullptr;
    int            map = 0;
    int            mode = 0;
    std::uint64_t  read = 0;
    float          yaw = 0.0f;
    bool           fishing = false;
    bool           recorded = false;
};

TownRequest g_town;

// Reused only by the town request; collection proves bounds before writing.
std::vector<CCPoly> g_town_polys;

CBoxVu0 CameraBox(CCameraFollow &camera) {
    CBoxVu0 box{};
    float   radius = std::max({std::hypot(camera.pos[0] - camera.ref[0], camera.pos[2] - camera.ref[2]),
                               std::hypot(camera.next_pos[0] - camera.next_ref[0], camera.next_pos[2] - camera.next_ref[2]),
                               std::fabs(camera.distance)}) +
                     20.0f;
    for (int i = 0; i < 3; ++i) {
        float low = std::min({camera.ref[i], camera.next_ref[i], camera.follow[i], camera.pos[i], camera.next_pos[i]});
        float high = std::max({camera.ref[i], camera.next_ref[i], camera.follow[i], camera.pos[i], camera.next_pos[i]});
        box.min[i] = low - radius - std::fabs(camera.height);
        box.max[i] = high + radius + std::fabs(camera.height);
    }
    return box;
}

void Orbit(float *point, const float *centre, float turn) {
    float c = std::cos(turn);
    float s = std::sin(turn);
    float x = point[0] - centre[0];
    float z = point[2] - centre[2];
    point[0] = centre[0] + x * c + z * s;
    point[2] = centre[2] + z * c - x * s;
}

} // namespace

float MouseLookTurn(CCameraFollow *camera, float radians, float stick, bool pitch) {
    MouseLookControlPitch(camera, pitch && camera->follow_on);
    const InputMouseLook &look = InputGetMouseLook();
    g_turn = {};
    if (look.yaw == 0.0f) {
        return stick;
    }
    float reading = stick + look.yaw / radians;
    // Mouse and stick that cancel turn nothing, as in the town, which then makes no AddAngle.
    if (reading != 0.0f) {
        g_turn = {camera, radians * -reading, radians * -stick, look.read};
    }
    return reading;
}

float MouseLookTakeTurn(CCameraFollow *camera, float delta) {
    if (g_turn.camera != camera) {
        return 0.0f;
    }
    // A reading lasts until its camera's next turn within the same pad read: a turn the camera
    // refused is not carried into a later one.
    Share share = g_turn;
    g_turn = {};
    if (share.read != InputGetMouseLook().read || delta != share.delta) {
        return 0.0f;
    }
    return delta - share.stick_delta;
}

void MouseLookControlPitch(CCamera *camera, bool enabled) {
    if (g_pitch.camera != camera) {
        g_pitch = {};
    }
    g_pitch.camera = camera;
    g_pitch.read = InputGetMouseLook().read;
    g_pitch.enabled = enabled;
    if (!enabled) {
        g_pitch.offset = 0.0f;
    }
}

float MouseLookViewPitch(CCamera *camera, float base) {
    const InputMouseLook &look = InputGetMouseLook();
    if (g_pitch.camera != camera || !g_pitch.enabled) {
        return base;
    }
    // Paused menus retain the view, but only an active owner can consume new motion.
    // Scripted follow-off cameras restore their authored framing.
    auto *follow = dynamic_cast<CCameraFollow *>(camera);
    if (!follow || !follow->follow_on) {
        g_pitch = {};
        return base;
    }
    if (g_pitch.read == look.read && !CCamera::StopCamera && g_pitch.applied != look.read) {
        g_pitch.offset -= look.pitch;
        g_pitch.applied = look.read;
    }
    if (g_pitch.offset == 0.0f) {
        return base;
    }
    float angle = std::clamp(base + g_pitch.offset, -kMaxTilt, kMaxTilt);
    // Excess motion at the limit is discarded, so reversing the mouse responds immediately.
    g_pitch.offset = angle - base;
    return angle;
}

float MouseLookEyeAngleV(float angle) {
    float pitch = InputGetMouseLook().pitch;
    float tilted = angle - pitch;
    if (pitch > 0.0f) {
        return std::max(tilted, std::min(angle, -kMaxTilt));
    }
    if (pitch < 0.0f) {
        return std::min(tilted, std::max(angle, kMaxTilt));
    }
    return angle;
}

float MouseLookEyeAngleH(float angle) {
    constexpr float kPi = std::numbers::pi_v<float>;
    float           yaw = InputGetMouseLook().yaw;
    if (yaw == 0.0f) {
        return angle;
    }
    return std::remainder(angle - yaw, 2.0f * kPi);
}

float MouseLookClampOrbit(const float *eye, const float *look, float turn, const MouseLookPolys &polys) {
    float         distance = std::hypot(eye[0] - look[0], eye[2] - look[2]);
    float         angle = std::atan2(eye[0] - look[0], eye[2] - look[2]);
    CCameraFollow camera(distance, eye[1] - look[1], angle, 8.0f);
    sceVu0CopyVector(camera.pos, const_cast<float *>(eye));
    sceVu0CopyVector(camera.next_pos, const_cast<float *>(eye));
    sceVu0CopyVector(camera.ref, const_cast<float *>(look));
    sceVu0CopyVector(camera.next_ref, const_cast<float *>(look));
    sceVu0CopyVector(camera.follow, const_cast<float *>(look));
    CBoxVu0 box = CameraBox(camera);
    CCPoly *walls = nullptr;
    int     count = polys(box, &walls);
    return MouseCameraClamp(camera, turn, walls, count);
}

void MouseLookTurnOrbit(CCameraFollow *camera, float turn) {
    constexpr float kTurn = 2.0f * std::numbers::pi_v<float>;
    Orbit(camera->pos, camera->ref, turn);
    Orbit(camera->next_pos, camera->next_ref, turn);
    camera->angle = std::remainder(camera->angle + turn, kTurn);
    camera->next_angle = std::remainder(camera->next_angle + turn, kTurn);
    // The view's own angles, which a step works out from the eye, without stepping it.
    camera->CCamera::Step(0);
}

void TownMouseBegin(CCameraFollow *camera, CEditGround *ground, int map, int mode) {
    g_town = {true, camera, ground, map, mode, InputGetMouseLook().read};
}

void TownMouseRecord(CCameraFollow *camera, bool fishing) {
    if (g_town.open && g_town.camera == camera && g_town.read == InputGetMouseLook().read && !g_town.recorded) {
        g_town.yaw = InputGetMouseLook().yaw;
        g_town.fishing = fishing;
        g_town.recorded = true;
    }
}

void TownMouseApply(CCamera *shown, CEditGround *ground, int map, int mode) {
    TownRequest request = g_town;
    g_town = {};
    CCameraFollow *camera = request.camera;
    if (!request.open || !request.recorded || static_cast<CCamera *>(camera) != shown || request.ground != ground ||
        request.map != map || request.mode != mode || request.read != InputGetMouseLook().read ||
        !camera->follow_on || CCamera::StopCamera) {
        return;
    }
    if (!ground) {
        return;
    }
    MouseLookControlPitch(camera, true);
    if (request.yaw == 0.0f) {
        return;
    }
    int     mask = request.fishing ? 1 : 0xFFFF;
    CBoxVu0 box = CameraBox(*camera);
    int     count = MouseCameraPolys(*ground, box, mask, g_town_polys);
    float   turn = MouseCameraClamp(*camera, -request.yaw, g_town_polys.data(), count);
    if (turn != 0.0f) {
        MouseLookTurnOrbit(camera, turn);
    }
}
