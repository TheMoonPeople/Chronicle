#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>

#include "../camera_zoom.hpp"
#include "../platform/clock.hpp"
#include "../platform/config.hpp"
#include "../platform/input.hpp"
#include "../platform/paths.hpp"
#include "camerafollow.hpp"
#include "collision.hpp"
#include "dungeonmap.hpp"
#include "frame.hpp"
#include "platform_fixture.hpp"

TEST(CameraZoom, RetailRelaxationCannotExpandPastAWallAndResetReturnsToNormal) {
    auto root = std::filesystem::temp_directory_path() /
                ("chronicle-zoom-test-" + std::to_string(dc::test::ProcessId()));

    struct Cleanup {
        std::filesystem::path root;

        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(root / "config.json", error);
            std::filesystem::remove(root, error);
        }
    } cleanup{root};

    PathsSetSaveRoot(root);
    Config config;
    config.mouse_zoom = true;
    config.mouse_capture = false;
    ASSERT_TRUE(ConfigChange(config));
    InputApplyConfig(config);
    ClockSetUnbounded(true);
    ClockReset();

    auto map = std::make_unique<CDungeonMap>();
    map->map_type = 0;
    CFrame        frame;
    CCollisionMDT collision;
    CCPolyBox     mesh{};
    float         points[3][3] = {
        {-200, -100, 90},
        {200,  -100, 90},
        {0,    200,  90}
    };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            mesh.poly.vertex[i][j] = points[i][j];
        }
    }
    for (int j = 0; j < 3; ++j) {
        mesh.box.min[j] = collision.min[j] = std::min({points[0][j], points[1][j], points[2][j]});
        mesh.box.max[j] = collision.max[j] = std::max({points[0][j], points[1][j], points[2][j]});
    }
    collision.mesh = &mesh;
    collision.mesh_count = 1;
    frame.flags = 1;
    frame.collision = &collision;
    map->parts[0].frame[0] = &frame;
    map->parts[0].camera_collision = &frame;

    CCameraFollow camera(60.0f, 30.0f, 0.0f, 8.0f);
    camera.Step(-1);
    InputKeyboardMouse input;
    input.mouse_wheel = -5.0f;
    InputSetScriptedDevices(input);
    InputLatchPad(0);
    camera.SetDistance(62.0f); // Retail begins restoring toward 80 before the zoom hook.
    DungeonZoomApply(&camera, map.get(), false, 60.0f);
    ASSERT_GT(camera.distance, 60.0f);
    ASSERT_LT(camera.distance, 80.0f);
    InputSetScriptedDevices({});
    for (int step = 0; step < 100; ++step) {
        ClockPump();
        InputLatchPad(0);
        float previous = camera.distance;
        camera.AddDistance((80.0f - previous) / 10.0f);
        DungeonZoomApply(&camera, map.get(), false, previous);
        camera.Step(1);
        ASSERT_LT(camera.pos[2], 80.0f);
        ASSERT_LT(camera.next_pos[2], 80.0f);
    }
    input = {};
    input.mouse_buttons = 1u << 2; // Logical Mouse3, the default reset binding.
    InputSetScriptedDevices(input);
    ClockPump();
    InputLatchPad(0);
    ASSERT_TRUE(InputGetMouseLook().zoom_reset);
    DungeonZoomApply(&camera, map.get(), false, camera.distance);
    ASSERT_FLOAT_EQ(camera.distance, 60.0f);
    input = {};
    input.mouse_wheel = 3.0f;
    InputSetScriptedDevices(input);
    ClockPump();
    InputLatchPad(0);
    DungeonZoomApply(&camera, map.get(), false, 60.0f);
    ASSERT_FLOAT_EQ(camera.distance, 36.0f);
    ASSERT_FLOAT_EQ(DungeonZoomNearDistance(&camera, 60.0f, false), 36.0f);
    ASSERT_FLOAT_EQ(DungeonZoomNearDistance(&camera, 60.0f, true), 60.0f);
    ASSERT_FLOAT_EQ(DungeonZoomNearDistance(nullptr, 60.0f, false), 60.0f);
}
