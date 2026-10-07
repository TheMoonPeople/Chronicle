#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <libpad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <vector>

#include "../camera_port.hpp"
#include "../mouse_collision.hpp"
#include "../platform/clock.hpp"
#include "../platform/input.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "collision.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "editarea.hpp"
#include "editground.hpp"
#include "frame.hpp"
#include "gamepad.hpp"

extern int   viewMode;
extern float viewAngleH;
void         EyeCamera(CCamera *camera, CCharacter *character, int right_stick);

namespace {

constexpr float kDegree = std::numbers::pi_v<float> / 180.0f;

// Without capture the window's motion always counts, so no window is needed.
void Settings(float sensitivity, bool invert_y) {
    InputResetBindings();
    InputMouseSettings settings;
    settings.sensitivity = sensitivity;
    settings.invert_y = invert_y;
    settings.capture = false;
    InputSetMouseSettings(settings);
    ClockSetUnbounded(true);
    ClockReset();
}

void Move(float dx, float dy) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = dx;
    event.motion.yrel = dy;
    InputHandleEvent(event);
}

// The angle between two headings, folded into half a turn either way.
float Gap(float a, float b) { return std::remainder(a - b, 2.0f * std::numbers::pi_v<float>); }

} // namespace

TEST(PlatformMouseLook, TakesEachCountOnceAtTheSensitivity) {
    Settings(0.5f, false);
    Move(7.0f, -3.0f);
    Move(13.0f, -1.0f);
    InputLatchPad(0);
    ASSERT_NEAR(InputGetMouseLook().yaw, 10.0f * kDegree, 1e-6f);
    ASSERT_NEAR(InputGetMouseLook().pitch, 2.0f * kDegree, 1e-6f);

    // Far past a stick's full deflection, and nothing is held over to the next read.
    Move(400.0f, 0.0f);
    ClockPump();
    InputLatchPad(0);
    ASSERT_NEAR(InputGetMouseLook().yaw, 200.0f * kDegree, 1e-5f);
    ClockPump();
    InputLatchPad(0);
    ASSERT_TRUE(InputGetMouseLook().yaw == 0.0f && InputGetMouseLook().pitch == 0.0f);

    // Pad 2's reads leave the look alone.
    Move(4.0f, 0.0f);
    InputLatchPad(1);
    ASSERT_TRUE(InputGetMouseLook().yaw == 0.0f);
    InputLatchPad(0);
    ASSERT_NEAR(InputGetMouseLook().yaw, 2.0f * kDegree, 1e-6f);
}

TEST(PlatformMouseLook, InvertAndStickBindings) {
    Settings(1.0f, true);
    Move(0.0f, -5.0f);
    InputLatchPad(0);
    ASSERT_NEAR(InputGetMouseLook().pitch, -5.0f * kDegree, 1e-6f);

    // An axis bound to a stick is the stick's alone.
    std::string_view mouse_x[] = {"MouseX*0.5"};
    ASSERT_TRUE(InputBindKeys("rx", mouse_x));
    Move(10.0f, 10.0f);
    InputLatchPad(0);
    ASSERT_TRUE(InputGetMouseLook().yaw == 0.0f);
    ASSERT_NEAR(InputGetMouseLook().pitch, 10.0f * kDegree, 1e-6f);
}

TEST(PlatformMouseLook, DropsMotionAcrossALoad) {
    Settings(1.0f, false);
    InputLatchPad(0);
    Move(30.0f, 0.0f);
    for (int tick = 0; tick < 60; ++tick) {
        ClockPump();
    }
    InputLatchPad(0);
    ASSERT_TRUE(InputGetMouseLook().yaw == 0.0f);
}

TEST(PlatformMouseLook, FollowCameraTurnsAtOnce) {
    Settings(0.2f, false);
    CCameraFollow camera(60.0f, 5.0f, 0.0f, 8.0f);
    camera.Step(-1);

    // Past half a turn the camera's easing would take the short way round, backwards; 20 degrees
    // would show a fraction of itself. The eye is where the whole turn puts it after one step.
    for (float counts : {1000.0f, 100.0f}) {
        float start = camera.GetAngle();
        Move(counts, 0.0f);
        InputLatchPad(0);
        camera.AddAngle(0.04f * -MouseLookTurn(&camera, 0.04f, 0.0f));
        camera.Step(1);
        float want = start - counts * 0.2f * kDegree;
        ASSERT_NEAR(Gap(camera.GetAngle(), want), 0.0f, 1e-4f);
        ASSERT_NEAR(Gap(std::atan2(camera.pos[0], camera.pos[2]), want), 0.0f, 1e-4f);
    }

    // The stick's own turn still eases.
    InputLatchPad(0);
    float start = camera.GetAngle();
    camera.AddAngle(-0.04f);
    camera.Step(1);
    ASSERT_TRUE(Gap(camera.GetAngle(), start) > -0.04f && Gap(camera.GetAngle(), start) < 0.0f);

    // A reading the camera did not turn by is gone at the next pad read: the same delta from
    // anything else then only sets where the camera is going.
    Move(100.0f, 0.0f);
    InputLatchPad(0);
    float delta = 0.04f * -MouseLookTurn(&camera, 0.04f, 0.0f);
    InputLatchPad(0);
    float eye_x = camera.pos[0];
    start = camera.GetAngle();
    camera.AddAngle(delta);
    ASSERT_TRUE(camera.GetAngle() == start && camera.pos[0] == eye_x);
}

TEST(PlatformMouseLook, ThirdPersonPitchIsInstantAndKeepsTheEyeCollisionSafe) {
    Settings(0.2f, false);
    CCameraFollow camera(70, 5, 0, 8);
    camera.SetFollow(0, 14, 0);
    camera.Step(-1);
    std::array<float, 4> eye, ref, next_eye, next_ref;
    std::copy_n(camera.pos, 4, eye.begin());
    std::copy_n(camera.ref, 4, ref.begin());
    std::copy_n(camera.next_pos, 4, next_eye.begin());
    std::copy_n(camera.next_ref, 4, next_ref.begin());
    Move(0, -300);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    float base = std::atan2(5.0f, 70.0f);
    float matrix[4][4], again[4][4];
    camera.GetCameraMatrix(matrix);
    ASSERT_NEAR(std::asin(-matrix[1][2]), base - 60 * kDegree, 1e-5f);
    camera.GetCameraMatrix(again);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            ASSERT_EQ(matrix[i][j], again[i][j]);
        }
    }
    ASSERT_EQ(camera.height, 5);
    ASSERT_EQ(camera.distance, 70);
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(camera.pos[i], eye[i]);
        ASSERT_EQ(camera.ref[i], ref[i]);
        ASSERT_EQ(camera.next_pos[i], next_eye[i]);
        ASSERT_EQ(camera.next_ref[i], next_ref[i]);
    }
    // Repeating a zero-motion read retains the view while following the player.
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    camera.SetFollow(5, 14, 0);
    camera.Step(1);
    camera.GetCameraMatrix(again);
    ASSERT_NEAR(std::asin(-again[1][2]), base - 60 * kDegree, 1e-5f);
}

TEST(PlatformMouseLook, PitchLimitDiscardsExcessAndLockOnRestoresFraming) {
    Settings(0.2f, false);
    CCameraFollow camera(70, 5, 0, 8);
    camera.Step(-1);
    Move(0, -4000);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_NEAR(MouseLookViewPitch(&camera, 0), -1.5f, 1e-6f);
    Move(0, 10);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_NEAR(MouseLookViewPitch(&camera, 0), -1.5f + 2 * kDegree, 1e-6f);
    MouseLookControlPitch(&camera, false);
    ASSERT_EQ(MouseLookViewPitch(&camera, .25f), .25f);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&camera, .25f), .25f);
}

TEST(PlatformMouseLook, UncontrolledCamerasAndLaterReadsDoNotConsumePitch) {
    Settings(0.2f, false);
    CCameraFollow camera(70, 5, 0, 8), other(70, 5, 0, 8);
    Move(0, -100);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&other, .3f), .3f);
    InputLatchPad(0);
    ASSERT_EQ(MouseLookViewPitch(&camera, .3f), .3f);
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&camera, .3f), .3f);
}

TEST(PlatformMouseLook, FirstPersonPitchHasTheSameFreerRange) {
    Settings(0.2f, false);
    Move(0, -400);
    InputLatchPad(0);
    ASSERT_NEAR(MouseLookEyeAngleV(0), -80 * kDegree, 1e-6f);
    Move(0, 1000);
    InputLatchPad(0);
    ASSERT_NEAR(MouseLookEyeAngleV(0), 1.5f, 1e-6f);
}

TEST(PlatformMouseLook, PausedViewsHoldPitchWithoutConsumingMotionAndScriptsResetIt) {
    Settings(.2f, false);
    CCameraFollow camera(70, 5, 0, 8);
    camera.Step(-1);
    Move(0, -100);
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    float held = MouseLookViewPitch(&camera, .1f);
    ASSERT_NEAR(held, .1f - 20 * kDegree, 1e-6f);
    Move(0, -200);
    InputLatchPad(0);
    ASSERT_EQ(MouseLookViewPitch(&camera, .1f), held);
    CCamera::StopCamera = 1;
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&camera, .1f), held);
    CCamera::StopCamera = 0;
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&camera, .1f), held);
    camera.FollowOff();
    ASSERT_EQ(MouseLookViewPitch(&camera, .1f), .1f);
    camera.FollowOn();
    InputLatchPad(0);
    MouseLookControlPitch(&camera, true);
    ASSERT_EQ(MouseLookViewPitch(&camera, .1f), .1f);
}

TEST(PlatformMouseLook, TownPitchNeedsTheSameCameraMapAndInputRead) {
    Settings(.2f, false);
    CEditGround ground{};
    ground.Initialize();
    CCameraFollow camera(70, 5, 0, 8);
    camera.Step(-1);
    float base = std::atan2(5.0f, 70.0f);
    Move(0, -100);
    InputLatchPad(0);
    TownMouseBegin(&camera, &ground, 0, 1);
    TownMouseRecord(&camera, false);
    TownMouseApply(&camera, &ground, 1, 1);
    ASSERT_EQ(MouseLookViewPitch(&camera, base), base);
    // Closing an invalid request does not let a later Apply consume its pitch.
    TownMouseApply(&camera, &ground, 0, 1);
    ASSERT_EQ(MouseLookViewPitch(&camera, base), base);
    Move(0, -100);
    InputLatchPad(0);
    TownMouseBegin(&camera, &ground, 0, 1);
    TownMouseRecord(&camera, false);
    TownMouseApply(&camera, &ground, 0, 1);
    ASSERT_NEAR(MouseLookViewPitch(&camera, base), base - 20 * kDegree, 1e-6f);
}

TEST(PlatformMouseLook, InteriorEyeTurnsTheCharacter) {
    static unsigned char dma[2][1024];
    ASSERT_TRUE(scePadInit(0) == 1 && scePadPortOpen(0, 0, dma[0]) == 1 && scePadPortOpen(1, 0, dma[1]) == 1);
    Settings(0.2f, false);
    viewMode = 1;
    CCamera    eye(1.0f);
    CCharacter character;

    // The left stick held or at rest: the mouse turns the character's heading, which the view takes,
    // once, and never through RX, which EdMoveChara reads only while the left stick rests.
    for (std::uint8_t lx : {std::uint8_t{255}, kInputAxisCentre}) {
        InputPadState pad;
        pad.connected = true;
        pad.left_x = lx;
        InputSetOverride(0, &pad);
        for (int i = 0; i < 4; ++i) {
            GamePad.UpDate();
        }
        ASSERT_TRUE((GamePad.GetLXf() != 0.0f) == (lx != kInputAxisCentre));

        character.SetRotation(0.0f, 0.5f, 0.0f);
        viewAngleH = 0.5f;
        InputKeyboardMouse mouse;
        mouse.mouse_dx = 100.0f;
        InputSetScriptedDevices(mouse);
        GamePad.UpDate();
        ASSERT_TRUE(EdGetRXf(1) == GamePad.GetRXf());
        EyeCamera(&eye, &character, 1);
        ASSERT_NEAR(viewAngleH, 0.5f - 20.0f * kDegree, 1e-5f);
        ASSERT_TRUE(character.GetRotation()->y == viewAngleH);
    }

    // Many turns in one read still leave a heading within half a turn.
    InputKeyboardMouse mouse;
    mouse.mouse_dx = 4000.0f;
    InputSetScriptedDevices(mouse);
    GamePad.UpDate();
    EyeCamera(&eye, &character, 1);
    ASSERT_NEAR(Gap(viewAngleH, 0.5f - 820.0f * kDegree), 0.0f, 1e-4f);
    ASSERT_TRUE(std::fabs(viewAngleH) <= std::numbers::pi_v<float>);
}

TEST(PlatformMouseLook, TownTurnStopsAtAWall) {
    // A wall in the plane x = 30 facing the eye, which circles look at 60 from +z.
    CCPoly wall{};
    float  corners[3][4] = {
        {30.0f, -100.0f, -300.0f, 1.0f},
        {30.0f, -100.0f, 300.0f,  1.0f},
        {30.0f, 300.0f,  0.0f,    1.0f}
    };
    for (int i = 0; i < 3; ++i) {
        sceVu0CopyVector(wall.vertex[i], corners[i]);
    }
    wall.normal[0] = -1.0f;
    MouseLookPolys walled = [&](CBoxVu0 &, CCPoly **polys) {
        *polys = &wall;
        return 1;
    };
    MouseLookPolys open = [&](CBoxVu0 &, CCPoly **polys) {
        *polys = &wall;
        return 0;
    };
    float look[4] = {0.0f, 10.0f, 0.0f, 1.0f};
    float eye[4] = {0.0f, 15.0f, 60.0f, 1.0f};
    float whole = 200.0f * kDegree;

    // Towards the wall the turn stops short of it (10 away); away from it, or in the open past half a
    // turn, it is whole.
    float stopped = MouseLookClampOrbit(eye, look, whole, walled);
    ASSERT_GT(stopped, 0.0f);
    ASSERT_LT(stopped, std::asin(20.0f / 60.0f));
    ASSERT_NEAR(MouseLookClampOrbit(eye, look, -100.0f * kDegree, walled), -100.0f * kDegree, 1e-4f);
    ASSERT_NEAR(MouseLookClampOrbit(eye, look, whole, open), whole, 1e-4f);

    // What was stopped is not turned later.
    CCameraFollow camera(60.0f, 5.0f, 0.0f, 8.0f);
    camera.Step(-1);
    MouseLookTurnOrbit(&camera, stopped);
    for (int i = 0; i < 10; ++i) {
        camera.Step(1);
    }
    ASSERT_NEAR(Gap(camera.GetAngle(), stopped), 0.0f, 1e-4f);

    // The town's RX is the stick's alone, so an AddAngle of what the mouse's reading would have been
    // turns nothing at once, and a request outliving its pad read applies nothing.
    Settings(0.2f, false);
    viewMode = 0;
    EdMoveCharaInfo.camera = &camera;
    EdMoveCharaInfo.interior = 0;
    Move(100.0f, 0.0f);
    InputLatchPad(0);
    TownMouseBegin(&camera, nullptr, 0, 0);
    ASSERT_TRUE(EdGetRXf(1) == GamePad.GetRXf());
    float start = camera.GetAngle();
    camera.AddAngle(0.03f * -(20.0f * kDegree / 0.03f));
    ASSERT_TRUE(camera.GetAngle() == start);
    InputLatchPad(0);
    TownMouseApply(&camera, nullptr, 0, 0);
    ASSERT_TRUE(camera.GetAngle() == start);
}

namespace {
CCPoly Triangle(std::array<float, 3> a, std::array<float, 3> b, std::array<float, 3> c) {
    CCPoly                              p{};
    std::array<std::array<float, 3>, 3> points{a, b, c};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            p.vertex[i][j] = points[i][j];
        }
    }
    return p;
}
} // namespace

TEST(PlatformMouseLook, ThinWallStopsTheContinuousRayFan) {
    CCPoly         thin = Triangle({.25f, -100, 20}, {.35f, -100, 20}, {.3f, 100, 20});
    MouseLookPolys walls = [&](CBoxVu0 &, CCPoly **out) { *out=&thin; return 1; };
    float          eye[4] = {0, 15, 60, 1}, look[4] = {0, 10, 0, 1};
    // Both former endpoint samples (0 and .03) miss this triangle, but the ray at .015 hits.
    float accepted = MouseLookClampOrbit(eye, look, .06f, walls);
    ASSERT_GE(accepted, 0);
    ASSERT_LT(accepted, .015f);
}

TEST(PlatformMouseLook, InvalidFloorDoesNotDisableClearance) {
    CCPoly         floor = Triangle({-100, 0, -100}, {100, 0, -100}, {0, 0, 300});
    MouseLookPolys walls = [&](CBoxVu0 &, CCPoly **out) { *out=&floor; return 1; };
    float          eye[4] = {0, 17, 60, 1}, look[4] = {0, 10, 0, 1};
    ASSERT_EQ(MouseLookClampOrbit(eye, look, 45 * kDegree, walls), 0);
}

TEST(PlatformMouseLook, ClearFloorDoesNotLimitAnInstantFlick) {
    CCPoly         floor = Triangle({-1000, 0, -1000}, {1000, 0, -1000}, {0, 0, 3000});
    MouseLookPolys walls = [&](CBoxVu0 &, CCPoly **out) { *out=&floor; return 1; };
    float          eye[4] = {0, 19, 70, 1}, look[4] = {0, 14, 0, 1};
    ASSERT_FLOAT_EQ(MouseLookClampOrbit(eye, look, 200 * kDegree, walls), 200 * kDegree);
    ASSERT_FLOAT_EQ(MouseLookClampOrbit(eye, look, -200 * kDegree, walls), -200 * kDegree);
}

TEST(PlatformMouseLook, ContractedEyeDoesNotLeavePendingTargetAcrossWall) {
    CCPoly        wall = Triangle({30, -100, -300}, {30, -100, 300}, {30, 300, 0});
    CCameraFollow camera(60, 5, 0, 8);
    camera.SetFollow(0, 10, 0);
    camera.Step(-1);
    camera.SetPos(nullptr, 0, 15, 20);
    camera.Step(1);
    float accepted = MouseCameraClamp(camera, 45 * kDegree, &wall, 1);
    ASSERT_GE(accepted, 0);
    ASSERT_LT(accepted, std::asin(20.0f / 60));
    MouseLookTurnOrbit(&camera, accepted);
    for (int i = 0; i < 100; ++i) {
        camera.Step(1);
        ASSERT_LT(camera.pos[0], 20.0f);
        ASSERT_LT(camera.next_pos[0], 20.0f);
    }
}

TEST(PlatformMouseLook, CollectorHandlesMoreThanRetailScratchAndStopsBeforeBudget) {
    CEditGround ground{};
    ground.Initialize();
    CFrame                 frame;
    CCollisionMDT          collision;
    std::vector<CCPolyBox> mesh(401);
    for (auto &p : mesh) {
        p.poly = Triangle({.25f, -100, 20}, {.35f, -100, 20}, {.3f, 100, 20});
        for (int j = 0; j < 3; ++j) {
            p.box.min[j] = std::min({p.poly.vertex[0][j], p.poly.vertex[1][j], p.poly.vertex[2][j]});
            p.box.max[j] = std::max({p.poly.vertex[0][j], p.poly.vertex[1][j], p.poly.vertex[2][j]});
        }
    }
    collision.mesh = mesh.data();
    collision.mesh_count = int(mesh.size());
    frame.flags = 1;
    frame.collision = &collision;
    ground.parts[0].handle = 0;
    ground.parts[0].camera_frame = &frame;
    CBoxVu0 box{};
    for (int j = 0; j < 3; ++j) {
        collision.min[j] = mesh[0].box.min[j];
        collision.max[j] = mesh[0].box.max[j];
        ground.parts[0].bound.min[j] = -100;
        ground.parts[0].bound.max[j] = 100;
        box.min[j] = -200;
        box.max[j] = 200;
    }
    std::vector<CCPoly> output;
    ASSERT_EQ(MouseCameraPolys(ground, box, 0xFFFF, output), 401);
    ASSERT_EQ(output.size(), 401u);
    // No collector call can run on this deliberately undersized fixture when its claimed
    // mesh count exceeds the allocation budget.
    collision.mesh_count = 32769;
    ASSERT_EQ(MouseCameraPolys(ground, box, 0xFFFF, output), -1);
    ASSERT_TRUE(output.empty());
}

TEST(PlatformMouseLook, CollectorAppendsEveryGridAreaAndRejectsInvalidMetadata) {
    CEditGround ground{};
    ground.Initialize();
    CEditArea first{}, second{};
    first.SetSize(16, 16, 20, 1);
    second.SetSize(16, 16, 20, 1);
    second.offset_y = 100;
    ground.areas[0] = &first;
    ground.areas[1] = &second;
    CBoxVu0             box{};
    std::vector<CCPoly> output;
    EXPECT_EQ(MouseCameraPolys(ground, box, 0xffff, output), 1024);
    ASSERT_EQ(output.size(), 1024u);
    EXPECT_EQ(output[0].vertex[0][1], 0);
    EXPECT_EQ(output[512].vertex[0][1], 100);
    second.width = 17;
    EXPECT_EQ(MouseCameraPolys(ground, box, 0xffff, output), -1);
    second.width = 16;
    second.map_no = 5;
    EXPECT_EQ(MouseCameraPolys(ground, box, 0xffff, output), -1);
    ground.areas[0] = ground.areas[1] = nullptr;
}

TEST(PlatformMouseLook, CollectorBoundsVisitsEvenForDisabledSiblingCycles) {
    CEditGround ground{};
    ground.Initialize();
    CFrame parent, child;
    parent.flags = 0;
    child.flags = 4;
    parent.child = &child;
    child.brother = &child;
    ground.parts[0].handle = 0;
    ground.parts[0].camera_frame = &parent;
    CBoxVu0 box{};
    for (int i = 0; i < 3; ++i) {
        ground.parts[0].bound.min[i] = -100;
        ground.parts[0].bound.max[i] = 100;
        box.min[i] = -200;
        box.max[i] = 200;
    }
    std::vector<CCPoly> output;
    int                 result = MouseCameraPolys(ground, box, 0xffff, output);
    parent.child = child.brother = nullptr;
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(output.empty());
}

TEST(PlatformMouseLook, MenuMotionAndFocusLossCannotReachTheCamera) {
    Settings(0.2f, false);
    Move(100, 0);
    InputLatchPad(0);
    ASSERT_NE(InputGetMouseLook().yaw, 0);
    InputSetMenuMouse(true);
    ASSERT_EQ(InputGetMouseLook().yaw, 0);
    Move(100, 0);
    InputLatchPad(0);
    ASSERT_EQ(InputGetMouseLook().yaw, 0);
    InputSetMenuMouse(false);
    InputLatchPad(0);
    ASSERT_EQ(InputGetMouseLook().yaw, 0);
    Move(100, 0);
    InputLatchPad(0);
    ASSERT_NE(InputGetMouseLook().yaw, 0);
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    InputHandleEvent(event);
    ASSERT_EQ(InputGetMouseLook().yaw, 0);
}
