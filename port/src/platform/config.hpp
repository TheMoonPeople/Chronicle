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

// What the FPS counter shows: the frame rate alone, with the logic ticks, or all of it with the draws.
enum class ConfigFpsDetail {
    Fps,
    Ticks,
    All,
};

enum class ConfigAspect {
    Auto,
    FourThree,
};

enum class ConfigGyro {
    Off,
    Always,
    FirstPerson,
    Held,
};

struct ConfigKeyBinding {
    std::string              action;
    std::vector<std::string> keys;

    bool operator==(const ConfigKeyBinding &) const = default;
};

// The game's own Options screen settings, which retail keeps in each save: here they hold for every
// save, and the save's copy follows them. config.json keeps each in the section of the screen's page
// that shows it: audio.sound, input.vibration, video.soft_focus, the rest under game.
struct ConfigGameOptions {
    bool save_cursor_position = true;
    bool vibration = true;
    bool fast_messages = false;
    bool stereo = true;
    bool clock = true;
    bool fast_time = false;
    // The dungeon map's density, 1 to 3; 0 hides it.
    int  map = 2;
    bool enemy_damage = true;
    bool player_damage = true;
    bool enemy_hp = true;
    bool names = true;
    bool soft_focus = true;

    bool operator==(const ConfigGameOptions &) const = default;
};

struct Config {
    double            tick_rate = 60.0;
    bool              debug_mode = false;
    bool              qte_always_win = false;
    ConfigPresentMode present_mode = ConfigPresentMode::Fifo;
    bool              interpolation = true;
    double            max_fps = 0.0;
    // 0: the monitor's (WindowConfig).
    int                           window_width = 0;
    int                           window_height = 0;
    bool                          fullscreen = false;
    ConfigAspect                  aspect = ConfigAspect::Auto;
    float                         ui_scale = 1.0f;
    bool                          show_fps = true;
    ConfigFpsDetail               fps_detail = ConfigFpsDetail::All;
    float                         detail_distance = 0.0f;
    float                         shadow_distance = 0.0f;
    float                         master_volume = 1.0f;
    std::vector<ConfigKeyBinding> key_bindings;
    float                         mouse_sensitivity = 0.2f;
    float                         stick_sensitivity = 1.33f;
    bool                          stick_invert_x = false;
    bool                          stick_invert_y = false;
    ConfigGyro                    gyro = ConfigGyro::Held;
    float                         gyro_sensitivity = 0.5f;
    bool                          gyro_invert_x = false;
    bool                          gyro_invert_y = false;
    bool                          mouse_invert_y = false;
    bool                          mouse_capture = true;
    bool                          mouse_zoom = false;
    std::vector<std::string>      mouse_release_keys = {"Escape"};
    ConfigGameOptions             options;
    bool                          discord_rich_presence = true;
    // game.language: 0 asks at start-up, as retail does; 2 to 6 is the language to start in
    // (LanguageCode: English, Francais, Deutsch, Italiano, Espanol) and skips the language screen.
    int                           language = 0;

    bool operator==(const Config &) const = default;
};

const Config &ConfigGet();

// How far from the player or the eye town houses, villagers and dungeon monsters keep their full
// detail: video.detail_distance, or infinity where it is 0. Past it the game's own distances apply.
float ConfigDetailDistance();

// How far from the eye a town part casts its shadow at full strength: video.shadow_distance, or
// infinity where it is 0. Past it the game's own distances apply.
float ConfigShadowDistance();

// Parses a JSON text (comments allowed) over the defaults: an object of the sections game, video,
// audio, input and discord. Unknown keys and bad values are reported on stderr and leave the default in
// place, as does a text that is not JSON; an empty text is the defaults.
Config ConfigParse(std::string_view text);

// The JSON text ConfigParse reads back as the same settings.
std::string ConfigSerialize(const Config &config);

// Writes the current settings to <save root>/config.json, creating the save root if need be, and
// says on stderr where it saved, or that it could not. Returns whether the file was written. The
// text goes to config.json.tmp first and is renamed over config.json, so a crash leaves the old
// file or the new one, never a part.
bool ConfigSave();

// Loads <save root>/config.json and says on stderr where it loaded from. With no file there, keeps
// the defaults and saves them as a new config.json. Returns whether a file was read.
bool ConfigLoad();

// Applies a change of settings to the running game; before is what ConfigChange replaced. Each part
// of the game that holds a setting adds its own and looks only at its own keys.
using ConfigChangeHook = void (*)(const Config &before, const Config &after);

// Adding a hook twice runs it once. Hooks added or removed while a change is being applied take part
// from the next change.
void ConfigAddChangeHook(ConfigChangeHook hook);

void ConfigRemoveChangeHook(ConfigChangeHook hook);

// What a settings screen calls with the settings it edited. They are taken as config.json would read
// them back (a bad value is reported and becomes its default), become ConfigGet(), are saved (comments
// in the file are not kept), and reach the running game through the change hooks, in the order they
// were added; ConfigAppliesOnRestart names the settings that wait for the next start instead. Settings
// equal to the current ones change nothing, but are saved if the last save failed. Returns whether the
// settings are saved. A call from a change hook is refused and returns false. Main thread only, as are
// the hooks.
bool ConfigChange(const Config &config);

// Whether a change to the setting, by its config.json name ("game.debug_mode"), reaches the running
// game only at the next start.
bool ConfigAppliesOnRestart(std::string_view key);
