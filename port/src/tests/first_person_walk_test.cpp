#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <libpad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

#include "../first_person_walk.hpp"
#include "btactstatus.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "collision.hpp"
#include "dungeonmap.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "platform/clock.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"

extern int   viewMode, chara_mode, chara_fishing;
extern float viewAngleH, viewAngleV;
void         EyeCamera(CCamera *, CCharacter *, int);
int          PortEdMoveCheck(float *, float *, float *, MoveCheckInfo *, CCPoly *, int, int);

namespace {
void Keyboard(const std::vector<int> &keys, float dx = 0) {
    InputKeyboardMouse held;
    held.keys = keys;
    held.mouse_dx = dx;
    InputSetScriptedDevices(held);
    InputLatchPad(0);
}

void PrepareInput() {
    InputResetBindings();
    InputSetMovementLocked(false);
    InputSetMenuMouse(false);
    InputMouseSettings mouse;
    mouse.capture = false;
    InputSetMouseSettings(mouse);
    InputPadState pad;
    pad.connected = true;
    InputSetOverride(0, &pad);
    ClockSetUnbounded(true);
    ClockReset();
}

CCPoly Poly(std::array<float, 3> a, std::array<float, 3> b, std::array<float, 3> c, std::array<float, 3> normal) {
    CCPoly     p{};
    std::array vertices{a, b, c};
    for (int j = 0; j < 3; ++j) {
        p.normal[j] = normal[j];
        for (int i = 0; i < 3; ++i) {
            p.vertex[i][j] = vertices[i][j];
        }
    }
    return p;
}
} // namespace

TEST(FirstPersonWalk, KeyboardBindingsNormalizeCancelAndRespectMenuFocusAndPadLocks) {
    PrepareInput();
    Keyboard({SDL_SCANCODE_W, SDL_SCANCODE_D});
    auto move = InputGetKeyboardMovement();
    EXPECT_NEAR(std::hypot(move.x, move.y), 1, 1e-6);
    EXPECT_GT(move.x, 0);
    EXPECT_LT(move.y, 0);
    Keyboard({SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D});
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().x, 0);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
    std::string_view keys[]{"Up"};
    ASSERT_TRUE(InputBindKeys("ly-", keys));
    Keyboard({SDL_SCANCODE_W});
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
    Keyboard({SDL_SCANCODE_UP});
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, -1);
    InputSetMenuMouse(true);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
    InputSetMenuMouse(false);
    GamePad.KeyLock(1);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
    GamePad.KeyLock(0);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, -1);
    InputSetOverride(0, nullptr);
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_UP;
    event.key.down = true;
    InputHandleEvent(event);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, -1);
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    InputHandleEvent(event);
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
}

TEST(FirstPersonWalk, TownKeyboardMovesAlongMouseHeadingWithoutSteeringViewAndLocksStopWalking) {
    PrepareInput();
    CMainChara character;
    character.Initialize();
    character.move_info = {};
    EdMoveCharaInfo.chara = &character;
    viewMode = 1;
    chara_mode = 0;
    chara_fishing = 0;
    viewAngleH = 0;
    viewAngleV = 0;
    EdSetKeyMode(1);
    Keyboard({SDL_SCANCODE_W}, 100);
    sceVu0FVECTOR pos{0, 0, 0, 1}, velocity{0, 0, 0, 0}, next{};
    MoveCheckInfo info{};
    PortEdMoveCheck(pos, velocity, next, &info, nullptr, 0, 0);
    float heading = -100 * 0.1f * 3.14159265358979323846f / 180;
    EXPECT_NEAR(velocity[0], 0.6f * std::sin(heading), 1e-5);
    EXPECT_NEAR(velocity[2], 0.6f * std::cos(heading), 1e-5);
    CCamera eye(1);
    EyeCamera(&eye, &character, 0);
    EXPECT_NEAR(viewAngleH, heading, 1e-5);
    EXPECT_FLOAT_EQ(viewAngleV, 0);
    Keyboard({SDL_SCANCODE_D});
    velocity[0] = velocity[2] = 0;
    PortEdMoveCheck(pos, velocity, next, &info, nullptr, 0, 0);
    EXPECT_NEAR(velocity[0], 0.6f * std::cos(heading), 1e-5);
    EXPECT_NEAR(velocity[2], -0.6f * std::sin(heading), 1e-5);
    EyeCamera(&eye, &character, 0);
    EXPECT_NEAR(viewAngleH, heading, 1e-5);
    for (int locked : {1, 2}) {
        chara_mode = locked;
        velocity[0] = velocity[2] = 0;
        PortEdMoveCheck(pos, velocity, next, &info, nullptr, 0, 0);
        EXPECT_FLOAT_EQ(velocity[0], 0);
        EXPECT_FLOAT_EQ(velocity[2], 0);
    }
    chara_mode = 0;
    EdMoveCharaInfo.key_lock = 1;
    velocity[0] = velocity[2] = 0;
    PortEdMoveCheck(pos, velocity, next, &info, nullptr, 0, 0);
    EXPECT_FLOAT_EQ(velocity[0], 0);
    EXPECT_FLOAT_EQ(velocity[2], 0);
}

TEST(FirstPersonWalk, DungeonWalkUsesBodyFloorWallAndObjectCollisionAndRejectsMalformedMaps) {
    auto map = std::make_unique<CDungeonMap>();
    map->map_type = 1;
    for (auto &cell : map->cells) {
        cell.parts_no = -1;
    }
    map->cells[0].parts_no = 0;
    map->cells[0].direction = 0;
    CFrame                   frame;
    CCollisionMDT            collision;
    std::array<CCPolyBox, 3> mesh{};
    mesh[0].poly = Poly({-100, 0, -100}, {-100, 0, 100}, {100, 0, -100}, {0, 1, 0});
    mesh[1].poly = Poly({100, 0, -100}, {-100, 0, 100}, {100, 0, 100}, {0, 1, 0});
    mesh[2].poly = Poly({-100, -100, 10}, {0, 100, 10}, {100, -100, 10}, {0, 0, -1});
    for (int j = 0; j < 3; ++j) {
        collision.min[j] = 1000;
        collision.max[j] = -1000;
        for (auto &m : mesh) {
            m.box.min[j] = std::min({m.poly.vertex[0][j], m.poly.vertex[1][j], m.poly.vertex[2][j]});
            m.box.max[j] = std::max({m.poly.vertex[0][j], m.poly.vertex[1][j], m.poly.vertex[2][j]});
            collision.min[j] = std::min(collision.min[j], m.box.min[j]);
            collision.max[j] = std::max(collision.max[j], m.box.max[j]);
        }
    }
    collision.mesh = mesh.data();
    collision.mesh_count = 3;
    frame.flags = 1;
    frame.collision = &collision;
    map->parts[0].collision = &frame;
    sceVu0FVECTOR pos{0, 0, 0, 1}, velocity{};
    MoveCheckInfo info{};
    for (int tick = 0; tick < 100; ++tick) {
        velocity[0] = 0;
        velocity[1] = -.1f;
        velocity[2] = .6f;
        ASSERT_TRUE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
        EXPECT_NEAR(pos[1], 0, 1e-5);
        EXPECT_TRUE(info.landed);
        EXPECT_LE(pos[2], 5.001f);
    }
    EXPECT_GT(pos[2], 4);
    map->map_type = 0;
    map->parts[0].frame[0] = &frame;
    pos[0] = pos[2] = 0;
    ASSERT_TRUE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
    EXPECT_GT(pos[2], 0);
    map->parts[0].collision = nullptr;
    map->box_collision_model = &frame;
    map->boxes[0].used = 1;
    for (int tick = 0; tick < 50; ++tick) {
        velocity[2] = .6f;
        velocity[1] = -.1f;
        ASSERT_TRUE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
        EXPECT_LE(pos[2], 5.001f);
    }
    map->boxes[0].used = 0;
    map->collision_model = &frame;
    map->atra_num = 1;
    map->atra[0].used = 1;
    velocity[2] = .6f;
    ASSERT_TRUE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
    EXPECT_LE(pos[2], 5.001f);
    map->parts[0].collision = &frame;
    map->map_type = 1;
    map->cells[0].parts_no = 72;
    auto z = pos[2];
    EXPECT_FALSE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
    EXPECT_FLOAT_EQ(pos[2], z);
    map->cells[0].parts_no = 0;
    collision.mesh_count = 32769;
    EXPECT_FALSE(FirstPersonDungeonStep(*map, nullptr, pos, velocity, info, 1));
    EXPECT_FLOAT_EQ(pos[2], z);
}

TEST(FirstPersonWalk, ReservedZoomResetCannotAlsoWalkAndDungeonScriptLocksKeepTheirOwnership) {
    PrepareInput();
    Config config;
    config.mouse_zoom = true;
    config.mouse_capture = false;
    InputApplyConfig(config);
    std::string_view reset[]{"W"};
    ASSERT_TRUE(InputBindKeys("zoom_reset", reset));
    Keyboard({SDL_SCANCODE_W});
    EXPECT_FLOAT_EQ(InputGetKeyboardMovement().y, 0);
    InputResetBindings();
    Keyboard({SDL_SCANCODE_W});
    BT_ACT_STATUS status{};
    status.can_act = 1;
    sceVu0FVECTOR velocity{0, -0.1f, 0, 0};
    ASSERT_TRUE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, false, true, false));
    EXPECT_FLOAT_EQ(velocity[2], 0.6f);
    EXPECT_FLOAT_EQ(velocity[1], -0.1f);
    status.can_act = 0;
    status.movement_locked = 1;
    EXPECT_FALSE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, false, true, false));
    EXPECT_FLOAT_EQ(velocity[2], 0);
    EXPECT_EQ(status.movement_locked, 1);
    status.can_act = 1;
    EXPECT_FALSE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, false, true, false));
    status.movement_locked = 0;
    status.action_on = 1;
    EXPECT_FALSE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, false, true, false));
    status.action_on = 0;
    EXPECT_FALSE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, true, true, false));
    EXPECT_FALSE(FirstPersonDungeonKeyboardVelocity(velocity, 0, status, false, false, false));
}
