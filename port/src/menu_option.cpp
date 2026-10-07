#include "menu_option.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "gametext.hpp"
#include "gameutil.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "platform/display.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

// The Options screen as the port's settings screen: pages of rows whose values change config.json
// through ConfigChange.

namespace {

// The save's configuration words (CSaveData::config) that hold the game's options.
constexpr int kWordClockOff = 2;
constexpr int kWordFastTime = 3;
constexpr int kWordFastMessages = 4;
constexpr int kWordMono = 5;
constexpr int kWordSoftFocusOff = 6;
constexpr int kWordVibrationOff = 7;
constexpr int kWordNamesOff = 8;
constexpr int kWordPlayerDamageOff = 9;
constexpr int kWordEnemyDamageOff = 10;
constexpr int kWordEnemyHpOff = 11;

// minimap_status that hides the dungeon map.
constexpr int kMapOff = 3;

// The font's pad buttons, as {N} codes.
#define PAD_GLYPH_L1 "{-766}"
#define PAD_GLYPH_R1 "{-765}"
#define PAD_GLYPH_CIRCLE "{-762}"
#define PAD_GLYPH_TRIANGLE "{-761}"
#define PAD_GLYPH_CROSS "{-760}"
#define PAD_GLYPH_SQUARE "{-759}"

// Where the screen draws, on the game's 640x480 2D screen: the page names below the menu's own
// heading, the rows, the scroll bar right of the values, and EXIT left of the help window.
constexpr int kTabY = 96;
constexpr int kTabGap = 24;
constexpr int kRowY = 138;
constexpr int kRowStep = 25;
constexpr int kVisibleRows = 8;
constexpr int kLabelX = 138;
constexpr int kValueX = 374;
constexpr int kValueRight = 554;
constexpr int kBarX = 580;
constexpr int kExitX = 136;
constexpr int kExitY = 352;
constexpr int kExitHeight = 31;
// A help window's frame round its middle (MenuHelpWinDraw): 24 pixels a side and 22 above and below.
constexpr int kPlateSide = 24;
constexpr int kPlateHeight = 44;

// Retail's menu auto-repeat (the title's and the dungeon menu's): the frames a direction is held
// before it repeats, and between repeats.
constexpr int kRepeatDelay = 30;
constexpr int kRepeatStep = 5;

// Frames the scroll bar stays after the list last moved, and the last of them it fades over.
constexpr int kBarFrames = 75;
constexpr int kBarFade = 15;

struct Row {
    // The config.json name, for ConfigAppliesOnRestart.
    const char *key;
    const char *label;
    // Port help; null: game_help, an allmenu.mes message.
    const char *help;
    int         game_help;
    int (*count)(const Config &);
    // Which of its count choices the setting is at; set makes it that choice.
    int (*get)(const Config &);
    void (*set)(Config &, int choice);
    // The value as shown; null: the choice's name in names, '|' between them.
    std::string (*text)(const Config &);
    const char *names = nullptr;
    // Puts back the default exactly where it need not be one of the choices; null: its choice.
    void (*restore)(Config &config, const Config &defaults) = nullptr;
};

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
Row GameRow(const char *key, const char *label, const char *names, int help) {
    return {key, label, nullptr, help, Two, GetOption<Field, First>, SetOption<Field, First>, nullptr, names};
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
    return {key, label, help, -1, Two, GetOnOff<Field>, SetOnOff<Field>, nullptr, "On|Off"};
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

using Resolution = OptionResolution;

// The smallest size the list offers from the display's own modes.
constexpr int kMinWidth = 800;
constexpr int kMinHeight = 600;

// 0x0 is the monitor's resolution.
constexpr Resolution kResolutions[] = {
    {0,    0   },
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

// The sizes that fit the window's monitor, kept while the screen is open.
std::vector<Resolution> g_sizes;

// Desktop: whatever fullscreen says, the window is fullscreen at the monitor's size (WindowConfig).
bool Desktop(const Config &config) {
    return config.window_width == 0 && config.window_height == 0;
}

void ListResolutions() {
    const Config &config = ConfigGet();
    int           width = 0;
    int           height = 0;
    bool          known = WindowDisplaySize(width, height, !config.fullscreen && !Desktop(config));
    g_sizes = OptionResolutionList(WindowDisplayModes(), config.window_width, config.window_height, known, width,
                                   height);
}

int ResolutionChoice(const Config &config) {
    double area = static_cast<double>(config.window_width) * config.window_height;
    int    best = 0;
    for (int i = 0; i < static_cast<int>(g_sizes.size()); ++i) {
        const Resolution &size = g_sizes[i];
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

constexpr double kMaxFps[] = {0, 30, 60, 75, 90, 120, 144, 165, 240};

constexpr ConfigPresentMode kPresentModes[] = {ConfigPresentMode::Fifo, ConfigPresentMode::Mailbox,
                                               ConfigPresentMode::Immediate};

// Rows whose choices are a setting's own steps.

int MapChoice(const Config &config) {
    return config.options.map == 0 ? kMapOff : config.options.map - 1;
}

void SetMap(Config &config, int choice) {
    config.options.map = choice == kMapOff ? 0 : choice + 1;
}

int MapCount(const Config &) {
    return 4;
}

int ResolutionCount(const Config &) {
    return static_cast<int>(g_sizes.size());
}

void SetResolution(Config &config, int choice) {
    config.window_width = g_sizes[choice].width;
    config.window_height = g_sizes[choice].height;
}

std::string ResolutionText(const Config &config) {
    if (config.window_width == 0 && config.window_height == 0) {
        return "Desktop";
    }
    return std::format("{} x {}", config.window_width, config.window_height);
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
    int        width = 0;
    int        height = 0;
    bool       known = WindowDisplaySize(width, height, true);
    Resolution window = known ? kResolutions[1] : Resolution{1280, 960};
    for (const Resolution &size : kResolutions) {
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

int PresentModeCount(const Config &) {
    return static_cast<int>(std::size(kPresentModes));
}

int PresentModeChoice(const Config &config) {
    return static_cast<int>(std::ranges::find(kPresentModes, config.present_mode) - std::begin(kPresentModes));
}

void SetPresentMode(Config &config, int choice) {
    config.present_mode = kPresentModes[choice];
}

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

// Hundredths up to 0.99, then tenths up to 10, whatever the setting's unit.
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
    return config.mouse_sensitivity > 10.0f ? std::format("{:.2g}", config.mouse_sensitivity)
                                            : std::format("{:.2f}", config.mouse_sensitivity);
}

void RestoreMouseSensitivity(Config &config, const Config &defaults) {
    config.mouse_sensitivity = defaults.mouse_sensitivity;
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

int ZoomResetCount(const Config &config) { return ZoomResetChoice(config) == 5 ? 6 : 5; }

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
    return config.stick_sensitivity > 10.0f ? std::format("{:.2g}", config.stick_sensitivity)
                                            : std::format("{:.2f}", config.stick_sensitivity);
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
    return config.gyro_sensitivity > 10.0f ? std::format("{:.2g}", config.gyro_sensitivity)
                                           : std::format("{:.2f}", config.gyro_sensitivity);
}

void RestoreGyroSensitivity(Config &config, const Config &defaults) {
    config.gyro_sensitivity = defaults.gyro_sensitivity;
}

const Row kGameRows[] = {
    GameRow<&ConfigGameOptions::save_cursor_position, true>("game.save_cursor_position", "Save Cursor Position",
                                                            "On|Off", 0x15E),
    GameRow<&ConfigGameOptions::fast_messages, false>("game.message_speed", "Message Speed", "Normal|Fast", 0x160),
    GameRow<&ConfigGameOptions::clock, true>("game.clock", "Clock", "On|Off", 0x162),
    GameRow<&ConfigGameOptions::fast_time, false>("game.time_speed", "Time Speed", "Normal|Fast", 0x163),
    Row{"game.map", "Dungeon Map", nullptr, 0x164, MapCount, MapChoice, SetMap, nullptr, "1|2|3|Off"},
    GameRow<&ConfigGameOptions::enemy_damage, true>("game.enemy_damage", "Enemy Damage", "On|Off", 0x165),
    GameRow<&ConfigGameOptions::player_damage, true>("game.player_damage", "Party Damage", "On|Off", 0x166),
    GameRow<&ConfigGameOptions::enemy_hp, true>("game.enemy_hp", "Enemy HP", "On|Off", 0x167),
    GameRow<&ConfigGameOptions::names, true>("game.names", "Names", "On|Off", 0x168),
    OnOffRow<&Config::discord_rich_presence>("discord.rich_presence", "Enable Discord",
                                             "\"Discord Rich Presence\"\nShows what you are\nplaying on Discord."),
    OnOffRow<&Config::qte_always_win>("game.qte_always_win", "Always Win QTEs",
                                      "\"Always Win QTEs\"\nButton prompts always\nend in a perfect."),
};

const Row kDisplayRows[] = {
    Row{"video.fullscreen", "Window Mode", "\"Window Mode\"\nPlay in a window or\non the whole screen.", -1, Two,
        Fullscreen, SetFullscreen, nullptr, "Windowed|Fullscreen", RestoreFullscreen},
    Row{"video.width", "Resolution",
        "\"Resolution\"\nThe window's size.\nFullscreen and Desktop\nuse the whole monitor.", -1, ResolutionCount,
        ResolutionChoice, SetResolution, ResolutionText},
    Row{"video.present_mode", "V-Sync", "\"V-Sync\"\nOn: no tearing.\nFast: no tearing, less\ndelay. Off: may tear.",
        -1, PresentModeCount, PresentModeChoice, SetPresentMode, nullptr, "On|Fast|Off"},
    Row{"video.max_fps", "Frame Limit", "\"Frame Limit\"\nThe most frames drawn\nin a second.", -1, MaxFpsCount,
        MaxFpsChoice, SetMaxFps, MaxFpsText},
    Row{"video.aspect", "Aspect Ratio", "\"Aspect Ratio\"\nWide: the world fills\nthe window. 4:3: the\nPS2's picture.",
        -1, Two, Aspect, SetAspect, nullptr, "Wide|4:3"},
    Row{"video.ui_scale", "Interface Size",
        "\"Interface Size\"\nThe size of the HUD\nand menus, from when\nOptions closes.", -1, UiScaleCount,
        UiScaleChoice, SetUiScale, UiScaleText},
    OnOffRow<&Config::interpolation>("video.interpolation", "Smooth Motion",
                                     "\"Smooth Motion\"\nDraws frames between\nthe game's steps."),
    OnOffRow<&Config::show_fps>("video.show_fps", "FPS Counter", "\"FPS Counter\"\nShows the frame rate\nin the corner."),
    GameRow<&ConfigGameOptions::soft_focus, true>("video.soft_focus", "Soft Focus", "On|Off", 0x169),
};

const Row kAudioRows[] = {
    Row{"audio.master_volume", "Volume", "\"Volume\"\nHow loud the game is.", -1, VolumeCount, VolumeChoice, SetVolume,
        VolumeText},
    GameRow<&ConfigGameOptions::stereo, true>("audio.sound", "Sound", "Stereo|Mono", 0x161),
};

const Row kControlRows[] = {
    GameRow<&ConfigGameOptions::vibration, true>("input.vibration", "Vibration", "On|Off", 0x15F),
    Row{"input.mouse_sensitivity", "Mouse Sensitivity", "\"Mouse Sensitivity\"\nHow fast the mouse\nturns the camera.",
        -1, MouseSensitivityCount, MouseSensitivityChoice, SetMouseSensitivity, MouseSensitivityText, nullptr,
        RestoreMouseSensitivity},
    OnOffRow<&Config::mouse_invert_y>("input.mouse_invert_y", "Invert Mouse Y",
                                      "\"Invert Mouse Y\"\nMoving the mouse up\nlooks down."),
    OnOffRow<&Config::mouse_zoom>("input.mouse_zoom", "Mouse Wheel Zoom",
                                  "\"Mouse Wheel Zoom\"\nScroll to move closer\nor farther from your\ncharacter."),
    Row{"input.bindings.zoom_reset", "Reset Zoom", "\"Reset Zoom\"\nRestores the normal\ncamera distance.",
        -1, ZoomResetCount, ZoomResetChoice, SetZoomReset, ZoomResetText, nullptr, RestoreZoomReset},
    Row{"input.stick_sensitivity", "Stick Sensitivity", "\"Stick Sensitivity\"\nHow far a gamepad's\nstick has to tilt.",
        -1, StickSensitivityCount, StickSensitivityChoice, SetStickSensitivity, StickSensitivityText, nullptr,
        RestoreStickSensitivity},
    OnOffRow<&Config::stick_invert_x>("input.stick_invert_x", "Invert Stick X",
                                      "\"Invert Stick X\"\nFlips the camera's\nleft and right."),
    OnOffRow<&Config::stick_invert_y>("input.stick_invert_y", "Invert Stick Y",
                                      "\"Invert Stick Y\"\nFlips the camera's\nup and down."),
    Row{"input.gyro", "Gyro", "\"Gyro\"\nWhen tilting the pad\nturns the camera.", -1, GyroCount, GyroChoice, SetGyro,
        nullptr, "Off|Always|First Person|While Held"},
    Row{"input.gyro_sensitivity", "Gyro Sensitivity", "\"Gyro Sensitivity\"\nHow fast tilting\nturns the camera.",
        -1, GyroSensitivityCount, GyroSensitivityChoice, SetGyroSensitivity, GyroSensitivityText, nullptr,
        RestoreGyroSensitivity},
    OnOffRow<&Config::gyro_invert_x>("input.gyro_invert_x", "Invert Gyro X",
                                     "\"Invert Gyro X\"\nFlips the gyro's\nleft and right."),
    OnOffRow<&Config::gyro_invert_y>("input.gyro_invert_y", "Invert Gyro Y",
                                     "\"Invert Gyro Y\"\nFlips the gyro's\nup and down."),
};

// A page of the screen. Every page so far is a list of rows; one that needs its own layout, such as
// key bindings, would add its own key and draw functions here and leave rows empty.
struct Tab {
    const char          *name;
    std::span<const Row> rows;
};

const Tab kTabs[] = {
    {"Game",     kGameRows   },
    {"Display",  kDisplayRows},
    {"Audio",    kAudioRows  },
    {"Controls", kControlRows},
};

constexpr int kTabCount = static_cast<int>(std::size(kTabs));
constexpr int kTabHelp = 998;
constexpr int kExitHelp = 999;
constexpr int kSaveHelp = 997;
constexpr int kDisplayHelp = 996;
constexpr int kDisplayNowHelp = 995;
constexpr int kRowHelp = 1000;

// The cursor on the page names, a row, or the exit button (the page's row count).
constexpr int kOnTabs = -1;

// What the pointer is over that is not a row, a page name or EXIT but takes a click.
enum class Glyph {
    None,
    L1,
    R1,
    Up,
    Down,
};

struct Screen {
    // From InitMenuOption until MenuOptionKey reports the screen closed.
    bool      open;
    int       mode;
    int       block_no;
    int       step;
    int       step_count;
    bool      texture_ready;
    CTexture *texture;
    int       tab;
    int       row;
    // Each page's first row in view.
    int   first[kTabCount];
    float cursor_x;
    float cursor_y;
    // Frames left of the scroll bar's showing (kBarFrames).
    int bar_frames;
    // What the screen opened with, for undo.
    Config opened;
    bool   save_failed;
    // The text of kDisplayNowHelp, which names the window's mode.
    std::string display_now;
    // allmenu.mes, which CommonMenuMes2 reads, while the help line shows port text.
    short *game_messages;
    // Frames each pad button has been held, for the screen's own auto-repeat.
    int held[16];
    // The mouse's pointer on the 2D screen, shown once the mouse moves and until a key moves the
    // cursor; the buttons it held at the last frame.
    bool          pointing;
    float         pointer_x;
    float         pointer_y;
    std::uint32_t mouse_buttons;
    Glyph         glyph;
};

Screen g_screen;

// The port's text, laid out once: a relayout runs MakeRandTbl's 64 rand() calls. A row's help is
// message kRowHelp plus its index among all the pages' rows.
struct Texts {
    GameText             tabs[kTabCount];
    std::deque<GameText> labels;
    std::deque<GameText> values;
    GameText             left;
    GameText             right;
    GameText             l1;
    GameText             r1;
    GameText             shortcuts;
    GameTextFile         help;

    Texts() {
        left.Set("<");
        right.Set(">");
        l1.Set(PAD_GLYPH_L1);
        r1.Set(PAD_GLYPH_R1);
        shortcuts.Set(PAD_GLYPH_SQUARE " Reset tab\n" PAD_GLYPH_TRIANGLE " Undo changes\n" PAD_GLYPH_CIRCLE " Close");
        help.Set(kSaveHelp, "Could not save.\nChanges apply now.\nClose to retry writing\nconfig.json.");
        help.Set(kDisplayHelp, "The display could not\nchange, and kept the\nmode it had. Try\nanother mode or size.");
        help.Set(kTabHelp, "\"Options\"\n" PAD_GLYPH_L1 " " PAD_GLYPH_R1 " or left and right\nturn the page.");
        help.Set(kExitHelp, PAD_GLYPH_CROSS " Close\n" PAD_GLYPH_SQUARE " This page's defaults\n" PAD_GLYPH_TRIANGLE
                                            " Undo every change\n" PAD_GLYPH_CIRCLE " Close from any row");
        int index = 0;
        for (int t = 0; t < kTabCount; ++t) {
            tabs[t].Set(kTabs[t].name);
            for (const Row &row : kTabs[t].rows) {
                labels.emplace_back().Set(row.label);
                values.emplace_back();
                if (row.help != nullptr) {
                    help.Set(kRowHelp + index, row.help);
                }
                ++index;
            }
        }
    }
};

Texts &GetTexts() {
    static Texts texts;
    return texts;
}

int RowCount(int tab) {
    return static_cast<int>(kTabs[tab].rows.size());
}

// The row's index among all the pages' rows.
int RowIndex(int tab, int row) {
    int index = row;
    for (int t = 0; t < tab; ++t) {
        index += RowCount(t);
    }
    return index;
}

int FirstRow() {
    return g_screen.first[g_screen.tab];
}

// The top of a row's line on the screen, the page scrolled as it is.
int RowTop(int row) {
    return kRowY + (row - FirstRow()) * kRowStep;
}

bool Scrolls(int tab) {
    return RowCount(tab) > kVisibleRows;
}

void ShowBar() {
    if (Scrolls(g_screen.tab)) {
        g_screen.bar_frames = kBarFrames;
    }
}

void ScrollTo(int first) {
    int most = std::max(0, RowCount(g_screen.tab) - kVisibleRows);
    first = std::clamp(first, 0, most);
    if (first != FirstRow()) {
        g_screen.first[g_screen.tab] = first;
        ShowBar();
    }
}

// After the mouse scrolled the page, takes the cursor's row along so it stays in view.
void CursorToScroll() {
    if (g_screen.row >= 0 && g_screen.row < RowCount(g_screen.tab)) {
        g_screen.row = std::clamp(g_screen.row, FirstRow(), FirstRow() + kVisibleRows - 1);
    }
}

// Scrolls the page so the cursor's row is in view.
void ScrollToCursor() {
    int row = g_screen.row;
    if (row < 0 || row >= RowCount(g_screen.tab)) {
        return;
    }
    if (row < FirstRow()) {
        ScrollTo(row);
    } else if (row >= FirstRow() + kVisibleRows) {
        ScrollTo(row - kVisibleRows + 1);
    }
}

int TabX(int tab) {
    int x = kLabelX;
    for (int t = 0; t < tab; ++t) {
        x += GetTexts().tabs[t].Width() + kTabGap;
    }
    return x;
}

int L1X() {
    return kLabelX - GetTexts().l1.Width() - 12;
}

int R1X() {
    return TabX(kTabCount - 1) + GetTexts().tabs[kTabCount - 1].Width() + 12;
}

// Puts the settings into effect; a display mode the window did not take is shown as the mode it
// is in (DisplayApply).
void Apply(const Config &config) {
    g_screen.save_failed = !DisplayApply(config);
    ListResolutions();
}

// A change of the window's mode that SDL finished late, which the host's pump has put in the
// settings (DisplayPump).
void FollowWindow() {
    if (std::optional<bool> saved = DisplayTakePumpSave()) {
        g_screen.save_failed = !*saved;
        ListResolutions();
    }
}

void Change(const Row &row, int choice) {
    Config config = ConfigGet();
    row.set(config, choice);
    Apply(config);
}

// Moves the row's setting one choice left or right, or round from the last to the first; plays
// the cursor sound when it moved.
void Step(const Row &row, int direction, bool wrap) {
    const Config &config = ConfigGet();
    int           count = row.count(config);
    int           choice = row.get(config);
    int           next = wrap ? (choice + direction + count) % count : std::clamp(choice + direction, 0, count - 1);
    if (next != choice) {
        Change(row, next);
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

void ResetTab(int tab) {
    Config       config = ConfigGet();
    const Config defaults;
    for (const Row &row : kTabs[tab].rows) {
        if (row.restore != nullptr) {
            row.restore(config, defaults);
        } else {
            row.set(config, row.get(defaults));
        }
    }
    Apply(config);
}

void ShowHelp() {
    ClsMes &mes = CommonMenuMes2;
    short  *buffer = GetTexts().help.Data();
    int     message = kExitHelp;
    if (DisplayGetWarning() == DisplayWarning::Kept) {
        message = kDisplayHelp;
    } else if (DisplayGetWarning() == DisplayWarning::Changed) {
        WindowMode  mode = DisplayShownMode();
        std::string now = mode.fullscreen ? "fullscreen." : std::format("a {} x {}\nwindow.", mode.width, mode.height);
        if (now != g_screen.display_now) {
            g_screen.display_now = now;
            GetTexts().help.Set(kDisplayNowHelp, "The display could not\nchange as asked. It is\nnow " + now);
            buffer = GetTexts().help.Data();
            mes.mes_made = -1;
        }
        message = kDisplayNowHelp;
    } else if (g_screen.save_failed) {
        message = kSaveHelp;
    } else if (g_screen.row == kOnTabs) {
        message = kTabHelp;
    } else if (g_screen.row < RowCount(g_screen.tab)) {
        const Row &row = kTabs[g_screen.tab].rows[g_screen.row];
        if (row.help != nullptr) {
            message = kRowHelp + RowIndex(g_screen.tab, g_screen.row);
        } else {
            buffer = g_screen.game_messages;
            message = row.game_help;
        }
    }
    if (mes.buff != buffer || mes.mes_made != message) {
        mes.SetBuff(buffer);
        mes.MakeMesWin(message);
    }
}

void Close() {
    // ConfigChange retries a failed write even when no setting changed.
    g_screen.save_failed = !ConfigChange(ConfigGet());
    g_screen.step = OPTION_STEP_FADE_OUT;
    g_screen.step_count = 0;
    InputSetMenuMouse(false);
    ComMenuSePlay(MENU_SOUND_REFUSE);
}

// The pad as held, before the game's auto-repeat, which each menu that opens this screen sets its
// own way: the d-pad, and the left stick as the menus' MenuModeOn(120) reads it.
std::uint16_t HeldButtons() {
    const InputPadState &pad = InputGetPad(0);
    std::uint16_t        held = pad.buttons;
    int                  x = AxisCalibration(pad.left_x);
    int                  y = AxisCalibration(pad.left_y);
    if (x > 120) {
        held |= PAD_RIGHT;
    } else if (x < -120) {
        held |= PAD_LEFT;
    }
    if (y > 120) {
        held |= PAD_DOWN;
    } else if (y < -120) {
        held |= PAD_UP;
    }
    return held;
}

// Which of mask's buttons fire this frame: on the press, then retail's menu auto-repeat.
int Pressed(std::uint16_t held, int mask, bool repeat) {
    int fired = 0;
    for (int bit = 0; bit < 16; ++bit) {
        int button = 1 << bit;
        if ((mask & button) == 0) {
            continue;
        }
        if ((held & button) == 0) {
            g_screen.held[bit] = 0;
            continue;
        }
        int frames = ++g_screen.held[bit];
        if (frames == 1 || (repeat && frames > kRepeatDelay && (frames - kRepeatDelay) % kRepeatStep == 0)) {
            fired |= button;
        }
    }
    return fired;
}

void SetTab(int tab) {
    bool on_exit = g_screen.row == RowCount(g_screen.tab);
    g_screen.tab = (tab + kTabCount) % kTabCount;
    if (g_screen.row != kOnTabs) {
        g_screen.row = on_exit ? RowCount(g_screen.tab) : std::min(g_screen.row, RowCount(g_screen.tab) - 1);
        ScrollToCursor();
    }
}

bool Inside(float x, float y, int left, int top, int right, int bottom) {
    return x >= left && x < right && y >= top && y < bottom;
}

// The mouse: motion moves the pointer and selects what it is over, a click acts on it, the wheel
// scrolls the page, and the right button closes.
void RunMouse() {
    InputMenuMouse mouse = InputTakeMenuMouse();
    std::uint32_t  clicked = mouse.buttons & ~g_screen.mouse_buttons;
    bool           moved = mouse.dx != 0.0f || mouse.dy != 0.0f;
    g_screen.mouse_buttons = mouse.buttons;

    if ((clicked & 2) != 0) {
        Close();
        return;
    }
    if (moved || clicked != 0) {
        if (!g_screen.pointing) {
            g_screen.pointing = true;
            g_screen.pointer_x = g_screen.cursor_x + 26.0f;
            g_screen.pointer_y = g_screen.cursor_y + 14.0f;
        }
        // The 2D screen is 480 high in the window, whatever its shape; the screen keeps the
        // interface at 100% (MenuOptionOpen).
        int   width = 0;
        int   height = 0;
        float scale = WindowSize(width, height) ? 1.0f / std::min(width / 640.0f, height / 480.0f) : 1.0f;
        g_screen.pointer_x = std::clamp(g_screen.pointer_x + mouse.dx * scale, 0.0f, 639.0f);
        g_screen.pointer_y = std::clamp(g_screen.pointer_y + mouse.dy * scale, 0.0f, 479.0f);
    }
    if (!g_screen.pointing) {
        return;
    }
    if (moved) {
        ShowBar();
    }

    float x = g_screen.pointer_x;
    float y = g_screen.pointer_y;
    int   rows = RowCount(g_screen.tab);
    int   old_row = g_screen.row;
    int   old_tab = g_screen.tab;

    if (mouse.wheel != 0.0f && Inside(x, y, kLabelX - 8, kRowY, kBarX + 16, kRowY + kVisibleRows * kRowStep)) {
        ScrollTo(FirstRow() + (mouse.wheel > 0.0f ? -1 : 1));
        CursorToScroll();
        moved = true;
    }

    // What the pointer is over: a row in view, the exit button, a page name, L1 or R1, a scroll arrow.
    int over_row = -2;
    int over_tab = -1;
    for (int r = FirstRow(); r < std::min(rows, FirstRow() + kVisibleRows); ++r) {
        if (Inside(x, y, kLabelX - 8, RowTop(r) - 1, kValueRight + 8, RowTop(r) + kRowStep - 1)) {
            over_row = r;
        }
    }
    if (Inside(x, y, kExitX, kExitY, kExitX + 60, kExitY + kExitHeight)) {
        over_row = rows;
    }
    for (int t = 0; t < kTabCount; ++t) {
        if (Inside(x, y, TabX(t) - 8, kTabY - 6, TabX(t) + GetTexts().tabs[t].Width() + 8, kTabY + 22)) {
            over_tab = t;
        }
    }
    int   bar_bottom = kRowY + kVisibleRows * kRowStep;
    Glyph glyph = Glyph::None;
    // The first page's name comes first where it meets L1.
    if (over_tab < 0 && Inside(x, y, L1X() - 4, kTabY - 6, kLabelX - 4, kTabY + 22)) {
        glyph = Glyph::L1;
    } else if (Inside(x, y, R1X() - 4, kTabY - 6, R1X() + GetTexts().r1.Width() + 4, kTabY + 22)) {
        glyph = Glyph::R1;
    } else if (Scrolls(g_screen.tab) && Inside(x, y, kBarX - 6, kRowY - 4, kBarX + 14, kRowY + 14)) {
        glyph = Glyph::Up;
    } else if (Scrolls(g_screen.tab) && Inside(x, y, kBarX - 6, bar_bottom - 18, kBarX + 14, bar_bottom)) {
        glyph = Glyph::Down;
    }
    g_screen.glyph = glyph;
    // The scroll bar stays while the pointer is on one of its arrows.
    if (glyph == Glyph::Up || glyph == Glyph::Down) {
        ShowBar();
    }

    if (moved) {
        if (over_row != -2) {
            g_screen.row = over_row;
        } else if (over_tab >= 0 || glyph == Glyph::L1 || glyph == Glyph::R1) {
            g_screen.row = kOnTabs;
        }
    }

    if ((clicked & 1) != 0) {
        if (over_tab >= 0) {
            SetTab(over_tab);
        } else if (glyph == Glyph::L1 || glyph == Glyph::R1) {
            SetTab(g_screen.tab + (glyph == Glyph::L1 ? -1 : 1));
        } else if (glyph == Glyph::Up || glyph == Glyph::Down) {
            ScrollTo(FirstRow() + (glyph == Glyph::Up ? -1 : 1));
            CursorToScroll();
        } else if (over_row == rows) {
            Close();
            return;
        } else if (over_row >= 0) {
            const Row &row = kTabs[g_screen.tab].rows[over_row];
            if (x >= kValueX - 8 && x < kValueX + 24) {
                Step(row, -1, false);
            } else if (x >= kValueRight - 16) {
                Step(row, 1, false);
            } else if (x >= kValueX) {
                Step(row, 1, true);
            }
        }
    }

    if (g_screen.tab != old_tab || g_screen.row != old_row) {
        ShowBar();
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

void RunKeys() {
    int           old_tab = g_screen.tab;
    int           old_row = g_screen.row;
    std::uint16_t held = HeldButtons();
    int           moves = Pressed(held, PAD_DPAD, true);
    int           pages = Pressed(held, PAD_L1 | PAD_L2 | PAD_R1 | PAD_R2, false);
    int           actions = Pressed(held, PAD_CROSS | PAD_CIRCLE | PAD_SQUARE | PAD_TRIANGLE, false);

    if (moves != 0 || pages != 0 || actions != 0) {
        g_screen.pointing = false;
    }
    if ((pages & (PAD_L1 | PAD_L2)) != 0) {
        SetTab(g_screen.tab - 1);
    } else if ((pages & (PAD_R1 | PAD_R2)) != 0) {
        SetTab(g_screen.tab + 1);
    }
    int rows = RowCount(g_screen.tab);

    if ((moves & PAD_DOWN) != 0) {
        g_screen.row = g_screen.row == rows ? kOnTabs : g_screen.row + 1;
    } else if ((moves & PAD_UP) != 0) {
        g_screen.row = g_screen.row == kOnTabs ? rows : g_screen.row - 1;
    }
    ScrollToCursor();

    int direction = 0;
    if ((moves & PAD_LEFT) != 0) {
        direction = -1;
    } else if ((moves & PAD_RIGHT) != 0) {
        direction = 1;
    }

    if ((actions & PAD_CIRCLE) != 0) {
        Close();
    } else if ((actions & PAD_SQUARE) != 0) {
        ResetTab(g_screen.tab);
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if ((actions & PAD_TRIANGLE) != 0) {
        Apply(g_screen.opened);
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if (g_screen.row == kOnTabs) {
        if (direction != 0) {
            SetTab(g_screen.tab + direction);
        }
    } else if (g_screen.row < rows) {
        const Row &row = kTabs[g_screen.tab].rows[g_screen.row];
        if (direction != 0) {
            Step(row, direction, false);
        } else if ((actions & PAD_CROSS) != 0) {
            Step(row, 1, true);
        }
    } else if ((actions & PAD_CROSS) != 0) {
        Close();
    }

    if (g_screen.tab != old_tab || g_screen.row != old_row) {
        ShowBar();
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

void DrawSprite(int x, int y, int u, int v, int width, int height, int alpha) {
    DrawMenu2DSprite(g_screen.texture, CRect_i_(x, y, width, height), CRect_i_(u, v, width, height), alpha);
}

// A help window round the 2D rectangle from left to right, one text line high.
void DrawPlate(int left, int right, int alpha) {
    float middle = static_cast<float>(right - left + 2 * 14 - 2 * kPlateSide) / 16.0f;
    MenuHelpWinDraw(left - 14, kTabY - (kPlateHeight - 20) / 2, middle, 0.0f, alpha);
}

// The page names on a help window's plate, set apart from the rows as the menus set off their notes.
void DrawTabPlate(int alpha) {
    DrawPlate(L1X(), R1X() + GetTexts().r1.Width(), alpha);
}

// The scroll bar while the list moves: the help window's fill as its track, retail's screen-adjust
// arrows at its ends and option.pac's stone as the thumb, its length the share of the rows in view.
void DrawScrollBar(int alpha) {
    if (!Scrolls(g_screen.tab) || g_screen.bar_frames <= 0) {
        return;
    }
    alpha = alpha * std::min(g_screen.bar_frames, kBarFade) / kBarFade;
    int rows = RowCount(g_screen.tab);
    int top = kRowY + 14;
    int height = kVisibleRows * kRowStep - 32;
    int thumb = std::max(12, height * kVisibleRows / rows);
    int thumb_y = top + (height - thumb) * FirstRow() / (rows - kVisibleRows);
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    DrawMenu2DSprite(StayTex, CRect_i_(kBarX, top, 8, height), CRect_i_(22, 22, 16, 20), alpha);
    MenuTextureReload(g_screen.block_no);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX, thumb_y, 8, thumb), CRect_i_(140, 280, 24, 32), alpha);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX - 4, kRowY - 4, 16, 16), CRect_i_(452, 136, 24, 24), alpha);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX - 4, top + height + 2, 16, 16), CRect_i_(452, 164, 24, 24),
                     alpha);
}

constexpr int   kBracketBeats = 30;
constexpr float kBracketClose = 4.0f;

// Retail's bracket corners round the chosen part, and its bobbing hand left of them or at the
// mouse's pointer.
void DrawCursor(int left, int right, int top, int alpha, int hand_left = -1) {
    static int bracket_count = 0;
    static int hand_count = 0;
    float      pulse = kBracketClose * static_cast<float>(bracket_count) / static_cast<float>(kBracketBeats - 1);
    int        x0 = static_cast<int>(left + pulse);
    int        x1 = static_cast<int>(right - pulse);
    int        y0 = static_cast<int>(top + pulse);
    int        y1 = static_cast<int>(top + 26 - pulse);
    DrawSprite(x0, y0, 0xB2, 0xF8, 16, 16, alpha);
    DrawSprite(x1, y0, 0xC2, 0xF8, 16, 16, alpha);
    DrawSprite(x0, y1, 0xB2, 0x108, 16, 16, alpha);
    DrawSprite(x1, y1, 0xC2, 0x108, 16, 16, alpha);
    bracket_count = (bracket_count + 1) % kBracketBeats;

    float target_x = static_cast<float>(hand_left >= 0 ? hand_left : left - 38);
    float target_y = static_cast<float>(top + 9);
    g_screen.cursor_x += (target_x - g_screen.cursor_x) / 4.0f;
    g_screen.cursor_y += (target_y - g_screen.cursor_y) / 4.0f;
    float hand_x = g_screen.cursor_x + 7.0f * cosf(0.0805536583f * hand_count);
    float hand_y = g_screen.cursor_y + 5.0f * sinf(0.116355285f * hand_count);
    if (g_screen.pointing) {
        // The finger's tip at the pointer.
        hand_x = g_screen.pointer_x - 26.0f;
        hand_y = g_screen.pointer_y - 14.0f;
    }
    CRect_i_ hand(0xD2, 0xF8, 0x20, 0x20);
    DrawMenu2DSprite(g_screen.texture, CRect_i_((int) (5.0f + hand_x), (int) (3.0f + hand_y), 0x20, 0x20), hand, 0, 0, 0,
                     (alpha * 100) >> 7);
    DrawMenu2DSprite(g_screen.texture, CRect_i_((int) hand_x, (int) hand_y, 0x20, 0x20), hand, alpha);
    hand_count = (hand_count + 1) % 0x107AC0;
}

// Writes options into a save's configuration words, dungeon status and menu cursors.
void WriteOptions(CSaveData &save, const ConfigGameOptions &options) {
    s32 *words = static_cast<s32 *>(save.GetConfigData());
    words[kWordClockOff] = !options.clock;
    words[kWordFastTime] = options.fast_time;
    words[kWordFastMessages] = options.fast_messages;
    words[kWordMono] = !options.stereo;
    words[kWordSoftFocusOff] = !options.soft_focus;
    words[kWordVibrationOff] = !options.vibration;
    words[kWordNamesOff] = !options.names;
    words[kWordPlayerDamageOff] = !options.player_damage;
    words[kWordEnemyDamageOff] = !options.enemy_damage;
    words[kWordEnemyHpOff] = !options.enemy_hp;
    reinterpret_cast<CUserStatus *>(save.GetDngStatus())->minimap_status = options.map == 0 ? kMapOff : options.map - 1;
    save.GetMenuCursor()->reset_pos = !options.save_cursor_position;
}

} // namespace

int OptionZoomResetChoice(const Config &config) { return ZoomResetChoice(config); }

int OptionZoomResetCount(const Config &config) { return ZoomResetCount(config); }

std::string OptionZoomResetText(const Config &config) { return ZoomResetText(config); }

void OptionSetZoomReset(Config &config, int choice) { SetZoomReset(config, choice); }

void OptionRestoreZoomReset(Config &config, const Config &defaults) { RestoreZoomReset(config, defaults); }

std::vector<OptionResolution> OptionResolutionList(std::span<const DisplayModeSize> modes, int configured_width,
                                                   int configured_height, bool display_known, int display_width,
                                                   int display_height) {
    std::vector<OptionResolution> sizes(std::begin(kResolutions), std::end(kResolutions));
    for (const DisplayModeSize &mode : modes) {
        if (mode.width >= kMinWidth && mode.height >= kMinHeight) {
            sizes.push_back({mode.width, mode.height});
        }
    }
    bool configured = configured_width > 0 && configured_height > 0;
    if (configured) {
        sizes.push_back({configured_width, configured_height});
    }
    // Desktop stays first; the rest run from the smallest area, narrower first.
    std::sort(sizes.begin() + 1, sizes.end(), [](const OptionResolution &a, const OptionResolution &b) {
        return std::pair(a.width * a.height, a.width) < std::pair(b.width * b.height, b.width);
    });
    std::vector<OptionResolution> listed;
    for (const OptionResolution &size : sizes) {
        bool fits = !display_known || (size.width <= display_width && size.height <= display_height) ||
                    (configured && size.width == configured_width && size.height == configured_height);
        bool again = !listed.empty() && listed.back().width == size.width && listed.back().height == size.height;
        if (fits && !again) {
            listed.push_back(size);
        }
    }
    return listed;
}

bool MenuOptionOpen() {
    return g_screen.open;
}

void GameOptionsApply(const ConfigGameOptions &options) {
    if (SaveData == nullptr) {
        return;
    }
    WriteOptions(*SaveData, options);
    CMenuCursor *cursor = SaveData->GetMenuCursor();
    if (cursor->reset_pos != 0) {
        cursor->InitPos();
    }
    // Takes config.json's stereo option whatever it is passed.
    CSnd.SetStereoMode(1);
}

void GameOptionsChanged(const Config &before, const Config &after) {
    if (after.options != before.options) {
        GameOptionsApply(after.options);
    }
}

void GameOptionsClear(CSaveData &save) {
    // These bytes remain reserved in the fixed save layout, but carry no player preference.
    s32 *words = static_cast<s32 *>(save.GetConfigData());
    std::fill(words + kWordClockOff, words + kWordEnemyHpOff + 1, 0);
    words[15] = 0;
    words[16] = 0;
    reinterpret_cast<CUserStatus *>(save.GetDngStatus())->minimap_status = 0;
    save.GetMenuCursor()->reset_pos = 0;
}

PC_OVERRIDE int InitMenuOption(int mode, int block_no, u_long128 *buffer) {
    if (buffer == nullptr) {
        buffer = (u_long128 *) read_buffer;
    }

    u_long128 *data = MenuCalcBufAlignment(buffer);
    StartReadBG();

    if (LoadFileBGMenuData(const_cast<char *>("option.pac"), data) <= 0) {
        return 0;
    }

    g_screen.open = true;
    g_screen.mode = mode;
    g_screen.block_no = block_no;

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        GamePad.SetAutoRepeat(PAD_DPAD, 30, 5);
        GamePad.MenuModeOn(120);
    }

    GetTexts();
    ListResolutions();
    g_screen.texture_ready = false;
    g_screen.step = OPTION_STEP_FADE_IN;
    g_screen.step_count = 0;
    g_screen.tab = 0;
    g_screen.row = 0;
    std::ranges::fill(g_screen.first, 0);
    g_screen.cursor_x = kLabelX - 48.0f;
    g_screen.cursor_y = RowTop(0);
    g_screen.bar_frames = 0;
    g_screen.opened = ConfigGet();
    g_screen.save_failed = false;
    g_screen.display_now.clear();
    // A display warning outlives the screen; a save result from before it opened does not.
    DisplayTakePumpSave();
    g_screen.game_messages = nullptr;
    std::ranges::fill(g_screen.held, 0);
    g_screen.pointing = false;
    g_screen.mouse_buttons = 0;
    g_screen.glyph = Glyph::None;
    return 1;
}

PC_OVERRIDE int MenuOptionKey() {
    int result = 0;
    FollowWindow();

    switch (g_screen.step) {
        case OPTION_STEP_FADE_IN:
            if (!g_screen.texture_ready) {
                ReadBG();

                if (ReadBGSync() == 0) {
                    LOADTEXTURE_INFO2 textures[3] = {
                        {const_cast<char *>("#frame_image_option#640#" SCREEN_HEIGHT_STR "#4"), 0, 0},
                        {nullptr,                                                               0, 0},
                        {nullptr,                                                               0, 0}
                    };
                    textures[0].block_no = g_screen.block_no;
                    textures[1].block_no = g_screen.block_no;
                    BG_READ_INFO *file = GetReadBGFile(0);
                    textures[1].name = (char *) GetPackFile((u_int *) file->buffer, const_cast<char *>("option.img"), nullptr);
                    TexManager.DeleteTextureBlock(g_screen.block_no);
                    TexManager.CleanUpTextureList();
                    TexManager.LoadTextureBlockEX(-1, textures);
                    g_screen.texture = TexManager.GetTexture(const_cast<char *>("option2"), -1);

                    if (g_screen.mode == OPTION_OPEN_TITLE) {
                        InitMenuMesSet(MENU_MES_SET_ALLMENU,
                                       (short *) GetPackFile((u_int *) file->buffer, const_cast<char *>("allmenu.mes"), nullptr));
                    }

                    g_screen.game_messages = CommonMenuMes2.buff;
                    ShowHelp();
                    g_screen.texture_ready = true;
                }
            }

            if (g_screen.texture_ready && g_screen.step_count > 12) {
                g_screen.step = OPTION_STEP_RUN;
                g_screen.step_count = 0;
                // What is still held from the menu that opened the screen, the mouse's buttons
                // too, fires only once let go.
                Pressed(HeldButtons(), 0xFFFF, false);
                InputSetMenuMouse(true);
                g_screen.mouse_buttons = InputTakeMenuMouse().buttons;
            }

            break;
        case OPTION_STEP_FADE_OUT:
            if (g_screen.step_count > 24) {
                if (g_screen.mode == OPTION_OPEN_TITLE) {
                    GamePad.AutoRepeatOff();
                    GamePad.MenuModeOff();
                    GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
                    GamePad.MenuModeOn(120);
                }

                CommonMenuMes2.SetBuff(g_screen.game_messages);
                CommonMenuMes2.mes_made = -1;
                g_screen.open = false;
                result = 1;
            }

            break;
        default:
            if (g_screen.bar_frames > 0) {
                g_screen.bar_frames--;
            }
            RunMouse();
            if (g_screen.step == OPTION_STEP_RUN) {
                RunKeys();
            }
            ShowHelp();
            break;
    }

    return result;
}

PC_OVERRIDE void DrawMenuOption() {
    setbilinear(0);

    if (!g_screen.texture_ready) {
        return;
    }

    MenuTextureReload(g_screen.block_no);
    int alpha = 0x80;

    switch (g_screen.step) {
        case OPTION_STEP_FADE_IN:
            alpha = g_screen.step_count * 7;
            break;
        case OPTION_STEP_FADE_OUT:
            alpha = 0x80 - g_screen.step_count * 7;
            break;
    }

    alpha = std::clamp(alpha, 0, 0x80);
    const Config &config = ConfigGet();
    Texts        &texts = GetTexts();
    const Tab    &tab = kTabs[g_screen.tab];
    int           rows = RowCount(g_screen.tab);
    int           first = FirstRow();
    int           last = std::min(rows, first + kVisibleRows);
    int           index = RowIndex(g_screen.tab, 0);

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        DrawSprite(0x50, 0x28, 0xB3, 0x118, 0xAA, 0x28, alpha);
    }
    DrawTabPlate(alpha);
    DrawScrollBar(alpha);
    MenuTextureReload(g_screen.block_no);
    DrawSprite(kExitX, kExitY, 452, 224, 60, kExitHeight, alpha);

    if (g_screen.step == OPTION_STEP_RUN) {
        // The brackets go round whatever a click would act on.
        Glyph glyph = g_screen.pointing ? g_screen.glyph : Glyph::None;
        if (glyph == Glyph::L1 || glyph == Glyph::R1) {
            int x = glyph == Glyph::L1 ? L1X() : R1X();
            DrawCursor(x - 10, x + (glyph == Glyph::L1 ? texts.l1 : texts.r1).Width() - 6, kTabY - 6, alpha);
        } else if (glyph != Glyph::None) {
            int y = glyph == Glyph::Up ? kRowY - 4 : kRowY + kVisibleRows * kRowStep - 16;
            DrawCursor(kBarX - 12, kBarX + 4, y - 13, alpha);
        } else if (g_screen.row == kOnTabs) {
            int x = TabX(g_screen.tab);
            DrawCursor(x - 10, x + texts.tabs[g_screen.tab].Width() - 6, kTabY - 6, alpha, L1X() - 44);
        } else if (g_screen.row < rows) {
            DrawCursor(kValueX - 22, kValueRight - 2, RowTop(g_screen.row) - 7, alpha, kLabelX - 48);
        } else {
            DrawCursor(kExitX - 8, kExitX + 52, kExitY - 4, alpha);
        }
    }

    if (g_screen.step != OPTION_STEP_RUN) {
        g_screen.step_count++;
    } else {
        g_screen.step_count = 0;
    }

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        float win_x;
        float win_y;
        float win_w;
        float win_h;
        int   text_x;
        int   text_y;
        GetMainMenuRightHelpWinLangOffset(win_x, win_y, win_w, win_h);
        MenuHelpWinDraw((int) win_x, (int) win_y, win_w, win_h, alpha);
        GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
        CommonMenuMes2.edge_alpha = alpha;
        MenuTextureReload(CommonMenuMes2.tex_block);
        DrawMenuClsMes(&CommonMenuMes2, (int) (win_x + text_x), (int) (win_y + text_y));
    }

    MenuTextureReload(0x1A);
    texts.shortcuts.Draw(kExitX, kExitY + 44, alpha);

    // The page shown bright and gold, the others dimmed as the weapon menu dims its other pages.
    texts.l1.Draw(L1X(), kTabY, alpha);
    texts.r1.Draw(R1X(), kTabY, alpha);
    for (int t = 0; t < kTabCount; ++t) {
        bool shown = t == g_screen.tab;
        texts.tabs[t].SetColour(shown ? FONT_COLOR_GOLD : FONT_COLOR_WHITE);
        texts.tabs[t].Draw(TabX(t), kTabY, shown ? alpha : alpha / 2);
    }

    for (int r = first; r < last; ++r) {
        const Row &row = tab.rows[r];
        int        y = RowTop(r) + 2;
        bool       selected = g_screen.row == r;
        int        choice = row.get(config);
        texts.labels[index + r].Draw(kLabelX, y, alpha);
        GameText   &value = texts.values[index + r];
        std::string text = row.text != nullptr ? row.text(config) : ChoiceName(row.names, choice);
        if (ConfigAppliesOnRestart(row.key)) {
            text += " *";
        }
        value.Set(text);
        value.SetColour(selected ? FONT_COLOR_YELLOW : FONT_COLOR_WHITE);
        value.Draw((kValueX + kValueRight - value.Width()) / 2, y, alpha);
        if (selected) {
            if (choice > 0) {
                texts.left.Draw(kValueX + 8, y, alpha);
            }
            if (choice < row.count(config) - 1) {
                texts.right.Draw(kValueRight - texts.right.Width(), y, alpha);
            }
        }
    }

    setbilinear(1);
}

PC_OVERRIDE int OptionMenuFadeOutStart() {
    return g_screen.step == OPTION_STEP_FADE_OUT;
}
