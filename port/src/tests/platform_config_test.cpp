#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "../platform/config.hpp"
#include "../platform/display.hpp"
#include "../platform/paths.hpp"
#include "platform_fixture.hpp"

TEST(PlatformConfig, Defaults) {
    Config config = ConfigParse("");
    ASSERT_TRUE(config.tick_rate == 60.0);
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

TEST(PlatformConfig, OptionalMouseZoomRoundTripsAndKeepsResetBindings) {
    ASSERT_FALSE(ConfigParse("").mouse_zoom);
    ASSERT_FALSE(ConfigParse(R"({"input":{"mouse_zoom":"on"}})").mouse_zoom);
    Config config = ConfigParse(R"({"input":{"mouse_zoom":true,"bindings":{
        "zoom_reset":["Mouse5","Home"],"cross":["Space","Z"]}}})");
    ASSERT_TRUE(config.mouse_zoom);
    Config restored = ConfigParse(ConfigSerialize(config));
    ASSERT_EQ(restored, config);
    ASSERT_EQ(restored.key_bindings.size(), 2u);
    ASSERT_FALSE(ConfigAppliesOnRestart("input.mouse_zoom"));
    ASSERT_FALSE(ConfigAppliesOnRestart("input.bindings.zoom_reset"));
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
    ASSERT_TRUE(bad.tick_rate == 60.0 && bad.window_width == 0 && bad.window_height == 0);

    ASSERT_TRUE(ConfigParse(R"({"video": {"present_mode": "Mailbox"}})").present_mode == ConfigPresentMode::Mailbox);
}

TEST(PlatformConfig, InvalidJsonKeepsTheDefaults) {
    ASSERT_TRUE(ConfigParse("[game]\ntick_rate = 50\n").tick_rate == 60.0);
    ASSERT_TRUE(ConfigParse(R"({"game": {"tick_rate": 50})").tick_rate == 60.0);
    ASSERT_TRUE(ConfigParse("[1, 2]").tick_rate == 60.0);
    ASSERT_TRUE(ConfigParse(" \n").tick_rate == 60.0);
}

TEST(PlatformConfig, LoadsFromSaveRoot) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("dc_config_test_" + std::to_string(dc::test::ProcessId()));
    std::filesystem::create_directories(root);
    PathsSetSaveRoot(root);
    ASSERT_TRUE(PathsSaveRoot() == root);
    // A missing file is created with the defaults, and read from then on.
    ASSERT_TRUE(!ConfigLoad());
    ASSERT_TRUE(std::filesystem::exists(root / "config.json"));
    ASSERT_TRUE(ConfigLoad());
    ASSERT_TRUE(ConfigGet().tick_rate == 60.0 && !ConfigGet().debug_mode);
    std::ofstream(root / "config.json") << R"({"game": {"tick_rate": 50}})";
    ASSERT_TRUE(ConfigLoad());
    ASSERT_TRUE(ConfigGet().tick_rate == 50.0);
    std::filesystem::remove_all(root);
}

TEST(PlatformConfig, ParsesMouseSettingsAndBindings) {
    Config defaults = ConfigParse("");
    ASSERT_TRUE(defaults.mouse_sensitivity == 0.2f && !defaults.mouse_invert_y && defaults.mouse_capture);
    ASSERT_TRUE(defaults.stick_sensitivity == 1.33f && defaults.gyro_sensitivity == 0.5f);
    ASSERT_TRUE(defaults.gyro == ConfigGyro::Held && !defaults.gyro_invert_x && !defaults.stick_invert_y);
    ASSERT_TRUE(ConfigParse(R"({"input": {"gyro": "first_person"}})").gyro == ConfigGyro::FirstPerson);
    ASSERT_TRUE(ConfigParse(R"({"input": {"gyro": true}})").gyro == ConfigGyro::Held);
    Config inverted =
        ConfigParse(ConfigSerialize(ConfigParse(R"({"input": {"gyro": "always", "gyro_invert_y": true, "stick_invert_x": true}})")));
    ASSERT_TRUE(inverted.gyro == ConfigGyro::Always && inverted.gyro_invert_y && inverted.stick_invert_x);
    ASSERT_TRUE(ConfigParse(R"({"input": {"gyro_sensitivity": 1.25}})").gyro_sensitivity == 1.25f);
    ASSERT_TRUE(ConfigParse(R"({"input": {"gyro_sensitivity": -1}})").gyro_sensitivity == 0.5f);
    ASSERT_TRUE(ConfigParse(ConfigSerialize(ConfigParse(R"({"input": {"gyro_sensitivity": 1.25}})"))).gyro_sensitivity ==
                1.25f);
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
    ASSERT_TRUE(bad.mouse_sensitivity == 0.2f && bad.stick_sensitivity == 1.33f && bad.mouse_capture);
    ASSERT_TRUE(ConfigParse(R"({"input": {"mouse_release": []}})").mouse_release_keys.empty());
}

TEST(PlatformConfig, DebugModeDefaultsOff) {
    ASSERT_TRUE(!ConfigParse("").debug_mode);
    ASSERT_TRUE(ConfigParse(R"({"game": {"debug_mode": true}})").debug_mode);
    ASSERT_TRUE(!ConfigParse(R"({"game": {"debug_mode": false}})").debug_mode);
    ASSERT_TRUE(!ConfigParse(R"({"game": {"debug_mode": "off"}})").debug_mode);
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

TEST(PlatformConfig, GameOptionsLiveInTheirPagesSections) {
    ConfigGameOptions defaults = ConfigParse("").options;
    ASSERT_TRUE(defaults == ConfigGameOptions{});
    ASSERT_TRUE(defaults.stereo && defaults.vibration && defaults.map == 2 && !defaults.fast_time);

    Config            parsed = ConfigParse(R"({
        "game": {"save_cursor_position": false, "message_speed": "Fast", "clock": false, "time_speed": "fast",
                 "map": 0, "enemy_damage": false, "player_damage": false, "enemy_hp": false, "names": false},
        "video": {"soft_focus": false},
        "audio": {"sound": "mono"},
        "input": {"vibration": false}
    })");
    ConfigGameOptions options = parsed.options;
    ASSERT_TRUE(!options.save_cursor_position && options.fast_messages && !options.clock && options.fast_time);
    ASSERT_TRUE(options.map == 0 && !options.enemy_damage && !options.player_damage && !options.enemy_hp);
    ASSERT_TRUE(!options.names && !options.soft_focus && !options.stereo && !options.vibration);

    // Each key only in its own section; a bad value keeps the default.
    Config bad = ConfigParse(R"({"game": {"sound": "mono", "vibration": false, "map": 4,
        "message_speed": "slow"}, "audio": {"clock": false}})");
    ASSERT_TRUE(bad.options == ConfigGameOptions{});

    Config config;
    config.options = options;
    ASSERT_TRUE(ConfigParse(ConfigSerialize(config)).options == options);
    ASSERT_EQ(ConfigSerialize(config).find("\"options\""), std::string::npos);
    ASSERT_TRUE(ConfigParse(R"({"options": {"clock": false, "sound": "mono"}})").options == defaults);
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

namespace {

std::vector<std::pair<Config, Config>> g_changes;

void RecordChange(const Config &before, const Config &after) {
    g_changes.emplace_back(before, after);
}

bool g_nested = true;

// Changes the settings again and removes itself, both from inside a change.
void ChangeAgain(const Config &before, const Config &after) {
    Config again = after;
    again.master_volume = 0.75f;
    g_nested = ConfigChange(again);
    ConfigRemoveChangeHook(ChangeAgain);
}

} // namespace

TEST(PlatformConfig, ChangeAppliesAndSaves) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("dc_config_change_" + std::to_string(dc::test::ProcessId()));

    struct Cleanup {
        std::filesystem::path root;

        ~Cleanup() {
            ConfigRemoveChangeHook(RecordChange);
            ConfigRemoveChangeHook(ChangeAgain);
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    } cleanup{root};

    g_changes.clear();
    g_nested = true;
    std::filesystem::create_directories(root);
    PathsSetSaveRoot(root);
    std::ofstream(root / "config.json") << R"({"video": {"show_fps": false}})";
    ASSERT_TRUE(ConfigLoad());
    ConfigAddChangeHook(RecordChange);
    ConfigAddChangeHook(RecordChange);

    Config config = ConfigGet();
    config.master_volume = 0.25f;
    config.ui_scale = 10.0f;
    ASSERT_TRUE(ConfigChange(config));
    ASSERT_TRUE(g_changes.size() == 1);
    ASSERT_TRUE(g_changes[0].first.master_volume == 1.0f && !g_changes[0].first.show_fps);
    ASSERT_TRUE(g_changes[0].second.master_volume == 0.25f && g_changes[0].second.ui_scale == 1.0f);
    ASSERT_TRUE(ConfigGet() == g_changes[0].second);
    ASSERT_TRUE(!std::filesystem::exists(root / "config.json.tmp"));

    ASSERT_TRUE(ConfigChange(ConfigGet()));
    ASSERT_TRUE(g_changes.size() == 1);
    ASSERT_TRUE(ConfigLoad());
    ASSERT_TRUE(ConfigGet().master_volume == 0.25f && !ConfigGet().show_fps);

    ConfigRemoveChangeHook(RecordChange);
    ConfigAddChangeHook(ChangeAgain);
    ConfigAddChangeHook(RecordChange);
    std::filesystem::remove(root / "config.json");
    std::filesystem::create_directory(root / "config.json");
    config.master_volume = 0.5f;
    ASSERT_TRUE(!ConfigChange(config));
    ASSERT_TRUE(!g_nested);
    ASSERT_TRUE(g_changes.size() == 2 && g_changes[1].second.master_volume == 0.5f);
    ASSERT_TRUE(ConfigGet().master_volume == 0.5f);
    ASSERT_TRUE(!ConfigChange(config));
    ASSERT_TRUE(!ConfigLoad());
    ASSERT_TRUE(!ConfigChange(ConfigGet()));
    std::filesystem::remove(root / "config.json");
    ASSERT_TRUE(ConfigChange(ConfigGet()));
    ASSERT_TRUE(std::filesystem::is_regular_file(root / "config.json"));
    ASSERT_TRUE(ConfigChange(ConfigGet()));
    ASSERT_TRUE(g_changes.size() == 2);
}

TEST(PlatformConfig, Discord) {
    ASSERT_TRUE(ConfigParse("").discord_rich_presence);
    Config config = ConfigParse(R"({"discord": {"rich_presence": false}})");
    ASSERT_TRUE(!config.discord_rich_presence);
    ASSERT_TRUE(ConfigParse(R"({"discord": {"rich_presence": "no"}})").discord_rich_presence);
    ASSERT_TRUE(!ConfigParse(ConfigSerialize(config)).discord_rich_presence);
}

namespace {

// One-shot faults: SDL refuses a fullscreen change or a size, or takes a fullscreen change but has
// not made it when its wait ends.
constexpr unsigned kRefuseFullscreen = 1;
constexpr unsigned kRefuseSize = 2;
constexpr unsigned kUnfinished = 4;

constexpr WindowMode kFullscreenWindow{true, 1920, 1080};

// A window as WindowSetMode drives it.
struct FakeWindow {
    WindowMode mode;
    // The size it has once out of fullscreen.
    WindowMode windowed;
    unsigned   faults;
    bool       pending;
    bool       pending_fullscreen;
    bool       event;
    // WindowSetMode calls, and the window changes they tried.
    int sets;
    int changes;
};

FakeWindow g_fake;

bool Fault(unsigned fault) {
    bool hit = (g_fake.faults & fault) != 0;
    g_fake.faults &= ~fault;
    return hit;
}

WindowModeResult FakeSetMode(const WindowConfig &config) {
    ++g_fake.sets;
    bool             desktop = config.width <= 0 && config.height <= 0;
    WindowModeResult result;
    result.before = g_fake.mode;
    result.requested = {config.fullscreen || desktop, config.width, config.height};
    if (!(result.before == result.requested)) {
        if (g_fake.mode.fullscreen != result.requested.fullscreen) {
            ++g_fake.changes;
            if (!Fault(kRefuseFullscreen)) {
                // A request SDL takes replaces any it has not finished.
                result.fullscreen_request = result.requested.fullscreen;
                g_fake.pending = Fault(kUnfinished);
                g_fake.pending_fullscreen = result.requested.fullscreen;
                if (!g_fake.pending) {
                    g_fake.mode = result.requested.fullscreen ? kFullscreenWindow : g_fake.windowed;
                }
            }
        }
        if (!g_fake.mode.fullscreen && !result.requested.fullscreen) {
            ++g_fake.changes;
            if (!Fault(kRefuseSize)) {
                g_fake.mode = g_fake.windowed = result.requested;
            }
        }
    }
    result.observed = g_fake.mode;
    return result;
}

bool FakeCurrentMode(WindowMode &mode) {
    mode = g_fake.mode;
    return true;
}

bool FakeTakeEvent() {
    return std::exchange(g_fake.event, false);
}

} // namespace

// The display fields stay what the window is when SDL refuses a mode or finishes it late, through
// one more ConfigChange that asks nothing of the window, even with --width and --height; the warning
// stands, past the screen closing and opening again, until a change of mode takes, and the host's
// pump follows a late change while it or an unfinished fullscreen change stands, and no other.
TEST(PlatformConfig, DisplayFieldsFollowTheWindow) {
    struct Fields {
        bool fullscreen;
        int  width;
        int  height;

        bool operator==(const Fields &) const = default;
    };

    // Start sets the fields and the window to match (StartCli with --width 1024 --height 768); Ask
    // asks for fullscreen or not, and Size for 1024x768; Edit changes another setting; Close saves as
    // the screen does when it closes; Finish makes SDL finish, Drag resizes the window by hand and
    // Event changes nothing, each sending an event that Pump, the host's, takes.
    enum Kind {
        Start,
        StartCli,
        Ask,
        Size,
        Edit,
        Close,
        Finish,
        Drag,
        Event,
        Pump,
    };

    struct Step {
        Kind           kind;
        bool           fullscreen;
        unsigned       faults;
        DisplayWarning warning;
        Fields         fields;
        int            sets;
        int            changes;
    };

    using enum DisplayWarning;
    const Fields full{true, 1280, 720};
    const Fields left{false, 1600, 900};
    const Fields full_left{true, 1600, 900};
    const Fields full_small{true, 1024, 768};
    const Fields small{false, 1024, 768};
    const Step   steps[] = {
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kRefuseFullscreen, Kept,    full,       1, 1},
        {Edit,     false, 0,                 Kept,    full,       1, 1},
        // Out of fullscreen but not resized, then back to fullscreen.
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kRefuseSize,       Changed, left,       1, 2},
        {Ask,      true,  0,                 None,    full_left,  2, 3},
        // The same, but the way back refused.
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kRefuseSize,       Changed, left,       1, 2},
        {Ask,      true,  kRefuseFullscreen, Kept,    left,       2, 3},
        {Edit,     false, 0,                 Kept,    left,       2, 3},
        {Close,    false, 0,                 Kept,    left,       2, 3},
        // Leaving fullscreen finishes after the screen closed, and the host's pump follows it once;
        // the screen opens again with the warning standing until a change takes.
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kUnfinished,       Kept,    full,       1, 1},
        {Close,    false, 0,                 Kept,    full,       1, 1},
        {Finish,   false, 0,                 Kept,    full,       1, 1},
        {Pump,     false, 0,                 Changed, left,       1, 1},
        {Pump,     false, 0,                 Changed, left,       1, 1},
        {Edit,     false, 0,                 Changed, left,       1, 1},
        {Ask,      true,  0,                 None,    full_left,  2, 2},
        // Entering fullscreen finishes late: the mode asked for after all. A resize by hand with no
        // warning standing leaves the fields alone.
        {Start,    false, 0,                 None,    left,       0, 0},
        {Ask,      true,  kUnfinished,       Kept,    left,       1, 1},
        {Finish,   false, 0,                 Kept,    left,       1, 1},
        {Pump,     false, 0,                 None,    full_left,  1, 1},
        {Drag,     false, 0,                 None,    full_left,  1, 1},
        {Pump,     false, 0,                 None,    full_left,  1, 1},
        // --width and --height: the size asked is refused, and the fields take the window's without
        // the overrides being asked of it again.
        {StartCli, true,  0,                 None,    full,       0, 0},
        {Ask,      false, kRefuseSize,       Changed, left,       1, 2},
        // An unfinished exit outlives a size kept for the window while fullscreen, which asks
        // nothing of it, then a second request, unfinished in turn, that finishes to the mode asked.
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kUnfinished,       Kept,    full,       1, 1},
        {Size,     false, 0,                 None,    full_small, 2, 1},
        {Close,    false, 0,                 None,    full_small, 2, 1},
        {Finish,   false, 0,                 None,    full_small, 2, 1},
        {Pump,     false, 0,                 Changed, left,       2, 1},
        {Ask,      true,  kUnfinished,       Kept,    left,       3, 2},
        {Finish,   false, 0,                 Kept,    left,       3, 2},
        {Pump,     false, 0,                 None,    full_left,  3, 2},
        // An unfinished entry outlives a resize that takes.
        {Start,    false, 0,                 None,    left,       0, 0},
        {Ask,      true,  kUnfinished,       Kept,    left,       1, 1},
        {Size,     false, 0,                 None,    small,      2, 2},
        {Close,    false, 0,                 None,    small,      2, 2},
        {Finish,   false, 0,                 None,    small,      2, 2},
        {Pump,     false, 0,                 Changed, full_small, 2, 2},
        // An exit asked again while the first is unfinished replaces it; one that never finishes
        // leaves the warning standing through events that change nothing.
        {Start,    true,  0,                 None,    full,       0, 0},
        {Ask,      false, kUnfinished,       Kept,    full,       1, 1},
        {Size,     false, 0,                 None,    full_small, 2, 1},
        {Ask,      false, kUnfinished,       Kept,    full_small, 3, 2},
        {Event,    false, 0,                 Kept,    full_small, 3, 2},
        {Pump,     false, 0,                 Kept,    full_small, 3, 2},
        {Finish,   false, 0,                 Kept,    full_small, 3, 2},
        {Pump,     false, 0,                 Changed, left,       3, 2},
    };

    std::filesystem::path root = std::filesystem::temp_directory_path() / ("dc_display_" + std::to_string(dc::test::ProcessId()));
    std::filesystem::create_directories(root);
    PathsSetSaveRoot(root);
    std::ofstream(root / "config.json") << "{}";
    ASSERT_TRUE(ConfigLoad());
    DisplayWindow fake;
    fake.set_mode = FakeSetMode;
    fake.current_mode = FakeCurrentMode;
    fake.take_event = FakeTakeEvent;
    for (int i = 0; i < static_cast<int>(std::size(steps)); ++i) {
        SCOPED_TRACE(i);
        const Step &step = steps[i];
        Config      config = ConfigGet();
        g_fake.faults = step.faults;
        if (step.kind == Start || step.kind == StartCli) {
            ConfigRemoveChangeHook(DisplayChanged);
            config.fullscreen = step.fields.fullscreen;
            config.window_width = step.fields.width;
            config.window_height = step.fields.height;
            ASSERT_TRUE(ConfigChange(config));
            g_fake = FakeWindow{};
            g_fake.windowed = {false, 1600, 900};
            g_fake.mode = step.fullscreen ? kFullscreenWindow : g_fake.windowed;
            DisplayUseWindow(fake);
            DisplaySetOverrides(step.kind == StartCli ? 1024 : 0, step.kind == StartCli ? 768 : 0, false);
            ConfigAddChangeHook(DisplayChanged);
        } else if (step.kind == Ask || step.kind == Size) {
            if (step.kind == Ask) {
                config.fullscreen = step.fullscreen;
            } else {
                config.window_width = 1024;
                config.window_height = 768;
            }
            ASSERT_TRUE(DisplayApply(config));
        } else if (step.kind == Edit) {
            config.options.clock = !config.options.clock;
            ASSERT_TRUE(DisplayApply(config));
        } else if (step.kind == Close) {
            ASSERT_TRUE(ConfigChange(config));
        } else if (step.kind == Finish) {
            ASSERT_TRUE(g_fake.pending);
            g_fake.mode = g_fake.pending_fullscreen ? kFullscreenWindow : g_fake.windowed;
            g_fake.pending = false;
            g_fake.event = true;
        } else if (step.kind == Drag) {
            g_fake.mode = g_fake.windowed = {false, 800, 600};
            g_fake.event = true;
        } else if (step.kind == Event) {
            g_fake.event = true;
        } else {
            DisplayPump();
            EXPECT_EQ(DisplayTakePumpSave().has_value(), steps[i - 1].kind == Finish);
        }
        std::ifstream file(root / "config.json");
        Config        saved = ConfigParse(std::string(std::istreambuf_iterator<char>(file), {}));
        const Config &live = ConfigGet();
        EXPECT_EQ(DisplayGetWarning(), step.warning);
        EXPECT_TRUE((Fields{live.fullscreen, live.window_width, live.window_height} == step.fields));
        EXPECT_TRUE((Fields{saved.fullscreen, saved.window_width, saved.window_height} == step.fields));
        if (step.warning == Changed) {
            EXPECT_TRUE((DisplayShownMode() == WindowMode{step.fields.fullscreen, step.fields.width, step.fields.height}));
        }
        EXPECT_EQ(g_fake.sets, step.sets);
        EXPECT_EQ(g_fake.changes, step.changes);
    }
    ConfigRemoveChangeHook(DisplayChanged);
    DisplaySetOverrides(0, 0, false);
    DisplayUseWindow({});
    std::error_code error;
    std::filesystem::remove_all(root, error);
}

TEST(PlatformConfig, QteAlwaysWin) {
    ASSERT_TRUE(!ConfigParse("").qte_always_win);
    Config config = ConfigParse(R"({"game": {"qte_always_win": true}})");
    ASSERT_TRUE(config.qte_always_win);
    ASSERT_TRUE(ConfigParse(ConfigSerialize(config)).qte_always_win);
    ASSERT_TRUE(!ConfigParse(R"({"game": {"qte_always_win": "yes"}})").qte_always_win);
}
