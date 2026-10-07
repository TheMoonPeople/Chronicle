#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <libpad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <memory>
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
#include "dungeonmap.hpp"
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

TEST(PlatformMouseLook, TiltHeight) {
    ASSERT_TRUE(MouseLookTiltHeight(5.0f, 60.0f, 0.0f) == 5.0f);
    // Level, then 45 degrees down: the eye rises to the distance.
    ASSERT_NEAR(MouseLookTiltHeight(0.0f, 60.0f, -45.0f * kDegree), 60.0f, 1e-3f);
    ASSERT_NEAR(MouseLookTiltHeight(60.0f, 60.0f, 45.0f * kDegree), 0.0f, 1e-3f);
    // Short of straight down, and an eye already past that is not pulled back.
    ASSERT_TRUE(std::isfinite(MouseLookTiltHeight(0.0f, 60.0f, -3.0f)));
    ASSERT_TRUE(MouseLookTiltHeight(0.0f, 60.0f, -3.0f) < 60.0f * 20.0f);
    ASSERT_NEAR(MouseLookTiltHeight(40.0f, 0.5f, -0.2f), 40.0f, 1e-2f);
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

TEST(PlatformMouseLook, FollowCameraHeightStaysInBounds) {
    Settings(0.2f, false);
    CCameraFollow camera(60.0f, 5.0f, 0.0f, 8.0f);
    camera.Step(-1);

    // Looking up again and again lowers only where the eye is going, which the game's own minimum
    // (the dungeon's 1.6) puts back each time, so the eye never passes under it.
    for (int read = 0; read < 10; ++read) {
        Move(0.0f, -100.0f);
        InputLatchPad(0);
        camera.AddHeight(-MouseLookRise(&camera, 0.0f, 30.0f));
        if (camera.GetHeight() <= 1.6f) {
            camera.SetHeight(1.6f);
        }
        camera.Step(1);
        ASSERT_TRUE(camera.pos[1] >= 1.6f - 1e-4f);
    }

    // The mouse raises the eye no higher than the ceiling the camera keeps.
    camera.SetHeight(29.0f);
    Move(0.0f, 100.0f);
    InputLatchPad(0);
    ASSERT_NEAR(MouseLookRise(&camera, 0.0f, 30.0f), -1.0f, 1e-4f);
}

TEST(PlatformMouseLook, ThirdPersonHeightKeepsTheFollowTargetAndSoftensDescent) {
    Settings(0.2f, false);
    CCameraFollow camera(60.0f, 5.0f, 0.0f, 8.0f);
    camera.SetFollow(12.0f, 14.0f, 20.0f);
    camera.Step(-1);
    Move(0.0f, 1000.0f);
    InputLatchPad(0);
    camera.AddHeight(-MouseLookRise(&camera, 0.0f, 30.0f, 5.0f));
    ASSERT_FLOAT_EQ(camera.height, 30.0f);
    camera.AddHeight(-0.5f); // The retail descent must not fight this mouse read.
    ASSERT_FLOAT_EQ(camera.height, 30.0f);
    for (int frame = 0; frame < 60; ++frame) {
        ClockPump();
        InputLatchPad(0);
        camera.AddHeight(-MouseLookRise(&camera, 0.0f, 30.0f, 5.0f));
        camera.AddHeight(-std::clamp((camera.height - 5.0f) * 0.05f, 0.15f, 0.5f));
        camera.Step(1);
        ASSERT_FLOAT_EQ(camera.ref[0], 12.0f);
        ASSERT_FLOAT_EQ(camera.ref[1], 14.0f);
        ASSERT_FLOAT_EQ(camera.ref[2], 20.0f);
        ASSERT_NEAR(std::hypot(camera.pos[0] - camera.ref[0], camera.pos[2] - camera.ref[2]), 60.0f, 1e-4f);
        ASSERT_GT(camera.pos[1], camera.ref[1]);
    }
    // Retail would already be nearly back to baseline after one second.
    ASSERT_NEAR(camera.height, 24.0f, 1e-3f);
    // Collision raises remain immediate, even during mouse ownership.
    camera.AddHeight(7.0f);
    ASSERT_NEAR(camera.height, 31.0f, 1e-3f);
    // No gameplay height read in this pad read: a scripted descent stays retail.
    ClockPump();
    InputLatchPad(0);
    camera.AddHeight(-0.5f);
    ASSERT_NEAR(camera.height, 30.5f, 1e-3f);
}

TEST(PlatformMouseLook, ThirdPersonHeightBoundsReverseAndLeaveControllerUnchanged) {
    Settings(0.2f, false);
    CCameraFollow camera(60.0f, 30.0f, 0.0f, 8.0f);
    camera.Step(-1);
    Move(0.0f, -1000.0f);
    InputLatchPad(0);
    camera.AddHeight(-MouseLookRise(&camera, 0.0f, 30.0f, 5.0f));
    ASSERT_FLOAT_EQ(camera.height, 5.0f);
    Move(0.0f, 10.0f);
    InputLatchPad(0);
    camera.AddHeight(-MouseLookRise(&camera, 0.0f, 30.0f, 5.0f));
    ASSERT_GT(camera.height, 5.0f);
    // Explicit stick input retains both its height delta and retail descent.
    camera.SetHeight(20.0f);
    InputLatchPad(0);
    camera.AddHeight(-MouseLookRise(&camera, 0.5f, 30.0f, 5.0f));
    ASSERT_FLOAT_EQ(camera.height, 19.5f);
    camera.AddHeight(-0.5f);
    ASSERT_FLOAT_EQ(camera.height, 19.0f);
    camera.FollowOff();
    MouseLookRise(&camera, 0.0f, 30.0f, 5.0f);
    camera.FollowOn();
    camera.AddHeight(-0.5f);
    ASSERT_FLOAT_EQ(camera.height, 18.5f);
}

TEST(PlatformMouseLook, TownMouseCanLowerTheCameraAtItsCeiling) {
    Settings(0.2f, false);
    CCameraFollow camera(60.0f, 30.0f, 0.0f, 8.0f);
    camera.Step(-1);
    EdMoveCharaInfo.camera = &camera;
    EdMoveCharaInfo.interior = 0;
    EdDebugCameraFlag = 0;
    Move(0.0f, -10.0f);
    InputLatchPad(0);
    ASSERT_FLOAT_EQ(EdGetRYf(1), 0.0f);
    ASSERT_LT(camera.height, 30.0f);
    ASSERT_GT(camera.height, 5.0f);
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

TEST(PlatformMouseLook, ZoomDistanceCannotCrossAWallOrLeaveAnUnsafePendingTarget) {
    CCameraFollow camera(60.0f, 30.0f, 0.0f, 8.0f);
    camera.Step(-1);
    CCPoly wall = Triangle({-200, -100, 90}, {200, -100, 90}, {0, 200, 90});
    float accepted = MouseCameraClampDistance(camera, 140.0f, &wall, 1);
    ASSERT_GT(accepted, 60.0f);
    ASSERT_LT(accepted, 80.0f);
    camera.SetDistance(accepted);
    for (int step = 0; step < 100; ++step) {
        camera.Step(1);
        ASSERT_LT(camera.pos[2], 80.0f);
        ASSERT_LT(camera.next_pos[2], 80.0f);
        ASSERT_FLOAT_EQ(camera.ref[2], 0.0f);
    }
    ASSERT_FLOAT_EQ(MouseCameraClampDistance(camera, 40.0f, &wall, 1), 40.0f);
    ASSERT_FLOAT_EQ(MouseCameraClampDistance(camera, 100.0f, nullptr, -1), camera.distance);
    camera.next_pos[2] = 100.0f;
    ASSERT_FLOAT_EQ(MouseCameraClampDistance(camera, 40.0f, &wall, 1), camera.distance);
}

TEST(PlatformMouseLook, DungeonZoomCollectorRejectsInvalidCellsAndOversizedMeshes) {
    auto map = std::make_unique<CDungeonMap>();
    map->map_type = 1;
    for (auto &cell : map->cells) cell.parts_no = -1;
    CBoxVu0 box{};
    for (int i = 0; i < 3; ++i) {
        box.min[i] = -1000.0f;
        box.max[i] = 1000.0f;
    }
    std::vector<CCPoly> output;
    ASSERT_EQ(MouseCameraPolys(*map, box, output), 0);
    map->cells[0].parts_no = 72;
    ASSERT_EQ(MouseCameraPolys(*map, box, output), -1);
    map->cells[0].parts_no = 0;
    map->cells[0].direction = 0;
    CFrame frame;
    CCollisionMDT collision;
    frame.flags = 1;
    frame.collision = &collision;
    collision.mesh_count = 32769;
    map->parts[0].camera_collision = &frame;
    ASSERT_EQ(MouseCameraPolys(*map, box, output), -1);
    ASSERT_TRUE(output.empty());
}

TEST(PlatformMouseLook, LowDungeonCameraZoomRetainsFloorAndWallClearance) {
    CCameraFollow camera(60.0f, 1.6f, 0.0f, 8.0f);
    camera.SetFollow(0.0f, 6.0f, 0.0f);
    camera.Step(-1);
    CCPoly floor = Triangle({-1000, 0, -1000}, {1000, 0, -1000}, {0, 0, 1000});
    for (int step = 0; step < 240; ++step) {
        float goal = step < 120 ? 100.0f : 60.0f;
        camera.SetDistance(MouseCameraClampDistance(camera, goal, &floor, 1, 5.0f));
        camera.Step(1);
        ASSERT_GT(camera.pos[1], 5.0f);
        if (step == 119) ASSERT_NEAR(camera.distance, 100.0f, 1e-3f);
    }
    ASSERT_NEAR(camera.distance, 60.0f, 1e-3f);
    CCPoly wall = Triangle({-200, -100, 90}, {200, -100, 90}, {0, 200, 90});
    ASSERT_LT(MouseCameraClampDistance(camera, 140.0f, &wall, 1, 5.0f), 80.0f);
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
