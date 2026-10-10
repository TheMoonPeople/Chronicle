#include "options/rows.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "localize.hpp"
#include "menu_option.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"

namespace options {

namespace {

constexpr int kMapOff = 3;

int Two(const Config &) {
    return 2;
}

// A game option whose first choice is First.
template <bool ConfigGameOptions::*Field, bool First>
int GetOption(const Config &config) {
    return config.options.*Field == First ? 0 : 1;
}

template <bool ConfigGameOptions::*Field, bool First>
void SetOption(Config &config, int choice) {
    config.options.*Field = choice == 0 ? First : !First;
}

template <bool ConfigGameOptions::*Field, bool First>
Row GameRow(const char *key, const char *label, const char *names, int game_help) {
    return {.key = key,
            .label = label,
            .game_help = game_help,
            .count = Two,
            .get = GetOption<Field, First>,
            .set = SetOption<Field, First>,
            .names = names};
}

template <bool Config::*Field>
int GetOnOff(const Config &config) {
    return config.*Field ? 0 : 1;
}

template <bool Config::*Field>
void SetOnOff(Config &config, int choice) {
    config.*Field = choice == 0;
}

template <bool Config::*Field>
Row OnOffRow(const char *key, const char *label, const char *help) {
    return {.key = key,
            .label = label,
            .help = help,
            .count = Two,
            .get = GetOnOff<Field>,
            .set = SetOnOff<Field>,
            .names = "On|Off"};
}

template <bool Config::*Field>
Row NamedRow(const char *key, const char *label, const char *help, const char *names) {
    return {.key = key,
            .label = label,
            .help = help,
            .count = Two,
            .get = [](const Config &config) { return config.*Field ? 1 : 0; },
            .set = [](Config &config, int choice) { config.*Field = choice == 1; },
            .names = names};
}

Row SettingRow(const char *key, const char *label, const char *help, int (*count)(const Config &),
               int (*get)(const Config &), void (*set)(Config &, int), std::string (*text)(const Config &),
               const char *names = nullptr, void (*restore)(Config &, const Config &) = nullptr) {
    return {.key = key,
            .label = label,
            .help = help,
            .count = count,
            .get = get,
            .set = set,
            .text = text,
            .names = names,
            .restore = restore};
}

void QuitGame() {
    SDL_Event event{};
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

std::string ChoiceName(const char *names, int choice) {
    std::string_view rest = names;
    for (int i = 0; i < choice; ++i) {
        rest = rest.substr(rest.find('|') + 1);
    }
    return std::string(rest.substr(0, rest.find('|')));
}

int Nearest(std::span<const double> choices, double value) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(choices.size()); ++i) {
        if (std::abs(choices[i] - value) < std::abs(choices[best] - value)) {
            best = i;
        }
    }
    return best;
}

std::string Hundredths(float value) {
    return value > 10.0f ? std::format("{:.2g}", value) : std::format("{:.2f}", value);
}

constexpr OptionResolution kWindowSizes[] = {
    {1024, 768 },
    {1280, 720 },
    {1280, 960 },
    {1366, 768 },
    {1440, 1080},
    {1600, 900 },
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
};

std::vector<OptionResolution> g_sizes;

bool Desktop(const Config &config) {
    return config.window_width == 0 && config.window_height == 0;
}

int MapCount(const Config &) {
    return 4;
}

int MapChoice(const Config &config) {
    return config.options.map == 0 ? kMapOff : config.options.map - 1;
}

void SetMap(Config &config, int choice) {
    config.options.map = choice == kMapOff ? 0 : choice + 1;
}

int ResolutionCount(const Config &) {
    return static_cast<int>(g_sizes.size());
}

int ResolutionChoice(const Config &config) {
    double area = static_cast<double>(config.window_width) * config.window_height;
    int    best = 0;
    for (int i = 0; i < static_cast<int>(g_sizes.size()); ++i) {
        const OptionResolution &size = g_sizes[i];
        if (size.width == config.window_width && size.height == config.window_height) {
            return i;
        }
        double here = static_cast<double>(size.width) * size.height;
        double there = static_cast<double>(g_sizes[best].width) * g_sizes[best].height;
        if (std::abs(here - area) < std::abs(there - area)) {
            best = i;
        }
    }
    return best;
}

void SetResolution(Config &config, int choice) {
    config.window_width = g_sizes[choice].width;
    config.window_height = g_sizes[choice].height;
}

std::string ResolutionText(const Config &config) {
    return Desktop(config) ? "Desktop" : std::format("{} x {}", config.window_width, config.window_height);
}

int Fullscreen(const Config &config) {
    return config.fullscreen || Desktop(config) ? 1 : 0;
}

// A window left from Desktop takes the largest size that fits beside the desktop's panels.
void SetFullscreen(Config &config, int choice) {
    config.fullscreen = choice == 1;
    if (choice == 1 || !Desktop(config)) {
        return;
    }
    int              width = 0;
    int              height = 0;
    bool             known = WindowDisplaySize(width, height, true);
    OptionResolution window = known ? kWindowSizes[0] : OptionResolution{1280, 960};
    for (const OptionResolution &size : kWindowSizes) {
        if (known && size.width <= width && size.height <= height &&
            size.width * size.height > window.width * window.height) {
            window = size;
        }
    }
    config.window_width = window.width;
    config.window_height = window.height;
}

void RestoreFullscreen(Config &config, const Config &defaults) {
    config.fullscreen = defaults.fullscreen;
}

constexpr ConfigPresentMode kPresentModes[] = {ConfigPresentMode::Fifo, ConfigPresentMode::Mailbox,
                                               ConfigPresentMode::Immediate};

int PresentModeCount(const Config &) {
    return static_cast<int>(std::size(kPresentModes));
}

int PresentModeChoice(const Config &config) {
    return static_cast<int>(std::ranges::find(kPresentModes, config.present_mode) - std::begin(kPresentModes));
}

void SetPresentMode(Config &config, int choice) {
    config.present_mode = kPresentModes[choice];
}

constexpr ConfigFpsDetail kFpsDetails[] = {ConfigFpsDetail::Fps, ConfigFpsDetail::Ticks, ConfigFpsDetail::All};

int FpsDetailCount(const Config &) {
    return static_cast<int>(std::size(kFpsDetails));
}

int FpsDetailChoice(const Config &config) {
    return static_cast<int>(std::ranges::find(kFpsDetails, config.fps_detail) - std::begin(kFpsDetails));
}

void SetFpsDetail(Config &config, int choice) {
    config.fps_detail = kFpsDetails[choice];
}

constexpr double kMaxFps[] = {0, 30, 60, 75, 90, 120, 144, 165, 240};

int MaxFpsCount(const Config &) {
    return static_cast<int>(std::size(kMaxFps));
}

int MaxFpsChoice(const Config &config) {
    return Nearest(kMaxFps, config.max_fps);
}

void SetMaxFps(Config &config, int choice) {
    config.max_fps = kMaxFps[choice];
}

std::string MaxFpsText(const Config &config) {
    return config.max_fps > 0.0 ? std::format("{}", config.max_fps) : "Unlimited";
}

constexpr int kAnisotropy[] = {0, 2, 4, 8, 16};

int AnisotropyCount(const Config &) {
    return static_cast<int>(std::size(kAnisotropy));
}

int AnisotropyChoice(const Config &config) {
    return std::max(0, static_cast<int>(std::ranges::find(kAnisotropy, config.anisotropy) - std::begin(kAnisotropy)));
}

void SetAnisotropy(Config &config, int choice) {
    config.anisotropy = kAnisotropy[choice];
}

int Aspect(const Config &config) {
    return config.aspect == ConfigAspect::FourThree ? 1 : 0;
}

void SetAspect(Config &config, int choice) {
    config.aspect = choice == 1 ? ConfigAspect::FourThree : ConfigAspect::Auto;
}

// 50% to 200% in tenths.
int UiScaleCount(const Config &) {
    return 16;
}

int UiScaleChoice(const Config &config) {
    return std::clamp(static_cast<int>(std::lround(config.ui_scale * 10.0f)) - 5, 0, 15);
}

void SetUiScale(Config &config, int choice) {
    config.ui_scale = (choice + 5) / 10.0f;
}

std::string UiScaleText(const Config &config) {
    return std::format("{:.0f}%", config.ui_scale * 100.0f);
}

// 0% to 100% in fives.
int VolumeCount(const Config &) {
    return 21;
}

int VolumeChoice(const Config &config) {
    return static_cast<int>(std::lround(config.master_volume * 20.0f));
}

void SetVolume(Config &config, int choice) {
    config.master_volume = choice / 20.0f;
}

std::string VolumeText(const Config &config) {
    return std::format("{:.0f}%", config.master_volume * 100.0f);
}

// Hundredths up to 0.99, then tenths up to 10.
int MouseSensitivityCount(const Config &) {
    return 190;
}

int MouseSensitivityChoice(const Config &config) {
    float value = std::clamp(config.mouse_sensitivity, 0.01f, 10.0f);
    if (value < 0.995f) {
        return std::clamp(static_cast<int>(std::lround(value * 100.0f)) - 1, 0, 98);
    }
    return std::clamp(99 + static_cast<int>(std::lround((value - 1.0f) * 10.0f)), 99, 189);
}

void SetMouseSensitivity(Config &config, int choice) {
    config.mouse_sensitivity = choice < 99 ? (choice + 1) / 100.0f : 1.0f + (choice - 99) / 10.0f;
}

std::string MouseSensitivityText(const Config &config) {
    return Hundredths(config.mouse_sensitivity);
}

void RestoreMouseSensitivity(Config &config, const Config &defaults) {
    config.mouse_sensitivity = defaults.mouse_sensitivity;
}

constexpr float kCameraReturnRates[] = {0.0f, 0.05f, 0.2f, 0.5f, 1.0f};
constexpr const char *kCameraReturnNames[] = {"Off", "Very Slow", "Slow", "Moderate", "Retail"};

int CameraReturnChoice(const Config &config) {
    for (int choice = 0; choice < 5; ++choice) {
        if (config.mouse_camera_return == kCameraReturnRates[choice]) {
            return choice;
        }
    }
    return 5;
}

int CameraReturnCount(const Config &config) {
    return CameraReturnChoice(config) == 5 ? 6 : 5;
}

void SetCameraReturn(Config &config, int choice) {
    if (choice >= 0 && choice < 5) {
        config.mouse_camera_return = kCameraReturnRates[choice];
    }
}

std::string CameraReturnText(const Config &config) {
    int choice = CameraReturnChoice(config);
    return choice < 5 ? kCameraReturnNames[choice] : std::format("{:.4g}%", config.mouse_camera_return * 100.0f);
}

void RestoreCameraReturn(Config &config, const Config &defaults) {
    config.mouse_camera_return = defaults.mouse_camera_return;
}

constexpr const char *kZoomResetBindings[] = {"Mouse3", "Mouse4", "Mouse5", "Home", ""};
constexpr const char *kZoomResetNames[] = {"Middle Mouse", "Mouse4", "Mouse5", "Home", "Disabled"};

const std::vector<std::string> &ZoomResetBindings(const Config &config) {
    static const std::vector<std::string> defaults = {"Mouse3"};
    auto                                  binding = std::ranges::find(config.key_bindings, "zoom_reset", &ConfigKeyBinding::action);
    return binding == config.key_bindings.end() ? defaults : binding->keys;
}

int ZoomResetChoice(const Config &config) {
    const auto &keys = ZoomResetBindings(config);
    if (keys.empty()) {
        return 4;
    }
    for (int choice = 0; choice < 4; ++choice) {
        if (keys.size() == 1 && keys.front() == kZoomResetBindings[choice]) {
            return choice;
        }
    }
    return 5; // The file's custom binding remains a selectable choice until explicitly changed.
}

int ZoomResetCount(const Config &config) {
    return ZoomResetChoice(config) == 5 ? 6 : 5;
}

void SetZoomReset(Config &config, int choice) {
    if (choice < 0 || choice >= 5) {
        return;
    }
    auto                     binding = std::ranges::find(config.key_bindings, "zoom_reset", &ConfigKeyBinding::action);
    std::vector<std::string> keys;
    if (choice < 4) {
        keys.push_back(kZoomResetBindings[choice]);
    }
    if (binding == config.key_bindings.end()) {
        config.key_bindings.push_back({"zoom_reset", std::move(keys)});
    } else {
        binding->keys = std::move(keys);
    }
}

std::string ZoomResetText(const Config &config) {
    int choice = ZoomResetChoice(config);
    if (choice < 5) {
        return kZoomResetNames[choice];
    }
    std::string text;
    for (const auto &key : ZoomResetBindings(config)) {
        if (!text.empty()) {
            text += ", ";
        }
        text += key;
    }
    return text;
}

void RestoreZoomReset(Config &config, const Config &defaults) {
    auto binding = std::ranges::find(config.key_bindings, "zoom_reset", &ConfigKeyBinding::action);
    if (binding != config.key_bindings.end()) {
        config.key_bindings.erase(binding);
    }
    auto default_binding = std::ranges::find(defaults.key_bindings, "zoom_reset", &ConfigKeyBinding::action);
    if (default_binding != defaults.key_bindings.end()) {
        config.key_bindings.push_back(*default_binding);
    }
}

// 0.50 to 2.50 in twentieths.
int StickSensitivityCount(const Config &) {
    return 41;
}

int StickSensitivityChoice(const Config &config) {
    return static_cast<int>(std::lround((std::clamp(config.stick_sensitivity, 0.5f, 2.5f) - 0.5f) * 20.0f));
}

void SetStickSensitivity(Config &config, int choice) {
    config.stick_sensitivity = 0.5f + choice / 20.0f;
}

std::string StickSensitivityText(const Config &config) {
    return Hundredths(config.stick_sensitivity);
}

void RestoreStickSensitivity(Config &config, const Config &defaults) {
    config.stick_sensitivity = defaults.stick_sensitivity;
}

int GyroCount(const Config &) {
    return 4;
}

int GyroChoice(const Config &config) {
    return static_cast<int>(config.gyro);
}

void SetGyro(Config &config, int choice) {
    config.gyro = static_cast<ConfigGyro>(choice);
}

int GlyphDeviceCount(const Config &) {
    return 10;
}

int GlyphDeviceChoice(const Config &config) {
    return static_cast<int>(config.glyph_device);
}

void SetGlyphDevice(Config &config, int choice) {
    config.glyph_device = static_cast<ConfigGlyphDevice>(choice);
}

// 0.10 to 2.00 in twentieths.
int GyroSensitivityCount(const Config &) {
    return 39;
}

int GyroSensitivityChoice(const Config &config) {
    return static_cast<int>(std::lround((std::clamp(config.gyro_sensitivity, 0.1f, 2.0f) - 0.1f) * 20.0f));
}

void SetGyroSensitivity(Config &config, int choice) {
    config.gyro_sensitivity = 0.1f + choice / 20.0f;
}

std::string GyroSensitivityText(const Config &config) {
    return Hundredths(config.gyro_sensitivity);
}

void RestoreGyroSensitivity(Config &config, const Config &defaults) {
    config.gyro_sensitivity = defaults.gyro_sensitivity;
}

// Ask, then the five languages of the language screen (LanguageCode 2 to 6).
int LanguageChoice(const Config &config) {
    return config.language >= 2 && config.language <= 6 ? config.language - 1 : 0;
}

void SetLanguage(Config &config, int choice) {
    config.language = choice == 0 ? 0 : choice + 1;
}

int LanguageCount(const Config &) {
    return 6;
}

int ShadowCount(const Config &) {
    return 21;
}

int TextShadowChoice(const Config &config) {
    return std::clamp(config.text_shadow / 5, 0, 20);
}

void SetTextShadow(Config &config, int choice) {
    config.text_shadow = choice * 5;
}

std::string TextShadowText(const Config &config) {
    return std::format("{}%", config.text_shadow);
}

int GlyphShadowChoice(const Config &config) {
    return std::clamp(config.glyph_shadow / 5, 0, 20);
}

void SetGlyphShadow(Config &config, int choice) {
    config.glyph_shadow = choice * 5;
}

std::string GlyphShadowText(const Config &config) {
    return std::format("{}%", config.glyph_shadow);
}

int Three(const Config &) {
    return 3;
}

int SoundChoice(const Config &config) {
    return config.surround ? 2 : config.options.stereo ? 1 : 0;
}

void SetSound(Config &config, int choice) {
    config.options.stereo = choice != 0;
    config.surround = choice == 2;
}

template <int Config::*Member>
int PictureShadowChoice(const Config &config) {
    return std::clamp(config.*Member / 5, 0, 20);
}

template <int Config::*Member>
void SetPictureShadow(Config &config, int choice) {
    config.*Member = choice * 5;
}

template <int Config::*Member>
std::string PictureShadowText(const Config &config) {
    return std::format("{}%", config.*Member);
}

const Row kGameRows[] = {
    GameRow<&ConfigGameOptions::save_cursor_position, true>("game.save_cursor_position", "Save Cursor Position",
                                                            "On|Off", 0x15E),
    GameRow<&ConfigGameOptions::clock, true>("game.clock", "Clock", "On|Off", 0x162),
    GameRow<&ConfigGameOptions::fast_time, false>("game.time_speed", "Time Speed", "Normal|Fast", 0x163),
    Row{.key = "game.map",
        .label = "Dungeon Map",
        .game_help = 0x164,
        .count = MapCount,
        .get = MapChoice,
        .set = SetMap,
        .names = "1|2|3|Off"},
    GameRow<&ConfigGameOptions::enemy_damage, true>("game.enemy_damage", "Enemy Damage", "On|Off", 0x165),
    GameRow<&ConfigGameOptions::player_damage, true>("game.player_damage", "Party Damage", "On|Off", 0x166),
    GameRow<&ConfigGameOptions::enemy_hp, true>("game.enemy_hp", "Enemy HP", "On|Off", 0x167),
    GameRow<&ConfigGameOptions::names, true>("game.names", "Names", "On|Off", 0x168),
    OnOffRow<&Config::discord_rich_presence>("discord.rich_presence", "Enable Discord",
                                             "\"Discord Rich Presence\"\nShows what you are\nplaying on Discord."),
    OnOffRow<&Config::element_quick_select>("game.element_quick_select", "Element Quick Select",
                                            "\"Element Quick Select\"\nD-pad Up in a dungeon\npicks the element."),
    Row{.key = "quit_game", .label = "Quit Game", .help = "Exit the game and return to the desktop.",
        .activate = QuitGame},
};

const Row kDisplayRows[] = {
    SettingRow("video.fullscreen", "Window Mode", "\"Window Mode\"\nPlay in a window or\non the whole screen.", Two,
               Fullscreen, SetFullscreen, nullptr, "Windowed|Fullscreen", RestoreFullscreen),
    SettingRow("video.width", "Resolution",
               "\"Resolution\"\nThe window's size.\nFullscreen and Desktop\nuse the whole monitor.", ResolutionCount,
               ResolutionChoice, SetResolution, ResolutionText),
    SettingRow("video.present_mode", "V-Sync", "\"V-Sync\"\nOn: no tearing.\nFast: no tearing, less\ndelay. Off: may tear.",
               PresentModeCount, PresentModeChoice, SetPresentMode, nullptr, "On|Fast|Off"),
    SettingRow("video.max_fps", "Frame Limit", "\"Frame Limit\"\nThe most frames drawn\nin a second.", MaxFpsCount,
               MaxFpsChoice, SetMaxFps, MaxFpsText),
    SettingRow("video.aspect", "Aspect Ratio",
               "\"Aspect Ratio\"\nWide: the world fills\nthe window. 4:3: the\nPS2's picture.", Two, Aspect, SetAspect,
               nullptr, "Wide|4:3"),
    SettingRow("video.ui_scale", "Interface Size",
               "\"Interface Size\"\nThe size of the HUD\nand menus, from when\nOptions closes.", UiScaleCount,
               UiScaleChoice, SetUiScale, UiScaleText),
    OnOffRow<&Config::interpolation>("video.interpolation", "Smooth Motion",
                                     "\"Smooth Motion\"\nDraws frames between\nthe game's steps."),
    OnOffRow<&Config::show_fps>("video.show_fps", "FPS Counter", "\"FPS Counter\"\nShows the frame rate\nin the corner."),
    SettingRow("video.fps_detail", "FPS Info", "\"FPS Info\"\nWhat the counter shows:\nthe frame rate alone,\nwith ticks, or all.",
               FpsDetailCount, FpsDetailChoice, SetFpsDetail, nullptr, "FPS|FPS+Ticks|All"),
    SettingRow("video.anisotropy", "Anisotropic Filter",
               "\"Anisotropic Filter\"\nSharper textures on\nsurfaces seen at a\nslant.", AnisotropyCount,
               AnisotropyChoice, SetAnisotropy, nullptr, "Off|2x|4x|8x|16x"),
    GameRow<&ConfigGameOptions::soft_focus, true>("video.soft_focus", "Soft Focus", "On|Off", 0x169),
};

const Row kAudioRows[] = {
    SettingRow("audio.master_volume", "Volume", "\"Volume\"\nHow loud the game is.", VolumeCount, VolumeChoice,
               SetVolume, VolumeText),
    SettingRow("audio.sound", "Sound", "\"Sound\"\nMono, stereo, or\nsurround for 5.1\nspeakers.", Three, SoundChoice,
               SetSound, nullptr, "Mono|Stereo|Surround"),
    NamedRow<&Config::soundtrack>("audio.soundtrack", "Soundtrack",
                                  "\"Soundtrack\"\nPS2: the game's music.\nCustom: your own\nrecordings, from the\nnext song.",
                                  "PS2|Custom"),
};

const Row kControlRows[] = {
    GameRow<&ConfigGameOptions::vibration, true>("input.vibration", "Vibration", "On|Off", 0x15F),
    SettingRow("input.mouse_sensitivity", "Mouse Sensitivity", "\"Mouse Sensitivity\"\nHow fast the mouse\nturns the camera.",
               MouseSensitivityCount, MouseSensitivityChoice, SetMouseSensitivity, MouseSensitivityText, nullptr,
               RestoreMouseSensitivity),
    OnOffRow<&Config::mouse_invert_y>("input.mouse_invert_y", "Invert Mouse Y",
                                      "\"Invert Mouse Y\"\nMoving the mouse up\nlooks down."),
    SettingRow("input.mouse_camera_return", "Vertical Return",
               "\"Vertical Return\"\nHow quickly the camera\nreturns to normal height\nwhen the mouse stops.",
               CameraReturnCount, CameraReturnChoice, SetCameraReturn, CameraReturnText, nullptr, RestoreCameraReturn),
    OnOffRow<&Config::mouse_zoom>("input.mouse_zoom", "Mouse Wheel Zoom",
                                  "\"Mouse Wheel Zoom\"\nScroll to move closer\nor farther from your\ncharacter."),
    SettingRow("input.bindings.zoom_reset", "Reset Zoom", "\"Reset Zoom\"\nRestores the normal\ncamera distance.",
               ZoomResetCount, ZoomResetChoice, SetZoomReset, ZoomResetText, nullptr, RestoreZoomReset),
    SettingRow("input.stick_sensitivity", "Stick Sensitivity",
               "\"Stick Sensitivity\"\nHow far a gamepad's\nstick has to tilt.", StickSensitivityCount,
               StickSensitivityChoice, SetStickSensitivity, StickSensitivityText, nullptr, RestoreStickSensitivity),
    OnOffRow<&Config::stick_invert_x>("input.stick_invert_x", "Invert Stick X",
                                      "\"Invert Stick X\"\nFlips the camera's\nleft and right."),
    OnOffRow<&Config::stick_invert_y>("input.stick_invert_y", "Invert Stick Y",
                                      "\"Invert Stick Y\"\nFlips the camera's\nup and down."),
    SettingRow("input.gyro", "Gyro", "\"Gyro\"\nWhen tilting the pad\nturns the camera.", GyroCount, GyroChoice, SetGyro,
               nullptr, "Off|Always|First Person|While Held"),
    SettingRow("input.gyro_sensitivity", "Gyro Sensitivity", "\"Gyro Sensitivity\"\nHow fast tilting\nturns the camera.",
               GyroSensitivityCount, GyroSensitivityChoice, SetGyroSensitivity, GyroSensitivityText, nullptr,
               RestoreGyroSensitivity),
    OnOffRow<&Config::gyro_invert_x>("input.gyro_invert_x", "Invert Gyro X",
                                     "\"Invert Gyro X\"\nFlips the gyro's\nleft and right."),
    OnOffRow<&Config::gyro_invert_y>("input.gyro_invert_y", "Invert Gyro Y",
                                     "\"Invert Gyro Y\"\nFlips the gyro's\nup and down."),
};

const Row kAccessibilityRows[] = {
    OnOffRow<&Config::qte_always_win>("game.qte_always_win", "Always Win QTEs",
                                      "\"Always Win QTEs\"\nButton prompts always\nend in a perfect."),
};

const Row kTextRows[] = {
    Row{.key = "game.language",
        .label = "Language",
        .help = "\"Language\"\nThe language of the game;\nAsk shows the language\nscreen at start-up.",
        .count = LanguageCount,
        .get = LanguageChoice,
        .set = SetLanguage,
        .names = "Ask|English|Francais|Deutsch|Italiano|Espanol"},
    GameRow<&ConfigGameOptions::fast_messages, false>("game.message_speed", "Message Speed", "Normal|Fast", 0x160),
    NamedRow<&Config::glyphs_new>("input.glyphs", "Button Symbols",
                                  "\"Button Symbols\"\nNew: redrawn symbols\nfor your controller.\nOriginal: the PS2's.",
                                  "Original|New"),
    SettingRow("input.glyph_device", "Symbols Shown",
               "\"Symbols Shown\"\nAuto: the device you\nuse. Or always show one\nof the others.", GlyphDeviceCount,
               GlyphDeviceChoice, SetGlyphDevice, nullptr, "Auto|PS3|PS4|PS5|PS5 Colored|Xbox|Switch|Steam Deck|Steam Controller|Keyboard"),
    NamedRow<&Config::font_sharp>("video.text_font", "Text Font",
                                  "\"Text Font\"\nSharp: a clear font at\nthe screen's size.\nOriginal: the game's.",
                                  "Original|Sharp"),
    SettingRow("video.text_shadow", "Text Shadow",
               "\"Text Shadow\"\nHow dark the shadow\nunder the letters is.\n50% is the soft one.", ShadowCount,
               TextShadowChoice, SetTextShadow, TextShadowText),
    SettingRow("video.glyph_shadow", "Symbol Shadow",
               "\"Symbol Shadow\"\nHow dark the shadow\nunder the button\nsymbols is.", ShadowCount,
               GlyphShadowChoice, SetGlyphShadow, GlyphShadowText),
    SettingRow("video.name_shadow", "Area Name Shadow",
               "\"Area Name Shadow\"\nHow dark the shadow\nunder the area names\nis, from the next area.", ShadowCount,
               PictureShadowChoice<&Config::name_shadow>, SetPictureShadow<&Config::name_shadow>,
               PictureShadowText<&Config::name_shadow>),
    SettingRow("video.floor_shadow", "Floor Label Shadow",
               "\"Floor Label Shadow\"\nHow dark the shadow\nunder the dungeon floor\nlabels is.", ShadowCount,
               PictureShadowChoice<&Config::floor_shadow>, SetPictureShadow<&Config::floor_shadow>,
               PictureShadowText<&Config::floor_shadow>),
    SettingRow("video.boss_shadow", "Boss Name Shadow",
               "\"Boss Name Shadow\"\nHow dark the shadow\nunder the bosses'\nnames is.", ShadowCount,
               PictureShadowChoice<&Config::boss_shadow>, SetPictureShadow<&Config::boss_shadow>,
               PictureShadowText<&Config::boss_shadow>),
};

// Strings the binding rows point at. A deque never moves what it holds, so the c_str pointers the
// rows keep stay valid as more are added.
const char *Keep(std::string text) {
    static std::deque<std::string> store;
    return store.emplace_back(std::move(text)).c_str();
}

std::string ActionName(std::string_view action) {
    static constexpr struct {
        std::string_view action;
        const char      *name;
    } kNames[] = {
        {"up",             "Up"              },
        {"down",           "Down"            },
        {"left",           "Left"            },
        {"right",          "Right"           },
        {"cross",          "Cross"           },
        {"circle",         "Circle"          },
        {"square",         "Square"          },
        {"triangle",       "Triangle"        },
        {"l1",             "L1"              },
        {"r1",             "R1"              },
        {"l2",             "L2"              },
        {"r2",             "R2"              },
        {"l3",             "L3"              },
        {"r3",             "R3"              },
        {"start",          "Start"           },
        {"select",         "Select"          },
        {"lx-",            "Left Stick Left" },
        {"lx+",            "Left Stick Right"},
        {"ly-",            "Left Stick Up"   },
        {"ly+",            "Left Stick Down" },
        {"rx-",            "Right Stick Left"},
        {"rx+",            "Right Stick Right"},
        {"ry-",            "Right Stick Up"  },
        {"ry+",            "Right Stick Down"},
        {"fps_toggle",     "Toggle FPS"      },
        {"developer_menu", "Developer Menu"  },
        {"debug_menu",     "Debug Menu"      },
        {"gyro_hold",      "Gyro Hold"       },
        {"zoom_reset",     "Reset Zoom"      },
    };
    for (const auto &entry : kNames) {
        if (entry.action == action) {
            return entry.name;
        }
    }
    return std::string(action);
}

int One(const Config &) {
    return 1;
}

int Zero(const Config &) {
    return 0;
}

void NoopSet(Config &, int) {}

void RemoveBinding(Config &config, std::string_view action) {
    std::erase_if(config.key_bindings, [&](const ConfigKeyBinding &binding) {
        return std::string_view(binding.action) == action;
    });
}

// One row per action, in the order the game reads them. The whole-axis actions (lx ly rx ry) only
// take MouseX/MouseY, so a key cannot go on them, and Reset Zoom is edited by its own Controls row.
std::span<const Row> BindingRows() {
    static const std::vector<Row> rows = [] {
        std::vector<Row> list;
        for (const InputActionInfo &info : InputActions()) {
            if (info.whole_axis || info.action == "zoom_reset") {
                continue;
            }
            Row row;
            row.key = Keep("input.bindings." + std::string(info.action));
            row.label = Keep(ActionName(info.action));
            row.help = Keep("\"" + ActionName(info.action) + "\"\nPress confirm, then a\nkey or mouse button, to\nrebind it.");
            row.count = One;
            row.get = Zero;
            row.set = NoopSet;
            row.action = Keep(std::string(info.action));
            list.push_back(row);
        }
        return list;
    }();
    return rows;
}

} // namespace

std::span<const Page> Pages() {
    static const std::vector<Page> pages = {
        {"Game",          "\"Game\"\nHow the game plays.",                           kGameRows         },
        {"Display",       "\"Display\"\nThe window and picture.",                    kDisplayRows      },
        {"Audio",         "\"Audio\"\nSound and music.",                             kAudioRows        },
        {"Controls",      "\"Controls\"\nMouse, gamepad and gyro.",                  kControlRows      },
        {"Text",          "\"Text\"\nLanguage, lettering and\nsymbols.",             kTextRows         },
        {"Accessibility", "\"Accessibility\"\nHelp with harder parts\nof the game.", kAccessibilityRows},
        {"Bindings",      "\"Bindings\"\nWhat each key and\nmouse button does.",       BindingRows()     },
    };
    return pages;
}

// The Options screen's own text is looked up by key, in the language the game is in (localize.hpp); the
// English here is what it shows where the language has none. A setting's strings are options.<its
// config.json name>.label, .help and .choice.<n>, and a page's is options.page.<name>.
namespace {

// A value a row works out itself ("Desktop", "Unlimited") by its English: options.<name>.value.<English>.
std::string ValueText(const Row &row, const std::string &english) {
    return LocalizeText("options." + std::string(row.key) + ".value." + english, english);
}

std::string ChoiceText(const Row &row, int choice) {
    return LocalizeText("options." + std::string(row.key) + ".choice." + std::to_string(choice),
                        ChoiceName(row.names, choice));
}

} // namespace

std::string PageKey(const char *name) {
    std::string key = "options.page.";
    for (const char *c = name; *c != '\0'; ++c) {
        key += static_cast<char>(*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c);
    }
    return key;
}

std::vector<std::pair<std::string, std::string>> RowStrings() {
    std::vector<std::pair<std::string, std::string>> out;
    // Rows may share a key (the key-binding rows share one help); the same text twice is listed once, and two
    // texts under one key are both kept for the tests to find.
    auto add = [&](std::string key, std::string english) {
        for (const auto &[have_key, have_english] : out) {
            if (have_key == key && have_english == english) {
                return;
            }
        }
        out.emplace_back(std::move(key), std::move(english));
    };
    for (const Page &page : Pages()) {
        add(PageKey(page.name), page.name);
        add(PageKey(page.name) + ".help", page.help);
        for (const Row &row : page.rows) {
            const std::string prefix = "options." + std::string(row.key);
            add(prefix + ".label", row.label);
            if (row.help != nullptr) {
                add(prefix + ".help", row.help);
            }
            if (row.names != nullptr) {
                const int count = row.count(ConfigGet());
                for (int choice = 0; choice < count; ++choice) {
                    add(prefix + ".choice." + std::to_string(choice), ChoiceName(row.names, choice));
                }
            }
        }
    }
    add("options.video.width.value.Desktop", "Desktop");
    add("options.video.max_fps.value.Unlimited", "Unlimited");
    return out;
}

std::string RowValue(const Row &row, const Config &config) {
    if (row.action != nullptr) {
        return InputBindingLabel(row.action);
    }
    std::string value = row.text != nullptr ? ValueText(row, row.text(config)) : ChoiceText(row, row.get(config));
    if (ConfigAppliesOnRestart(row.key)) {
        value += " *";
    }
    return value;
}

bool StepRow(const Row &row, Config &config, int direction, bool wrap) {
    int count = row.count(config);
    int choice = row.get(config);
    int next = wrap ? (choice + direction + count) % count : std::clamp(choice + direction, 0, count - 1);
    if (next == choice) {
        return false;
    }
    row.set(config, next);
    return true;
}

void ResetPage(const Page &page, Config &config) {
    const Config defaults;
    for (const Row &row : page.rows) {
        if (row.action != nullptr) {
            RemoveBinding(config, row.action);
        } else if (row.restore != nullptr) {
            row.restore(config, defaults);
        } else {
            row.set(config, row.get(defaults));
        }
    }
}

void ListResolutions() {
    const Config &config = ConfigGet();
    int           width = 0;
    int           height = 0;
    bool          known = WindowDisplaySize(width, height, !config.fullscreen && !Desktop(config));
    g_sizes = OptionResolutionList(WindowDisplayModes(), config.window_width, config.window_height, known, width,
                                   height);
}

} // namespace options

int OptionZoomResetChoice(const Config &config) {
    return options::ZoomResetChoice(config);
}

int OptionZoomResetCount(const Config &config) {
    return options::ZoomResetCount(config);
}

std::string OptionZoomResetText(const Config &config) {
    return options::ZoomResetText(config);
}

void OptionSetZoomReset(Config &config, int choice) {
    options::SetZoomReset(config, choice);
}

void OptionRestoreZoomReset(Config &config, const Config &defaults) {
    options::RestoreZoomReset(config, defaults);
}

int OptionCameraReturnChoice(const Config &config) {
    return options::CameraReturnChoice(config);
}

int OptionCameraReturnCount(const Config &config) {
    return options::CameraReturnCount(config);
}

std::string OptionCameraReturnText(const Config &config) {
    return options::CameraReturnText(config);
}

void OptionSetCameraReturn(Config &config, int choice) {
    options::SetCameraReturn(config, choice);
}

void OptionRestoreCameraReturn(Config &config, const Config &defaults) {
    options::RestoreCameraReturn(config, defaults);
}

namespace options {

void SetBinding(Config &config, std::string_view action, std::string_view key) {
    auto binding = std::ranges::find(config.key_bindings, std::string(action), &ConfigKeyBinding::action);
    std::vector<std::string> keys;
    keys.emplace_back(key);
    if (binding == config.key_bindings.end()) {
        config.key_bindings.push_back({std::string(action), std::move(keys)});
    } else {
        binding->keys = std::move(keys);
    }
}

} // namespace options
