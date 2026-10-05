#include "config.hpp"

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

#include <nlohmann/json.hpp>

#include "paths.hpp"

namespace {

Config g_config;

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

bool Apply(Config &config, std::string_view name, const Json &value) {
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
    if (name == "input.mouse_invert_y") {
        return ReadBool(value, config.mouse_invert_y);
    }
    if (name == "input.mouse_capture") {
        return ReadBool(value, config.mouse_capture);
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
    if (name == "game.debug_mode") {
        return ReadBool(value, config.debug_mode);
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
    if (name == "video.detail_distance") {
        float distance = 0.0f;
        if (!ReadNumber(value, distance) || !(distance >= 0.0f) || !std::isfinite(distance)) {
            return false;
        }
        config.detail_distance = distance;
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
    root["video"]["present_mode"] = PresentModeName(config.present_mode);
    root["video"]["interpolation"] = config.interpolation;
    root["video"]["max_fps"] = config.max_fps;
    root["video"]["width"] = config.window_width;
    root["video"]["height"] = config.window_height;
    root["video"]["fullscreen"] = config.fullscreen;
    root["video"]["aspect"] = config.aspect == ConfigAspect::FourThree ? "4:3" : "auto";
    root["video"]["ui_scale"] = Shortest(config.ui_scale);
    root["video"]["show_fps"] = config.show_fps;
    root["video"]["detail_distance"] = Shortest(config.detail_distance);
    root["video"]["shadow_distance"] = Shortest(config.shadow_distance);
    root["audio"]["master_volume"] = Shortest(config.master_volume);
    root["input"]["mouse_sensitivity"] = Shortest(config.mouse_sensitivity);
    root["input"]["stick_sensitivity"] = Shortest(config.stick_sensitivity);
    root["input"]["mouse_invert_y"] = config.mouse_invert_y;
    root["input"]["mouse_capture"] = config.mouse_capture;
    root["input"]["mouse_release"] = config.mouse_release_keys;
    root["input"]["bindings"] = std::move(bindings);
    return root.dump(4) + "\n";
}

bool ConfigSave() {
    std::filesystem::path path = PathsSaveRoot() / "config.json";
    std::error_code       error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << ConfigSerialize(g_config);
    file.flush();
    if (!file) {
        std::fprintf(stderr, "config: could not save %s\n", path.string().c_str());
        return false;
    }
    std::fprintf(stderr, "config: saved %s\n", path.string().c_str());
    return true;
}

bool ConfigLoad() {
    std::filesystem::path path = PathsSaveRoot() / "config.json";
    std::ifstream         file(path, std::ios::binary);
    if (!file) {
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
    std::fprintf(stderr, "config: loaded %s\n", path.string().c_str());
    return true;
}
