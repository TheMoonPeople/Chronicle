#include "config.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "paths.hpp"

namespace {

Config                        g_config;
std::vector<ConfigChangeHook> g_change_hooks;
// Whether config.json holds g_config, so a failed save is retried with the same settings.
bool g_saved = false;
bool g_changing = false;

std::string_view Trim(std::string_view text) {
    constexpr std::string_view kSpace = " \t\r\n";
    std::size_t                first = text.find_first_not_of(kSpace);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(kSpace) - first + 1);
}

std::string Lower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

using Json = nlohmann::ordered_json;

template <class T>
bool ReadNumber(const Json &value, T &out) {
    if (!value.is_number()) {
        return false;
    }
    out = value.get<T>();
    return true;
}

bool ReadBool(const Json &value, bool &out) {
    if (!value.is_boolean()) {
        return false;
    }
    out = value.get<bool>();
    return true;
}

bool ReadPresentMode(const Json &value, ConfigPresentMode &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string mode = Lower(value.get<std::string>());
    if (mode == "fifo") {
        out = ConfigPresentMode::Fifo;
    } else if (mode == "mailbox") {
        out = ConfigPresentMode::Mailbox;
    } else if (mode == "immediate") {
        out = ConfigPresentMode::Immediate;
    } else {
        return false;
    }
    return true;
}

const char *PresentModeName(ConfigPresentMode mode) {
    switch (mode) {
        case ConfigPresentMode::Mailbox:
            return "mailbox";
        case ConfigPresentMode::Immediate:
            return "immediate";
        default:
            return "fifo";
    }
}

bool ReadFpsDetail(const Json &value, ConfigFpsDetail &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string detail = Lower(value.get<std::string>());
    if (detail == "fps") {
        out = ConfigFpsDetail::Fps;
    } else if (detail == "ticks") {
        out = ConfigFpsDetail::Ticks;
    } else if (detail == "all") {
        out = ConfigFpsDetail::All;
    } else {
        return false;
    }
    return true;
}

const char *FpsDetailName(ConfigFpsDetail detail) {
    switch (detail) {
        case ConfigFpsDetail::Fps:
            return "fps";
        case ConfigFpsDetail::Ticks:
            return "ticks";
        default:
            return "all";
    }
}

// The double that prints as the float does: 0.1f as 0.1, not 0.10000000149011612.
double Shortest(float value) {
    return std::stod(std::format("{}", value));
}

bool ReadAspect(const Json &value, ConfigAspect &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string aspect = Lower(value.get<std::string>());
    if (aspect == "auto") {
        out = ConfigAspect::Auto;
    } else if (aspect == "4:3") {
        out = ConfigAspect::FourThree;
    } else {
        return false;
    }
    return true;
}

constexpr const char *kGlyphDeviceNames[] = {"auto", "ps3", "ps4", "ps5", "xbox", "switch", "steamdeck", "steamcontroller", "keyboard"};

bool ReadGlyphDevice(const Json &value, ConfigGlyphDevice &out) {
    if (!value.is_string()) {
        return false;
    }
    const std::string &name = value.get_ref<const std::string &>();
    for (std::size_t i = 0; i < std::size(kGlyphDeviceNames); ++i) {
        if (name == kGlyphDeviceNames[i]) {
            out = static_cast<ConfigGlyphDevice>(i);
            return true;
        }
    }
    return false;
}

constexpr const char *kGyroNames[] = {"off", "always", "first_person", "held"};

bool ReadGyro(const Json &value, ConfigGyro &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string gyro = Lower(value.get<std::string>());
    for (std::size_t i = 0; i < std::size(kGyroNames); ++i) {
        if (gyro == kGyroNames[i]) {
            out = static_cast<ConfigGyro>(i);
            return true;
        }
    }
    return false;
}

constexpr const char *kLanguageNames[] = {"ask", "", "english", "francais", "deutsch", "italiano", "espanol"};

// "ask" or a language's name, as config.json spells them, becoming LanguageCode 0 or 2 to 6.
bool ReadLanguage(const Json &value, int &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string language = Lower(value.get<std::string>());
    for (std::size_t i = 0; i < std::size(kLanguageNames); ++i) {
        if (kLanguageNames[i][0] != '\0' && language == kLanguageNames[i]) {
            out = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool ReadList(const Json &value, std::vector<std::string> &out) {
    std::vector<std::string> items;
    if (value.is_string()) {
        items.push_back(value.get<std::string>());
    } else if (value.is_array()) {
        for (const Json &item : value) {
            if (!item.is_string()) {
                return false;
            }
            items.push_back(item.get<std::string>());
        }
    } else {
        return false;
    }
    out = std::move(items);
    return true;
}

void Ignore(std::string_view name, const Json &value) {
    std::fprintf(stderr, "config.json: ignoring %.*s = %s\n", static_cast<int>(name.size()), name.data(),
                 value.dump().c_str());
}

bool ApplyBindings(Config &config, const Json &bindings) {
    if (!bindings.is_object()) {
        return false;
    }
    for (const auto &[action, keys] : bindings.items()) {
        ConfigKeyBinding binding = {Lower(action), {}};
        if (ReadList(keys, binding.keys)) {
            config.key_bindings.push_back(std::move(binding));
        } else {
            Ignore("input.bindings." + action, keys);
        }
    }
    return true;
}

bool ReadChoice(const Json &value, std::string_view first, std::string_view second, bool &out) {
    if (!value.is_string()) {
        return false;
    }
    std::string choice = Lower(value.get<std::string>());
    if (choice != first && choice != second) {
        return false;
    }
    out = choice == second;
    return true;
}

// The game's own options, each in the section of the Options screen's page that shows it.
bool ApplyGameOption(ConfigGameOptions &options, std::string_view name, const Json &value) {
    if (name == "game.save_cursor_position") {
        return ReadBool(value, options.save_cursor_position);
    }
    if (name == "input.vibration") {
        return ReadBool(value, options.vibration);
    }
    if (name == "game.message_speed") {
        return ReadChoice(value, "normal", "fast", options.fast_messages);
    }
    if (name == "audio.sound") {
        bool mono = false;
        if (!ReadChoice(value, "stereo", "mono", mono)) {
            return false;
        }
        options.stereo = !mono;
        return true;
    }
    if (name == "game.clock") {
        return ReadBool(value, options.clock);
    }
    if (name == "game.time_speed") {
        return ReadChoice(value, "normal", "fast", options.fast_time);
    }
    if (name == "game.map") {
        if (!value.is_number_integer() || value.get<std::int64_t>() < 0 || value.get<std::int64_t>() > 3) {
            return false;
        }
        options.map = value.get<int>();
        return true;
    }
    if (name == "game.enemy_damage") {
        return ReadBool(value, options.enemy_damage);
    }
    if (name == "game.player_damage") {
        return ReadBool(value, options.player_damage);
    }
    if (name == "game.enemy_hp") {
        return ReadBool(value, options.enemy_hp);
    }
    if (name == "game.names") {
        return ReadBool(value, options.names);
    }
    if (name == "video.soft_focus") {
        return ReadBool(value, options.soft_focus);
    }
    return false;
}

bool Apply(Config &config, std::string_view name, const Json &value) {
    if (name == "audio.sound" && value == "surround") {
        config.options.stereo = true;
        config.surround = true;
        return true;
    }
    if (ApplyGameOption(config.options, name, value)) {
        return true;
    }
    if (name == "input.mouse_sensitivity") {
        float sensitivity = 0.0f;
        if (!ReadNumber(value, sensitivity) || !(sensitivity > 0.0f) || !std::isfinite(sensitivity)) {
            return false;
        }
        config.mouse_sensitivity = sensitivity;
        return true;
    }
    if (name == "input.stick_sensitivity") {
        float sensitivity = 0.0f;
        if (!ReadNumber(value, sensitivity) || !(sensitivity > 0.0f) || !std::isfinite(sensitivity)) {
            return false;
        }
        config.stick_sensitivity = sensitivity;
        return true;
    }
    if (name == "input.gyro") {
        return ReadGyro(value, config.gyro);
    }
    if (name == "input.stick_invert_x") {
        return ReadBool(value, config.stick_invert_x);
    }
    if (name == "input.stick_invert_y") {
        return ReadBool(value, config.stick_invert_y);
    }
    if (name == "input.gyro_invert_x") {
        return ReadBool(value, config.gyro_invert_x);
    }
    if (name == "input.gyro_invert_y") {
        return ReadBool(value, config.gyro_invert_y);
    }
    if (name == "input.gyro_sensitivity") {
        float sensitivity = 0.0f;
        if (!ReadNumber(value, sensitivity) || !(sensitivity > 0.0f) || !std::isfinite(sensitivity)) {
            return false;
        }
        config.gyro_sensitivity = sensitivity;
        return true;
    }
    if (name == "input.mouse_invert_y") {
        return ReadBool(value, config.mouse_invert_y);
    }
    if (name == "input.mouse_capture") {
        return ReadBool(value, config.mouse_capture);
    }
    if (name == "input.touchpad") {
        return ReadBool(value, config.touchpad);
    }
    if (name == "input.lightbar") {
        return ReadBool(value, config.lightbar);
    }
    if (name == "input.touchpad_sensitivity") {
        float sensitivity = 0.0f;
        if (!ReadNumber(value, sensitivity) || !(sensitivity > 0.0f) || !std::isfinite(sensitivity)) {
            return false;
        }
        config.touchpad_sensitivity = sensitivity;
        return true;
    }
    if (name == "input.rumble_strength") {
        float strength = 0.0f;
        if (!ReadNumber(value, strength) || !std::isfinite(strength) || strength < 0.0f || strength > 1.0f) {
            return false;
        }
        config.rumble_strength = strength;
        return true;
    }
    if (name == "video.text_font") {
        if (!value.is_string() || (value.get<std::string>() != "sharp" && value.get<std::string>() != "original")) {
            return false;
        }
        config.font_sharp = value.get<std::string>() == "sharp";
        return true;
    }
    if (name == "input.glyphs") {
        if (!value.is_string() || (value.get<std::string>() != "new" && value.get<std::string>() != "original")) {
            return false;
        }
        config.glyphs_new = value.get<std::string>() == "new";
        return true;
    }
    if (name == "input.glyph_device") {
        return ReadGlyphDevice(value, config.glyph_device);
    }
    if (name == "input.mouse_zoom") {
        return ReadBool(value, config.mouse_zoom);
    }
    if (name == "input.mouse_camera_return") {
        float rate = 0.0f;
        if (!ReadNumber(value, rate) || !std::isfinite(rate) || rate < 0.0f || rate > 1.0f) {
            return false;
        }
        config.mouse_camera_return = rate;
        return true;
    }
    if (name == "input.mouse_release") {
        return ReadList(value, config.mouse_release_keys);
    }
    if (name == "input.bindings") {
        return ApplyBindings(config, value);
    }
    if (name == "game.tick_rate") {
        double rate = 0.0;
        if (!ReadNumber(value, rate) || !(rate > 0.0) || !std::isfinite(rate)) {
            return false;
        }
        config.tick_rate = rate;
        return true;
    }
    if (name == "game.qte_always_win") {
        return ReadBool(value, config.qte_always_win);
    }
    if (name == "game.element_quick_select") {
        return ReadBool(value, config.element_quick_select);
    }
    if (name == "game.debug_mode") {
        return ReadBool(value, config.debug_mode);
    }
    if (name == "video.text_shadow" || name == "video.glyph_shadow" || name == "video.name_shadow" ||
        name == "video.floor_shadow" || name == "video.boss_shadow") {
        int &target = name == "video.text_shadow"    ? config.text_shadow
                      : name == "video.glyph_shadow" ? config.glyph_shadow
                      : name == "video.name_shadow"  ? config.name_shadow
                      : name == "video.floor_shadow" ? config.floor_shadow
                                                     : config.boss_shadow;
        if (value.is_string()) {
            // the earlier two-choice setting
            const std::string shadow = Lower(value.get<std::string>());
            if (shadow != "soft" && shadow != "deep") {
                return false;
            }
            target = shadow == "deep" ? 100 : 50;
            return true;
        }
        double percent = 0.0;
        if (!ReadNumber(value, percent) || !std::isfinite(percent)) {
            return false;
        }
        target = std::clamp(static_cast<int>(std::lround(percent / 5.0)) * 5, 0, 100);
        return true;
    }
    if (name == "game.language") {
        return ReadLanguage(value, config.language);
    }
    if (name == "video.present_mode") {
        return ReadPresentMode(value, config.present_mode);
    }
    if (name == "video.interpolation") {
        return ReadBool(value, config.interpolation);
    }
    if (name == "video.max_fps") {
        double fps = 0.0;
        if (!ReadNumber(value, fps) || !(fps >= 0.0) || !std::isfinite(fps)) {
            return false;
        }
        config.max_fps = fps;
        return true;
    }
    if (name == "video.vsync") {
        bool vsync = true;
        if (!ReadBool(value, vsync)) {
            return false;
        }
        config.present_mode = vsync ? ConfigPresentMode::Fifo : ConfigPresentMode::Immediate;
        return true;
    }
    if (name == "video.width" || name == "video.height") {
        if (!value.is_number_integer() || value.get<std::int64_t>() < 0 ||
            value.get<std::int64_t>() > std::numeric_limits<int>::max()) {
            return false;
        }
        (name == "video.width" ? config.window_width : config.window_height) = value.get<int>();
        return true;
    }
    if (name == "video.fullscreen") {
        return ReadBool(value, config.fullscreen);
    }
    if (name == "video.aspect") {
        return ReadAspect(value, config.aspect);
    }
    if (name == "video.ui_scale") {
        float scale = 0.0f;
        if (!ReadNumber(value, scale) || !(scale >= 0.25f && scale <= 4.0f)) {
            return false;
        }
        config.ui_scale = scale;
        return true;
    }
    if (name == "video.show_fps") {
        return ReadBool(value, config.show_fps);
    }
    if (name == "video.fps_detail") {
        return ReadFpsDetail(value, config.fps_detail);
    }
    if (name == "video.detail_distance") {
        float distance = 0.0f;
        if (!ReadNumber(value, distance) || !(distance >= 0.0f) || !std::isfinite(distance)) {
            return false;
        }
        config.detail_distance = distance;
        return true;
    }
    if (name == "video.anisotropy") {
        if (!value.is_number_integer()) {
            return false;
        }
        int samples = value.get<int>();
        if (samples != 0 && samples != 2 && samples != 4 && samples != 8 && samples != 16) {
            return false;
        }
        config.anisotropy = samples;
        return true;
    }
    if (name == "video.shadow_distance") {
        float distance = 0.0f;
        if (!ReadNumber(value, distance) || !(distance >= 0.0f) || !std::isfinite(distance)) {
            return false;
        }
        config.shadow_distance = distance;
        return true;
    }
    if (name == "discord.rich_presence") {
        return ReadBool(value, config.discord_rich_presence);
    }
    if (name == "audio.soundtrack") {
        return ReadChoice(value, "ps2", "custom", config.soundtrack);
    }
    // Written before audio.sound took "surround".
    if (name == "audio.surround") {
        return ReadBool(value, config.surround);
    }
    if (name == "audio.master_volume") {
        float volume = 0.0f;
        if (!ReadNumber(value, volume) || !std::isfinite(volume)) {
            return false;
        }
        config.master_volume = std::clamp(volume, 0.0f, 1.0f);
        return true;
    }
    return false;
}

bool WriteReplacing(const std::filesystem::path &path, std::string_view text) {
    std::filesystem::path temp = path;
    temp += ".tmp";
    std::error_code error;
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file << text;
        file.close();
        if (!file) {
            std::filesystem::remove(temp, error);
            return false;
        }
    }
    std::filesystem::rename(temp, path, error);
    if (error) {
        std::filesystem::remove(temp, error);
        return false;
    }
    return true;
}

} // namespace

const Config &ConfigGet() {
    return g_config;
}

float ConfigDetailDistance() {
    float distance = g_config.detail_distance;
    return distance > 0.0f ? distance : std::numeric_limits<float>::infinity();
}

float ConfigShadowDistance() {
    float distance = g_config.shadow_distance;
    return distance > 0.0f ? distance : std::numeric_limits<float>::infinity();
}

Config ConfigParse(std::string_view text) {
    Config config;
    if (Trim(text).empty()) {
        return config;
    }
    Json root = Json::parse(text, nullptr, false, true);
    if (!root.is_object()) {
        std::fprintf(stderr, "config.json: %s; using the defaults\n",
                     root.is_discarded() ? "not valid JSON" : "expected an object");
        return config;
    }
    for (const auto &[section, keys] : root.items()) {
        if (!keys.is_object()) {
            Ignore(section, keys);
            continue;
        }
        for (const auto &[key, value] : keys.items()) {
            std::string name = section + "." + key;
            if (!Apply(config, name, value)) {
                Ignore(name, value);
            }
        }
    }
    return config;
}

std::string ConfigSerialize(const Config &config) {
    Json bindings = Json::object();
    for (const ConfigKeyBinding &binding : config.key_bindings) {
        bindings[binding.action] = binding.keys;
    }
    Json root;
    root["game"]["tick_rate"] = config.tick_rate;
    root["game"]["debug_mode"] = config.debug_mode;
    root["game"]["qte_always_win"] = config.qte_always_win;
    root["game"]["element_quick_select"] = config.element_quick_select;
    root["game"]["language"] = kLanguageNames[config.language >= 2 && config.language <= 6 ? config.language : 0];
    const ConfigGameOptions &options = config.options;
    root["game"]["save_cursor_position"] = options.save_cursor_position;
    root["game"]["message_speed"] = options.fast_messages ? "fast" : "normal";
    root["game"]["clock"] = options.clock;
    root["game"]["time_speed"] = options.fast_time ? "fast" : "normal";
    root["game"]["map"] = options.map;
    root["game"]["enemy_damage"] = options.enemy_damage;
    root["game"]["player_damage"] = options.player_damage;
    root["game"]["enemy_hp"] = options.enemy_hp;
    root["game"]["names"] = options.names;
    root["video"]["text_font"] = config.font_sharp ? "sharp" : "original";
    root["video"]["text_shadow"] = config.text_shadow;
    root["video"]["glyph_shadow"] = config.glyph_shadow;
    root["video"]["name_shadow"] = config.name_shadow;
    root["video"]["floor_shadow"] = config.floor_shadow;
    root["video"]["boss_shadow"] = config.boss_shadow;
    root["video"]["present_mode"] = PresentModeName(config.present_mode);
    root["video"]["interpolation"] = config.interpolation;
    root["video"]["max_fps"] = config.max_fps;
    root["video"]["width"] = config.window_width;
    root["video"]["height"] = config.window_height;
    root["video"]["fullscreen"] = config.fullscreen;
    root["video"]["aspect"] = config.aspect == ConfigAspect::FourThree ? "4:3" : "auto";
    root["video"]["ui_scale"] = Shortest(config.ui_scale);
    root["video"]["show_fps"] = config.show_fps;
    root["video"]["fps_detail"] = FpsDetailName(config.fps_detail);
    root["video"]["detail_distance"] = Shortest(config.detail_distance);
    root["video"]["shadow_distance"] = Shortest(config.shadow_distance);
    root["video"]["anisotropy"] = config.anisotropy;
    root["video"]["soft_focus"] = options.soft_focus;
    root["audio"]["master_volume"] = Shortest(config.master_volume);
    root["audio"]["sound"] = config.surround ? "surround" : options.stereo ? "stereo" : "mono";
    root["audio"]["soundtrack"] = config.soundtrack ? "custom" : "ps2";
    root["input"]["mouse_sensitivity"] = Shortest(config.mouse_sensitivity);
    root["input"]["stick_sensitivity"] = Shortest(config.stick_sensitivity);
    root["input"]["stick_invert_x"] = config.stick_invert_x;
    root["input"]["stick_invert_y"] = config.stick_invert_y;
    root["input"]["gyro"] = kGyroNames[static_cast<std::size_t>(config.gyro)];
    root["input"]["gyro_sensitivity"] = Shortest(config.gyro_sensitivity);
    root["input"]["gyro_invert_x"] = config.gyro_invert_x;
    root["input"]["gyro_invert_y"] = config.gyro_invert_y;
    root["input"]["mouse_invert_y"] = config.mouse_invert_y;
    root["input"]["mouse_capture"] = config.mouse_capture;
    root["input"]["mouse_zoom"] = config.mouse_zoom;
    root["input"]["touchpad"] = config.touchpad;
    root["input"]["touchpad_sensitivity"] = Shortest(config.touchpad_sensitivity);
    root["input"]["lightbar"] = config.lightbar;
    root["input"]["rumble_strength"] = Shortest(config.rumble_strength);
    root["input"]["glyphs"] = config.glyphs_new ? "new" : "original";
    root["input"]["glyph_device"] = kGlyphDeviceNames[static_cast<std::size_t>(config.glyph_device)];
    root["input"]["mouse_camera_return"] = Shortest(config.mouse_camera_return);
    root["input"]["mouse_release"] = config.mouse_release_keys;
    root["input"]["vibration"] = options.vibration;
    root["input"]["bindings"] = std::move(bindings);
    root["discord"]["rich_presence"] = config.discord_rich_presence;
    return root.dump(4) + "\n";
}

bool ConfigSave() {
    std::filesystem::path path = PathsSaveRoot() / "config.json";
    std::error_code       error;
    std::filesystem::create_directories(path.parent_path(), error);
    g_saved = WriteReplacing(path, ConfigSerialize(g_config));
    std::fprintf(stderr, "config: %s %s\n", g_saved ? "saved" : "could not save", PathsDisplay(path).c_str());
    return g_saved;
}

bool ConfigLoad() {
    std::filesystem::path path = PathsSaveRoot() / "config.json";
    std::ifstream         file(path, std::ios::binary);
    std::error_code       error;
    if (!file || !std::filesystem::is_regular_file(path, error)) {
        g_config = Config{};
        if (std::filesystem::exists(PathsSaveRoot() / "config.ini")) {
            std::fprintf(stderr, "config.ini is no longer read: move its settings to config.json (docs/PC.md)\n");
        }
        ConfigSave();
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    g_config = ConfigParse(text.str());
    g_saved = true;
    std::fprintf(stderr, "config: loaded %s\n", PathsDisplay(path).c_str());
    return true;
}

void ConfigAddChangeHook(ConfigChangeHook hook) {
    if (std::ranges::find(g_change_hooks, hook) == g_change_hooks.end()) {
        g_change_hooks.push_back(hook);
    }
}

void ConfigRemoveChangeHook(ConfigChangeHook hook) {
    std::erase(g_change_hooks, hook);
}

bool ConfigChange(const Config &config) {
    if (g_changing) {
        std::fprintf(stderr, "config: ignoring a change made while one is being applied\n");
        return false;
    }

    struct Guard {
        Guard() { g_changing = true; }

        ~Guard() { g_changing = false; }
    } guard;

    Config after = ConfigParse(ConfigSerialize(config));
    if (after == g_config) {
        return g_saved || ConfigSave();
    }
    Config before = std::exchange(g_config, std::move(after));
    ConfigSave();
    const std::vector<ConfigChangeHook> hooks = g_change_hooks;
    for (ConfigChangeHook hook : hooks) {
        hook(before, g_config);
    }
    return g_saved;
}

bool ConfigAppliesOnRestart(std::string_view key) {
    return key == "game.debug_mode";
}
