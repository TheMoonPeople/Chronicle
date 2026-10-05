#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class ConfigPresentMode {
    Fifo,
    Mailbox,
    Immediate,
};

enum class ConfigAspect {
    Auto,
    FourThree,
};

struct ConfigKeyBinding {
    std::string              action;
    std::vector<std::string> keys;
};

struct Config {
    double                        tick_rate = 50.0;
    bool                          debug_mode = true;
    ConfigPresentMode             present_mode = ConfigPresentMode::Fifo;
    bool                          interpolation = true;
    double                        max_fps = 0.0;
    // 0: the monitor's (WindowConfig).
    int                           window_width = 0;
    int                           window_height = 0;
    bool                          fullscreen = false;
    ConfigAspect                  aspect = ConfigAspect::Auto;
    float                         ui_scale = 1.0f;
    bool                          show_fps = true;
    float                         detail_distance = 0.0f;
    float                         shadow_distance = 0.0f;
    float                         master_volume = 1.0f;
    std::vector<ConfigKeyBinding> key_bindings;
    float                         mouse_sensitivity = 0.1f;
    float                         stick_sensitivity = 1.33f;
    bool                          mouse_invert_y = false;
    bool                          mouse_capture = true;
    std::vector<std::string>      mouse_release_keys = {"Escape"};
};

const Config &ConfigGet();

// How far from the player or the eye town houses, villagers and dungeon monsters keep their full
// detail: video.detail_distance, or infinity where it is 0. Past it the game's own distances apply.
float ConfigDetailDistance();

// How far from the eye a town part casts its shadow at full strength: video.shadow_distance, or
// infinity where it is 0. Past it the game's own distances apply.
float ConfigShadowDistance();

// Parses a JSON text (comments allowed) over the defaults: an object of the sections game, video,
// audio and input. Unknown keys and bad values are reported on stderr and leave the default in
// place, as does a text that is not JSON; an empty text is the defaults.
Config ConfigParse(std::string_view text);

// The JSON text ConfigParse reads back as the same settings.
std::string ConfigSerialize(const Config &config);

// Writes the current settings to <save root>/config.json, creating the save root if need be, and
// says on stderr where it saved, or that it could not. Returns whether the file was written.
bool ConfigSave();

// Loads <save root>/config.json and says on stderr where it loaded from. With no file there, keeps
// the defaults and saves them as a new config.json. Returns whether a file was read.
bool ConfigLoad();
