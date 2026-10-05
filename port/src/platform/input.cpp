#include "input.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "clock.hpp"
#include "config.hpp"
#include "mouse.hpp"
#include "window.hpp"

namespace {

enum class ActionKind {
    Button,
    HalfAxis,
    Axis,
    Host,
};

enum Axis {
    kAxisLeftX,
    kAxisLeftY,
    kAxisRightX,
    kAxisRightY,
};

struct Action {
    std::string_view name;
    ActionKind       kind;
    std::uint16_t    button;
    Axis             axis;
    int              direction;
    std::string_view defaults;
};

// The game-side reasons for each default are in docs/PC.md ("Keyboard and mouse").
// clang-format off
constexpr Action kActions[] = {
    {"up",           ActionKind::Button,   kInputUp,       kAxisLeftX,  0,  "Up"},
    {"down",         ActionKind::Button,   kInputDown,     kAxisLeftX,  0,  "Down"},
    {"left",         ActionKind::Button,   kInputLeft,     kAxisLeftX,  0,  "Left"},
    {"right",        ActionKind::Button,   kInputRight,    kAxisLeftX,  0,  "Right"},
    {"cross",        ActionKind::Button,   kInputCross,    kAxisLeftX,  0,  "Mouse1, Space"},
    {"circle",       ActionKind::Button,   kInputCircle,   kAxisLeftX,  0,  "F"},
    {"square",       ActionKind::Button,   kInputSquare,   kAxisLeftX,  0,  "E"},
    {"triangle",     ActionKind::Button,   kInputTriangle, kAxisLeftX,  0,  "Tab"},
    {"l1",           ActionKind::Button,   kInputL1,       kAxisLeftX,  0,  "Z"},
    {"r1",           ActionKind::Button,   kInputR1,       kAxisLeftX,  0,  "Mouse2, X"},
    {"l2",           ActionKind::Button,   kInputL2,       kAxisLeftX,  0,  "Q"},
    {"r2",           ActionKind::Button,   kInputR2,       kAxisLeftX,  0,  "R"},
    {"l3",           ActionKind::Button,   kInputL3,       kAxisLeftX,  0,  "V"},
    {"r3",           ActionKind::Button,   kInputR3,       kAxisLeftX,  0,  "Mouse3, B"},
    {"start",        ActionKind::Button,   kInputStart,    kAxisLeftX,  0,  "Return"},
    {"select",       ActionKind::Button,   kInputSelect,   kAxisLeftX,  0,  "Backspace, C"},
    {"lx-",          ActionKind::HalfAxis, 0,              kAxisLeftX,  -1, "A"},
    {"lx+",          ActionKind::HalfAxis, 0,              kAxisLeftX,  1,  "D"},
    {"ly-",          ActionKind::HalfAxis, 0,              kAxisLeftY,  -1, "W"},
    {"ly+",          ActionKind::HalfAxis, 0,              kAxisLeftY,  1,  "S"},
    {"rx-",          ActionKind::HalfAxis, 0,              kAxisRightX, -1, "J"},
    {"rx+",          ActionKind::HalfAxis, 0,              kAxisRightX, 1,  "L"},
    {"ry-",          ActionKind::HalfAxis, 0,              kAxisRightY, -1, "I"},
    {"ry+",          ActionKind::HalfAxis, 0,              kAxisRightY, 1,  "K"},
    {"lx",           ActionKind::Axis,     0,              kAxisLeftX,  0,  ""},
    {"ly",           ActionKind::Axis,     0,              kAxisLeftY,  0,  ""},
    {"rx",           ActionKind::Axis,     0,              kAxisRightX, 0,  "MouseX"},
    // Mouse up looks up: a negative ry lowers the follow camera (AddHeight(-GetRYf())).
    {"ry",           ActionKind::Axis,     0,              kAxisRightY, 0,  "-MouseY"},
    {"fps_toggle",   ActionKind::Host,     0,              kAxisLeftX,  0,  "F3"},
};

struct ButtonMap {
    SDL_GamepadButton button;
    std::uint16_t     pad;
};

const ButtonMap kGamepadButtons[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH,          kInputCross},
    {SDL_GAMEPAD_BUTTON_EAST,           kInputCircle},
    {SDL_GAMEPAD_BUTTON_WEST,           kInputSquare},
    {SDL_GAMEPAD_BUTTON_NORTH,          kInputTriangle},
    {SDL_GAMEPAD_BUTTON_BACK,           kInputSelect},
    {SDL_GAMEPAD_BUTTON_START,          kInputStart},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK,     kInputL3},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK,    kInputR3},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  kInputL1},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, kInputR1},
    {SDL_GAMEPAD_BUTTON_DPAD_UP,        kInputUp},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN,      kInputDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT,      kInputLeft},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     kInputRight},
};
// clang-format on

constexpr std::size_t kActionCount = std::size(kActions);
constexpr std::size_t kFirstHostAction = kActionCount - 1;
constexpr std::size_t kHostActionCount = kActionCount - kFirstHostAction;
static_assert(kActions[kFirstHostAction].name == "fps_toggle");
static_assert(static_cast<std::size_t>(InputHostAction::FpsToggle) == 0);

// AxisCalibration (ps2/src/gamepad.cpp): a byte within 49 above or 50 below the centre reads as
// zero, and the remaining 78 steps each side span the game's +-128.
constexpr int kDeadZoneAbove = 49;
constexpr int kDeadZoneBelow = 50;
constexpr int kLiveSpan = 78;
constexpr int kGameAxisRange = 128;

// The DualShock 2's L2 and R2 are digital in the mode the game uses.
constexpr Sint16 kTriggerThreshold = SDL_JOYSTICK_AXIS_MAX / 4;

// SDL rumble needs a duration; the game restates its motors every frame
// through scePadSetActDirect, so a short one is refreshed and a hung game
// stops shaking the pad on its own.
constexpr Uint32 kRumbleMilliseconds = 500;

constexpr int kMouseButtonCount = 5;

struct Source {
    enum Kind {
        Key,
        MouseButton,
        MouseAxis,
        GamepadButton,
    };

    Kind  kind = Key;
    int   code = 0;
    float scale = 1.0f;
};

struct PadSlot {
    SDL_Gamepad *gamepad = nullptr;
    InputRumble  rumble;
    bool         rumble_sent = false;
    Uint64       rumble_time = 0;
};

std::array<std::vector<Source>, kActionCount>            g_bindings;
bool                                                     g_bindings_ready = false;
InputMouseSettings                                       g_mouse;
std::array<PadSlot, kInputPadCount>                      g_slots;
std::array<InputPadState, kInputPadCount>                g_device;
std::array<InputPadState, kInputPadCount>                g_state;
std::array<InputPadState, kInputPadCount>                g_view;
std::array<std::optional<InputPadState>, kInputPadCount> g_override;
std::array<bool, SDL_SCANCODE_COUNT>                     g_keys{};
float                                                    g_mouse_dx = 0.0f;
float                                                    g_mouse_dy = 0.0f;
std::int64_t                                             g_last_latch_tick = -1;
std::int64_t                                             g_latch_serial = 0;
std::int64_t                                             g_stick_read_serial = -1;
bool                                                     g_stick_live = false;
bool                                                     g_gamepad_subsystem = false;
InputKeyboardMouse                                       g_scripted;
// Per host action: presses not yet consumed, and whether its non-key sources (mouse and gamepad
// buttons, the script's keys) were held at the last poll, for their press edges.
std::array<int, kHostActionCount>  g_host_presses{};
std::array<bool, kHostActionCount> g_host_polled{};

std::string Lower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

bool ParseSource(std::string_view name, ActionKind kind, Source &source) {
    std::string lower = Lower(Trim(name));
    if (kind == ActionKind::Axis) {
        std::string_view text = lower;
        float            sign = 1.0f;
        if (text.starts_with('-')) {
            sign = -1.0f;
            text = Trim(text.substr(1));
        }
        float       scale = 1.0f;
        std::size_t star = text.find('*');
        if (star != std::string_view::npos) {
            std::string_view factor = Trim(text.substr(star + 1));
            auto [end, error] = std::from_chars(factor.data(), factor.data() + factor.size(), scale);
            if (error != std::errc{} || end != factor.data() + factor.size() || !std::isfinite(scale)) {
                return false;
            }
            text = Trim(text.substr(0, star));
        }
        if (text != "mousex" && text != "mousey") {
            return false;
        }
        source = {Source::MouseAxis, text == "mousex" ? 0 : 1, sign * scale};
        return true;
    }
    if (kind == ActionKind::Host && lower.starts_with("gamepad:")) {
        std::string       button_name = lower.substr(8);
        SDL_GamepadButton button = SDL_GetGamepadButtonFromString(button_name.c_str());
        if (button == SDL_GAMEPAD_BUTTON_INVALID) {
            return false;
        }
        source = {Source::GamepadButton, static_cast<int>(button), 1.0f};
        return true;
    }
    bool button = lower.size() == 6 && lower.starts_with("mouse");
    if (button && lower[5] >= '1' && lower[5] < '1' + kMouseButtonCount) {
        source = {Source::MouseButton, lower[5] - '0', 1.0f};
        return true;
    }
    int scancode = InputScancodeFromName(Trim(name));
    if (scancode < 0) {
        return false;
    }
    source = {Source::Key, scancode, 1.0f};
    return true;
}

bool ParseSources(std::span<const std::string_view> names, ActionKind kind, std::vector<Source> &sources) {
    sources.clear();
    for (std::string_view name : names) {
        Source source;
        if (!ParseSource(name, kind, source)) {
            return false;
        }
        sources.push_back(source);
    }
    return true;
}

std::vector<std::string_view> SplitDefaults(std::string_view text) {
    std::vector<std::string_view> names;
    while (!text.empty()) {
        std::size_t comma = text.find(',');
        names.push_back(Trim(text.substr(0, comma)));
        text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
    }
    return names;
}

void EnsureBindings() {
    if (g_bindings_ready) {
        return;
    }
    for (std::size_t i = 0; i < kActionCount; ++i) {
        std::vector<std::string_view> names = SplitDefaults(kActions[i].defaults);
        ParseSources(names, kActions[i].kind, g_bindings[i]);
    }
    g_mouse = InputMouseSettings{};
    g_mouse.release_scancodes = {SDL_SCANCODE_ESCAPE};
    MouseConfigure(g_mouse.capture, g_mouse.release_scancodes);
    g_bindings_ready = true;
}

std::uint8_t StickToByte(Sint16 value) {
    return static_cast<std::uint8_t>((static_cast<int>(value) + 32768) >> 8);
}

// The DualShock 2 read full deflection well before its stick's stop and reached its corners on a
// diagonal; a modern stick reports a circle and reaches full only at the stop. The game's thresholds
// expect the former (the town runs past a magnitude of 0.85, after AxisCalibration's dead zone,
// which needs over 90% of a modern stick's travel), so the deflection is scaled by
// input.stick_sensitivity (1.33 by default, PCSX2's analog sensitivity) and the circle is stretched
// onto the square, keeping the direction.
float g_stick_sensitivity = 1.33f;

void StickPairToBytes(Sint16 x, Sint16 y, std::uint8_t &byte_x, std::uint8_t &byte_y) {
    float fx = std::clamp(static_cast<float>(x) / 32767.0f, -1.0f, 1.0f);
    float fy = std::clamp(static_cast<float>(y) / 32767.0f, -1.0f, 1.0f);
    float radius = std::min(1.0f, std::hypot(fx, fy) * g_stick_sensitivity);
    float edge = std::max(std::fabs(fx), std::fabs(fy));
    if (edge > 0.0f) {
        fx *= radius / edge;
        fy *= radius / edge;
    }
    byte_x = StickToByte(static_cast<Sint16>(std::lround(std::clamp(fx, -1.0f, 1.0f) * 32767.0f)));
    byte_y = StickToByte(static_cast<Sint16>(std::lround(std::clamp(fy, -1.0f, 1.0f) * 32767.0f)));
}

bool Deflected(std::uint8_t byte) {
    int offset = static_cast<int>(byte) - kInputAxisCentre;
    return offset > kDeadZoneAbove || offset < -kDeadZoneBelow;
}

std::uint8_t &AxisOf(InputPadState &state, Axis axis) {
    switch (axis) {
        case kAxisLeftX:
            return state.left_x;
        case kAxisLeftY:
            return state.left_y;
        case kAxisRightX:
            return state.right_x;
        case kAxisRightY:
            break;
    }
    return state.right_y;
}

void ReadGamepad(SDL_Gamepad *gamepad, InputPadState &state) {
    for (const ButtonMap &map : kGamepadButtons) {
        if (SDL_GetGamepadButton(gamepad, map.button)) {
            state.buttons |= map.pad;
        }
    }
    if (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold) {
        state.buttons |= kInputL2;
    }
    if (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold) {
        state.buttons |= kInputR2;
    }
    StickPairToBytes(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX),
                     SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY), state.left_x, state.left_y);
    StickPairToBytes(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX),
                     SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY), state.right_x, state.right_y);
}

InputKeyboardMouse LiveKeyboardMouse() {
    InputKeyboardMouse held;
    for (int scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {
        if (g_keys[scancode]) {
            held.keys.push_back(scancode);
        }
    }
    held.mouse_buttons = MouseButtons();
    held.mouse_dx = g_mouse_dx;
    held.mouse_dy = g_mouse_dy;
    return held;
}

void Compose(int pad) {
    g_state[pad] = pad == 0 ? InputApplyKeyboardMouse(g_device[pad], LiveKeyboardMouse()) : g_device[pad];
}

void SyncGamepads() {
    SDL_UpdateGamepads();
    for (PadSlot &slot : g_slots) {
        if (slot.gamepad != nullptr && !SDL_GamepadConnected(slot.gamepad)) {
            SDL_CloseGamepad(slot.gamepad);
            slot = PadSlot{};
        }
    }
    int             count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (ids == nullptr) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        bool open = false;
        for (const PadSlot &slot : g_slots) {
            open |= slot.gamepad != nullptr && SDL_GetGamepadID(slot.gamepad) == ids[i];
        }
        if (open) {
            continue;
        }
        for (PadSlot &slot : g_slots) {
            if (slot.gamepad == nullptr) {
                slot.gamepad = SDL_OpenGamepad(ids[i]);
                if (slot.gamepad == nullptr) {
                    std::fprintf(stderr, "input: SDL_OpenGamepad: %s\n", SDL_GetError());
                }
                break;
            }
        }
    }
    SDL_free(ids);
}

void SendRumble(int pad) {
    PadSlot &slot = g_slots[pad];
    if (slot.gamepad == nullptr) {
        return;
    }
    Uint16 low = static_cast<Uint16>(slot.rumble.large_motor * 257);
    Uint16 high = slot.rumble.small_motor ? 0xFFFF : 0;
    SDL_RumbleGamepad(slot.gamepad, low, high, kRumbleMilliseconds);
    slot.rumble_sent = true;
    slot.rumble_time = SDL_GetTicks();
}

bool GamepadButtonHeld(int button) {
    return std::ranges::any_of(g_slots, [&](const PadSlot &slot) {
        return slot.gamepad != nullptr &&
               SDL_GetGamepadButton(slot.gamepad, static_cast<SDL_GamepadButton>(button));
    });
}

bool KeyBound(std::size_t host, int scancode) {
    return std::ranges::any_of(g_bindings[kFirstHostAction + host], [&](const Source &source) {
        return source.kind == Source::Key && source.code == scancode;
    });
}

// Everything but the live keys, which count their presses as their events arrive.
bool HostPolledHeld(std::size_t host) {
    for (const Source &source : g_bindings[kFirstHostAction + host]) {
        bool held = false;
        switch (source.kind) {
            case Source::Key:
                held = std::ranges::find(g_scripted.keys, source.code) != g_scripted.keys.end();
                break;
            case Source::MouseButton:
                held = ((MouseButtons() | g_scripted.mouse_buttons) & (1u << (source.code - 1))) != 0;
                break;
            case Source::GamepadButton:
                held = GamepadButtonHeld(source.code);
                break;
            case Source::MouseAxis:
                break;
        }
        if (held) {
            return true;
        }
    }
    return false;
}

void PollHostActions() {
    for (std::size_t host = 0; host < kHostActionCount; ++host) {
        bool held = HostPolledHeld(host);
        if (held && !g_host_polled[host]) {
            ++g_host_presses[host];
        }
        g_host_polled[host] = held;
    }
}

} // namespace

void InputInit() {
    EnsureBindings();
    if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        g_gamepad_subsystem = true;
    } else {
        std::fprintf(stderr, "input: no gamepads: %s\n", SDL_GetError());
    }
    const Config &config = ConfigGet();
    for (const ConfigKeyBinding &binding : config.key_bindings) {
        std::vector<std::string_view> names(binding.keys.begin(), binding.keys.end());
        if (!InputBindKeys(binding.action, names)) {
            std::fprintf(stderr, "input: cannot bind %s\n", binding.action.c_str());
        }
    }
    g_stick_sensitivity = config.stick_sensitivity;
    InputMouseSettings mouse;
    mouse.sensitivity = config.mouse_sensitivity;
    mouse.invert_y = config.mouse_invert_y;
    mouse.capture = config.mouse_capture;
    for (const std::string &name : config.mouse_release_keys) {
        int scancode = InputScancodeFromName(name);
        if (scancode < 0) {
            std::fprintf(stderr, "input: unknown mouse_release key %s\n", name.c_str());
            continue;
        }
        mouse.release_scancodes.push_back(scancode);
    }
    InputSetMouseSettings(mouse);
    WindowAddEventHook(InputHandleEvent);
    MouseStart();
}

void InputShutdown() {
    MouseStop();
    g_keys.fill(false);
    g_scripted = {};
    g_host_presses.fill(0);
    g_host_polled.fill(false);
    for (PadSlot &slot : g_slots) {
        if (slot.gamepad != nullptr) {
            SDL_CloseGamepad(slot.gamepad);
        }
        slot = PadSlot{};
    }
    if (g_gamepad_subsystem) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        g_gamepad_subsystem = false;
    }
}

void InputPoll() {
    EnsureBindings();
    if (g_gamepad_subsystem) {
        SyncGamepads();
    }
    for (int pad = 0; pad < kInputPadCount; ++pad) {
        InputPadState state;
        state.connected = pad == 0 || g_slots[pad].gamepad != nullptr;
        if (g_slots[pad].gamepad != nullptr) {
            ReadGamepad(g_slots[pad].gamepad, state);
        }
        g_device[pad] = state;
        Compose(pad);
    }
    PollHostActions();
}

void InputHandleEvent(const SDL_Event &event) {
    if (MouseHandleEvent(event)) {
        return;
    }
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.scancode < SDL_SCANCODE_COUNT) {
                g_keys[event.key.scancode] = event.key.down;
            }
            if (event.key.down && !event.key.repeat) {
                EnsureBindings();
                for (std::size_t host = 0; host < kHostActionCount; ++host) {
                    if (KeyBound(host, event.key.scancode)) {
                        ++g_host_presses[host];
                    }
                }
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            g_keys.fill(false);
            break;
        default:
            break;
    }
}

void InputLatchPad(int pad) {
    if (pad != 0) {
        return;
    }
    g_stick_live = g_stick_read_serial == g_latch_serial;
    ++g_latch_serial;
    // Motion since the previous read, per tick: a frame that spans several ticks (or a load
    // between reads) turns the camera at the mouse's speed instead of all at once, and a mouse that
    // stopped reads as centred at the next read.
    std::int64_t now = ClockTickCount();
    std::int64_t ticks = g_last_latch_tick < 0 ? 1 : std::max<std::int64_t>(1, now - g_last_latch_tick);
    g_last_latch_tick = now;
    float dx = 0.0f;
    float dy = 0.0f;
    MouseTakeMotion(dx, dy);
    g_mouse_dx = dx / static_cast<float>(ticks);
    g_mouse_dy = dy / static_cast<float>(ticks);
    Compose(0);
}

void InputNoteLeftStickRead() { g_stick_read_serial = g_latch_serial; }

bool InputLeftStickLive() { return g_stick_live; }

const InputPadState &InputGetPad(int pad) {
    static const InputPadState kAbsent;
    if (pad < 0 || pad >= kInputPadCount) {
        return kAbsent;
    }
    g_view[pad] = g_override[pad] ? *g_override[pad] : g_state[pad];
    if (!g_stick_live) {
        g_view[pad].buttons |= g_view[pad].stick_dpad;
    }
    return g_view[pad];
}

void InputSetRumble(int pad, InputRumble rumble) {
    if (pad < 0 || pad >= kInputPadCount) {
        return;
    }
    PadSlot &slot = g_slots[pad];
    bool     changed = slot.rumble.small_motor != rumble.small_motor || slot.rumble.large_motor != rumble.large_motor;
    bool     active = rumble.small_motor || rumble.large_motor != 0;
    slot.rumble = rumble;
    if (changed || (active && SDL_GetTicks() - slot.rumble_time >= kRumbleMilliseconds / 2)) {
        SendRumble(pad);
    }
}

InputRumble InputGetRumble(int pad) {
    if (pad < 0 || pad >= kInputPadCount) {
        return {};
    }
    return g_slots[pad].rumble;
}

void InputSetOverride(int pad, const InputPadState *state) {
    if (pad < 0 || pad >= kInputPadCount) {
        return;
    }
    if (state != nullptr) {
        g_override[pad] = *state;
    } else {
        g_override[pad].reset();
    }
}

InputPadState InputApplyKeyboardMouse(InputPadState base, const InputKeyboardMouse &held) {
    EnsureBindings();
    std::array<int, 4>   push{};
    std::array<float, 4> mouse{};
    float                mouse_y = g_mouse.invert_y ? -held.mouse_dy : held.mouse_dy;
    for (std::size_t i = 0; i < kActionCount; ++i) {
        bool down = false;
        for (const Source &source : g_bindings[i]) {
            switch (source.kind) {
                case Source::Key:
                    down |= std::ranges::find(held.keys, source.code) != held.keys.end();
                    break;
                case Source::MouseButton:
                    down |= (held.mouse_buttons & (1u << (source.code - 1))) != 0;
                    break;
                case Source::MouseAxis:
                    mouse[kActions[i].axis] +=
                        source.scale * g_mouse.sensitivity * (source.code == 0 ? held.mouse_dx : mouse_y);
                    break;
                case Source::GamepadButton:
                    break;
            }
        }
        if (!down) {
            continue;
        }
        if (kActions[i].kind == ActionKind::Button) {
            base.buttons |= kActions[i].button;
        } else if (kActions[i].kind == ActionKind::HalfAxis) {
            push[kActions[i].axis] += kActions[i].direction;
        }
    }
    std::array<float, 4> deflection{};
    for (int x = kAxisLeftX; x <= kAxisRightX; x += 2) {
        float key_x = static_cast<float>((push[x] > 0) - (push[x] < 0));
        float key_y = static_cast<float>((push[x + 1] > 0) - (push[x + 1] < 0));
        // Two keys held make a diagonal of the same length as one, as a stick's gate does.
        if (key_x != 0.0f && key_y != 0.0f) {
            key_x *= std::numbers::sqrt2_v<float> / 2.0f;
            key_y *= std::numbers::sqrt2_v<float> / 2.0f;
        }
        deflection[x] = std::clamp(key_x + mouse[x], -1.0f, 1.0f);
        deflection[x + 1] = std::clamp(key_y + mouse[x + 1], -1.0f, 1.0f);
    }
    if (push[kAxisLeftY] < 0) {
        base.stick_dpad |= kInputUp;
    } else if (push[kAxisLeftY] > 0) {
        base.stick_dpad |= kInputDown;
    }
    if (push[kAxisLeftX] < 0) {
        base.stick_dpad |= kInputLeft;
    } else if (push[kAxisLeftX] > 0) {
        base.stick_dpad |= kInputRight;
    }
    for (int axis = kAxisLeftX; axis <= kAxisRightY; ++axis) {
        std::uint8_t &byte = AxisOf(base, static_cast<Axis>(axis));
        if (!Deflected(byte) && deflection[axis] != 0.0f) {
            byte = InputStickByte(deflection[axis]);
        }
    }
    return base;
}

std::uint8_t InputStickByte(float deflection) {
    if (!std::isfinite(deflection)) {
        return kInputAxisCentre;
    }
    float target = std::min(std::fabs(deflection), 1.0f) * kGameAxisRange;
    if (target < 0.5f) {
        return kInputAxisCentre;
    }
    // The smallest step past the dead zone that the game's integer scaling reads as at least target.
    int step = static_cast<int>(std::ceil(target * kLiveSpan / kGameAxisRange - 1e-3f));
    step = std::clamp(step, 1, kLiveSpan);
    if (deflection > 0.0f) {
        return static_cast<std::uint8_t>(std::min(255, kInputAxisCentre + kDeadZoneAbove + step));
    }
    return static_cast<std::uint8_t>(std::max(0, kInputAxisCentre - kDeadZoneBelow - step));
}

bool InputBindKeys(std::string_view action, std::span<const std::string_view> keys) {
    EnsureBindings();
    for (std::size_t i = 0; i < kActionCount; ++i) {
        if (kActions[i].name != action) {
            continue;
        }
        std::vector<Source> sources;
        if (!ParseSources(keys, kActions[i].kind, sources)) {
            return false;
        }
        g_bindings[i] = std::move(sources);
        return true;
    }
    return false;
}

void InputResetBindings() {
    g_bindings_ready = false;
    EnsureBindings();
}

void InputSetMouseSettings(const InputMouseSettings &settings) {
    EnsureBindings();
    g_mouse = settings;
    MouseConfigure(settings.capture, settings.release_scancodes);
}

const InputMouseSettings &InputGetMouseSettings() {
    EnsureBindings();
    return g_mouse;
}

int InputScancodeFromName(std::string_view name) {
    std::string lower = Lower(name);
    if (lower == "grave" || lower == "backquote" || lower == "backtick") {
        return SDL_SCANCODE_GRAVE;
    }
    std::string  text(name);
    SDL_Scancode scancode = SDL_GetScancodeFromName(text.c_str());
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        std::ranges::replace(text, '_', ' ');
        scancode = SDL_GetScancodeFromName(text.c_str());
    }
    return scancode == SDL_SCANCODE_UNKNOWN ? -1 : static_cast<int>(scancode);
}

bool InputHostHeld(InputHostAction action) {
    EnsureBindings();
    std::size_t host = static_cast<std::size_t>(action);
    for (const Source &source : g_bindings[kFirstHostAction + host]) {
        if (source.kind == Source::Key && g_keys[source.code]) {
            return true;
        }
    }
    return HostPolledHeld(host);
}

bool InputHostPressed(InputHostAction action) {
    int &presses = g_host_presses[static_cast<std::size_t>(action)];
    if (presses == 0) {
        return false;
    }
    --presses;
    return true;
}

void InputSetScriptedDevices(const InputKeyboardMouse &held) {
    EnsureBindings();
    g_scripted = held;
    PollHostActions();
}
