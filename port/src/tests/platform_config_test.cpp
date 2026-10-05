#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../platform/config.hpp"
#include "../platform/paths.hpp"

TEST(PlatformConfig, Defaults) {
    Config config = ConfigParse("");
    ASSERT_TRUE(config.tick_rate == 50.0);
    ASSERT_TRUE(config.present_mode == ConfigPresentMode::Fifo);
    ASSERT_TRUE(!config.fullscreen);
    ASSERT_TRUE(config.master_volume == 1.0f);
    ASSERT_TRUE(config.key_bindings.empty());
    ASSERT_TRUE(config.detail_distance == 0.0f);
    ASSERT_TRUE(config.shadow_distance == 0.0f);
}

TEST(PlatformConfig, DetailDistance) {
    ASSERT_TRUE(ConfigParse(R"({"video": {"detail_distance": 450}})").detail_distance == 450.0f);
    ASSERT_TRUE(ConfigParse(R"({"video": {"detail_distance": -1}})").detail_distance == 0.0f);
    ASSERT_TRUE(ConfigParse(R"({"video": {"detail_distance": "far"}})").detail_distance == 0.0f);
    Config config;
    config.detail_distance = 450.0f;
    ASSERT_TRUE(ConfigParse(ConfigSerialize(config)).detail_distance == 450.0f);
}

TEST(PlatformConfig, ShadowDistance) {
    ASSERT_TRUE(ConfigParse(R"({"video": {"shadow_distance": 320}})").shadow_distance == 320.0f);
    ASSERT_TRUE(ConfigParse(R"({"video": {"shadow_distance": -1}})").shadow_distance == 0.0f);
    ASSERT_TRUE(ConfigParse(R"({"video": {"shadow_distance": "far"}})").shadow_distance == 0.0f);
    Config config;
    config.shadow_distance = 320.0f;
    ASSERT_TRUE(ConfigParse(ConfigSerialize(config)).shadow_distance == 320.0f);
}

TEST(PlatformConfig, ParsesJson) {
    Config config = ConfigParse(R"({
        // comment
        "game": {"tick_rate": 59.94},
        "video": {"vsync": false, "width": 1920, "height": 1080, "fullscreen": true},
        "audio": {"master_volume": 1.5},
        "input": {"bindings": {"cross": ["Space", "Z"], "Start": "Return", "circle": 3}},
        "extra": {"tick_rate": -3},
        "stray": 1
    })");
    ASSERT_TRUE(config.tick_rate == 59.94);
    ASSERT_TRUE(config.present_mode == ConfigPresentMode::Immediate);
    ASSERT_TRUE(config.window_width == 1920 && config.window_height == 1080);
    ASSERT_TRUE(config.fullscreen);
    ASSERT_TRUE(config.master_volume == 1.0f);
    ASSERT_TRUE(config.key_bindings.size() == 2);
    ASSERT_TRUE(config.key_bindings[0].action == "cross");
    ASSERT_TRUE(config.key_bindings[0].keys.size() == 2);
    ASSERT_TRUE(config.key_bindings[0].keys[0] == "Space" && config.key_bindings[0].keys[1] == "Z");
    ASSERT_TRUE(config.key_bindings[1].action == "start" && config.key_bindings[1].keys[0] == "Return");

    Config bad = ConfigParse(R"({"game": {"tick_rate": -3, "unknown": 1}, "video": {"width": 1.5, "height": "tall"}})");
    ASSERT_TRUE(bad.tick_rate == 50.0 && bad.window_width == 0 && bad.window_height == 0);

    ASSERT_TRUE(ConfigParse(R"({"video": {"present_mode": "Mailbox"}})").present_mode == ConfigPresentMode::Mailbox);
}

TEST(PlatformConfig, InvalidJsonKeepsTheDefaults) {
    ASSERT_TRUE(ConfigParse("[game]\ntick_rate = 60\n").tick_rate == 50.0);
    ASSERT_TRUE(ConfigParse(R"({"game": {"tick_rate": 60})").tick_rate == 50.0);
    ASSERT_TRUE(ConfigParse("[1, 2]").tick_rate == 50.0);
    ASSERT_TRUE(ConfigParse(" \n").tick_rate == 50.0);
}

TEST(PlatformConfig, LoadsFromSaveRoot) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("dc_config_test_" + std::to_string(getpid()));
    std::filesystem::create_directories(root);
    PathsSetSaveRoot(root);
    ASSERT_TRUE(PathsSaveRoot() == root);
    // A missing file is created with the defaults, and read from then on.
    ASSERT_TRUE(!ConfigLoad());
    ASSERT_TRUE(std::filesystem::exists(root / "config.json"));
    ASSERT_TRUE(ConfigLoad());
    ASSERT_TRUE(ConfigGet().tick_rate == 50.0 && ConfigGet().debug_mode);
    std::ofstream(root / "config.json") << R"({"game": {"tick_rate": 60}})";
    ASSERT_TRUE(ConfigLoad());
    ASSERT_TRUE(ConfigGet().tick_rate == 60.0);
    std::filesystem::remove_all(root);
}

TEST(PlatformConfig, ParsesMouseSettingsAndBindings) {
    Config defaults = ConfigParse("");
    ASSERT_TRUE(defaults.mouse_sensitivity == 0.1f && !defaults.mouse_invert_y && defaults.mouse_capture);
    ASSERT_TRUE(defaults.stick_sensitivity == 1.33f);
    ASSERT_TRUE(defaults.mouse_release_keys.size() == 1 && defaults.mouse_release_keys[0] == "Escape");

    Config config = ConfigParse(R"({"input": {
        "mouse_sensitivity": 0.25,
        "stick_sensitivity": 1.5,
        "mouse_invert_y": true,
        "mouse_capture": false,
        "mouse_release": ["F12", "Pause"],
        "bindings": {"rx": "MouseX*2", "ry": ["-MouseY"], "r1": ["Mouse2", "X"]}
    }})");
    ASSERT_TRUE(config.mouse_sensitivity == 0.25f && config.stick_sensitivity == 1.5f);
    ASSERT_TRUE(config.mouse_invert_y && !config.mouse_capture);
    ASSERT_TRUE(config.mouse_release_keys.size() == 2 && config.mouse_release_keys[1] == "Pause");
    ASSERT_TRUE(config.key_bindings.size() == 3);
    ASSERT_TRUE(config.key_bindings[0].action == "rx" && config.key_bindings[0].keys[0] == "MouseX*2");
    ASSERT_TRUE(config.key_bindings[1].keys[0] == "-MouseY");
    ASSERT_TRUE(config.key_bindings[2].keys.size() == 2 && config.key_bindings[2].keys[0] == "Mouse2");

    Config bad = ConfigParse(R"({"input": {"mouse_sensitivity": -1, "stick_sensitivity": 0, "mouse_capture": "maybe"}})");
    ASSERT_TRUE(bad.mouse_sensitivity == 0.1f && bad.stick_sensitivity == 1.33f && bad.mouse_capture);
    ASSERT_TRUE(ConfigParse(R"({"input": {"mouse_release": []}})").mouse_release_keys.empty());
}

TEST(PlatformConfig, DebugModeDefaultsOn) {
    ASSERT_TRUE(ConfigParse("").debug_mode);
    ASSERT_TRUE(ConfigParse(R"({"game": {"debug_mode": true}})").debug_mode);
    ASSERT_TRUE(!ConfigParse(R"({"game": {"debug_mode": false}})").debug_mode);
    ASSERT_TRUE(ConfigParse(R"({"game": {"debug_mode": "off"}})").debug_mode);
}

TEST(PlatformConfig, ShowFpsAndTheHostKeys) {
    ASSERT_TRUE(ConfigParse("").show_fps);
    ASSERT_TRUE(!ConfigParse(R"({"video": {"show_fps": false}})").show_fps);
    ASSERT_TRUE(ConfigParse(R"({"video": {"show_fps": "maybe"}})").show_fps);

    // The toggle is a binding like the pad's; its default (F3) is input's.
    Config config = ConfigParse(R"({"input": {"bindings": {"fps_toggle": ["F4", "Gamepad:guide"], "start": "Return"}}})");
    ASSERT_TRUE(config.key_bindings.size() == 2);
    ASSERT_TRUE(config.key_bindings[0].action == "fps_toggle");
    ASSERT_TRUE((config.key_bindings[0].keys == std::vector<std::string>{"F4", "Gamepad:guide"}));
    ASSERT_TRUE(config.key_bindings[1].action == "start" && config.key_bindings[1].keys.size() == 1);
}

TEST(PlatformConfig, SerializeRoundTrips) {
    Config config = ConfigParse(R"({
        "game": {"tick_rate": 60, "debug_mode": false},
        "video": {"present_mode": "mailbox", "aspect": "4:3", "ui_scale": 1.5, "width": 1920, "max_fps": 144},
        "audio": {"master_volume": 0.5},
        "input": {"mouse_release": [], "bindings": {"cross": ["Space", "Z"], "start": "Return"}}
    })");
    Config again = ConfigParse(ConfigSerialize(config));
    ASSERT_TRUE(again.tick_rate == 60.0 && !again.debug_mode);
    ASSERT_TRUE(again.present_mode == ConfigPresentMode::Mailbox && again.aspect == ConfigAspect::FourThree);
    ASSERT_TRUE(again.ui_scale == 1.5f && again.window_width == 1920 && again.window_height == 0);
    ASSERT_TRUE(again.max_fps == 144.0 && again.master_volume == 0.5f);
    ASSERT_TRUE(again.mouse_release_keys.empty());
    ASSERT_TRUE(again.key_bindings.size() == 2 && again.key_bindings[0].action == "cross");
    ASSERT_TRUE((again.key_bindings[0].keys == std::vector<std::string>{"Space", "Z"}));
    ASSERT_TRUE(again.key_bindings[1].keys.size() == 1 && again.key_bindings[1].keys[0] == "Return");
}
