#include "input_script.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

#include "paths.hpp"

namespace {

struct ButtonName {
    std::string_view name;
    std::uint16_t    bit;
};

constexpr ButtonName kButtonNames[] = {
    {"cross",    kInputCross   },
    {"circle",   kInputCircle  },
    {"square",   kInputSquare  },
    {"triangle", kInputTriangle},
    {"start",    kInputStart   },
    {"select",   kInputSelect  },
    {"l1",       kInputL1      },
    {"r1",       kInputR1      },
    {"l2",       kInputL2      },
    {"r2",       kInputR2      },
    {"l3",       kInputL3      },
    {"r3",       kInputR3      },
    {"up",       kInputUp      },
    {"down",     kInputDown    },
    {"left",     kInputLeft    },
    {"right",    kInputRight   },
};

InputScript g_script;
bool        g_active = false;

InputPadState Released() {
    InputPadState state;
    state.connected = true;
    return state;
}

template <class T>
bool ParseNumber(std::string_view token, T &value) {
    auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    return error == std::errc() && end == token.data() + token.size();
}

std::vector<std::string_view> Tokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t                   at = 0;
    while (at < line.size()) {
        while (at < line.size() && (line[at] == ' ' || line[at] == '\t' || line[at] == '\r')) {
            ++at;
        }
        std::size_t start = at;
        while (at < line.size() && line[at] != ' ' && line[at] != '\t' && line[at] != '\r') {
            ++at;
        }
        if (at > start) {
            tokens.push_back(line.substr(start, at - start));
        }
    }
    return tokens;
}

bool ParseDeviceToken(std::string_view token, std::string_view lower, InputScriptStep &step, bool &device,
                      std::string &why) {
    device = true;
    if (lower.starts_with("key:")) {
        int scancode = InputScancodeFromName(token.substr(4));
        if (scancode < 0) {
            why = "unknown key \"" + std::string(token.substr(4)) + "\"";
            return false;
        }
        step.devices.keys.push_back(scancode);
        return true;
    }
    if (lower.starts_with("mouse:")) {
        std::string_view motion = lower.substr(6);
        std::size_t      comma = motion.find(',');
        if (comma == std::string_view::npos || !ParseNumber(motion.substr(0, comma), step.devices.mouse_dx) ||
            !ParseNumber(motion.substr(comma + 1), step.devices.mouse_dy)) {
            why = "mouse motion is mouse:dx,dy";
            return false;
        }
        return true;
    }
    if (lower.starts_with("wheel:")) {
        if (!ParseNumber(lower.substr(6), step.devices.mouse_wheel) || !std::isfinite(step.devices.mouse_wheel)) {
            why = "mouse wheel is wheel:number (finite notches)";
            return false;
        }
        return true;
    }
    if (lower.size() == 6 && lower.starts_with("mouse") && lower[5] >= '1' && lower[5] <= '5') {
        step.devices.mouse_buttons |= 1u << (lower[5] - '1');
        return true;
    }
    device = false;
    return true;
}

bool ParseLine(std::string_view line, InputScriptStep &step, std::string &why) {
    std::vector<std::string_view> tokens = Tokens(line);
    if (!ParseNumber(tokens[0], step.frame) || step.frame < 0) {
        why = "the frame is not a non-negative number";
        return false;
    }
    step.state = Released();
    std::size_t i = 1;
    if (i < tokens.size() && (tokens[i] == "pad1" || tokens[i] == "pad2")) {
        step.pad = tokens[i] == "pad2" ? 1 : 0;
        ++i;
    }
    for (; i < tokens.size(); ++i) {
        unsigned axis = 0;
        if (ParseNumber(tokens[i], axis)) {
            break;
        }
        std::string lower(tokens[i]);
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });
        bool device = false;
        if (!ParseDeviceToken(tokens[i], lower, step, device, why)) {
            return false;
        }
        if (device) {
            if (step.pad != 0) {
                why = "keys and the mouse drive pad 1 only";
                return false;
            }
            step.uses_devices = true;
            continue;
        }
        auto found = std::ranges::find(kButtonNames, std::string_view(lower), &ButtonName::name);
        if (found == std::end(kButtonNames)) {
            why = "unknown button \"" + std::string(tokens[i]) + "\"";
            return false;
        }
        step.state.buttons |= found->bit;
    }
    if (i == tokens.size()) {
        return true;
    }
    if (tokens.size() - i != 4) {
        why = "the sticks take four numbers (lx ly rx ry)";
        return false;
    }
    std::uint8_t *axes[] = {&step.state.left_x, &step.state.left_y, &step.state.right_x, &step.state.right_y};
    for (int axis = 0; axis < 4; ++axis) {
        unsigned value = 0;
        if (!ParseNumber(tokens[i + axis], value) || value > 255) {
            why = "a stick value is not 0 to 255";
            return false;
        }
        *axes[axis] = static_cast<std::uint8_t>(value);
    }
    return true;
}

} // namespace

bool InputScriptParse(std::string_view text, InputScript &script, std::string &error) {
    script.steps.clear();
    int number = 0;
    while (!text.empty()) {
        std::size_t      end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        ++number;
        line = line.substr(0, line.find('#'));
        if (Tokens(line).empty()) {
            continue;
        }
        InputScriptStep step;
        std::string     why;
        if (!ParseLine(line, step, why)) {
            error = "line " + std::to_string(number) + ": " + why;
            return false;
        }
        auto same_pad = [&](const InputScriptStep &other) { return other.pad == step.pad; };
        auto previous = std::ranges::find_last_if(script.steps, same_pad);
        if (!previous.empty() && step.frame < previous.front().frame) {
            error = "line " + std::to_string(number) + ": frames must not decrease";
            return false;
        }
        script.steps.push_back(step);
    }
    return true;
}

bool InputScriptLoad(const std::filesystem::path &path, InputScript &script, std::string &error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot read " + PathsDisplay(path);
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();
    if (!InputScriptParse(text.str(), script, error)) {
        error = PathsDisplay(path) + ": " + error;
        return false;
    }
    return true;
}

InputPadState InputScriptStateAt(const InputScript &script, int pad, std::int64_t frame) {
    const InputScriptStep *held = nullptr;
    for (const InputScriptStep &step : script.steps) {
        if (step.pad == pad && step.frame <= frame) {
            held = &step;
        }
    }
    if (held == nullptr) {
        return Released();
    }
    return held->uses_devices ? InputApplyKeyboardMouse(held->state, held->devices) : held->state;
}

InputKeyboardMouse InputScriptDevicesAt(const InputScript &script, std::int64_t frame) {
    const InputScriptStep *held = nullptr;
    for (const InputScriptStep &step : script.steps) {
        if (step.pad == 0 && step.frame <= frame) {
            held = &step;
        }
    }
    return held != nullptr ? held->devices : InputKeyboardMouse{};
}

bool InputScriptDrivesPad(const InputScript &script, int pad) {
    if (pad == 0) {
        return !script.steps.empty();
    }
    return std::ranges::any_of(script.steps, [&](const InputScriptStep &step) { return step.pad == pad; });
}

void InputScriptInstall(InputScript script) {
    g_script = std::move(script);
    g_active = !g_script.steps.empty();
    for (int pad = 0; pad < kInputPadCount; ++pad) {
        InputSetOverride(pad, nullptr);
    }
    InputScriptApply(0);
}

void InputScriptApply(std::int64_t frame) {
    if (!g_active) {
        return;
    }
    for (int pad = 0; pad < kInputPadCount; ++pad) {
        if (InputScriptDrivesPad(g_script, pad)) {
            InputPadState state = InputScriptStateAt(g_script, pad, frame);
            InputSetOverride(pad, &state);
        }
    }
    InputSetScriptedDevices(InputScriptDevicesAt(g_script, frame));
}

bool InputScriptActive() {
    return g_active;
}
