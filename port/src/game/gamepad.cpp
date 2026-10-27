#include "gamepad.hpp"
#include "sce/libgraph.h"
#include "sce/libpad.h"

#include <algorithm>
#include <array>
#include <memory>
#include <print>

#include "input/dualsense.hpp"
#include "mainselect.hpp"
#include "platform/input.hpp"
#include "ui/menu_pointer.hpp"

namespace dc::game {

using namespace dc::input;
using namespace dc::ui;

CGamePad GamePad;

namespace {

struct alignas(64) PadDmaBuffer {
    std::array<unsigned char, 1024> bytes;
};

// Allocated once; the pad library keeps using them after the ports open.
std::array<std::unique_ptr<PadDmaBuffer>, 2> pad_dma_buffers;

} // namespace

void CGamePad::Init() {
    key_lock = 0;
    key_lock2 = 0;
    vibration_enabled = 1;

    while (sceGsSyncV(0) == 0) {
    }

    scePadInit(0);
    unk_130[1] = 0;
    unk_130[2] = 0;
    unk_130[3] = 0;
    unk_130[4] = 0;

    for (int i = 0; i < 2; i++) {
        pad[i].input.status.phase = PAD_PHASE_QUERY;
        pad[i].input.status.button = 0;
        pad[i].input.status.right_y = 0;
        pad[i].input.status.right_x = 0;
        pad[i].input.status.left_y = 0;
        pad[i].input.status.left_x = 0;

        for (unsigned char &strength : pad[i].actuator.vibration) {
            strength = 0;
            pad[i].actuator.vibration_timer[i] = 0;
        }

        axis_threshold[i] = 0;
        repeat[i].enabled = 0;
        repeat[i].active = 0;

        for (int j = 0; j < 32; j++) {
            repeat[i].counter[j] = 0;
            repeat[i].repeat_delay[j] = 0;
            repeat[i].initial_delay[j] = 0;
        }
    }

    for (std::unique_ptr<PadDmaBuffer> &buffer : pad_dma_buffers) {
        if (!buffer) {
            buffer = std::make_unique<PadDmaBuffer>();
        }
    }

    if (!scePadPortOpen(0, 0, pad_dma_buffers[0]->bytes.data())) {
        std::println("ERROR: scePadPortOpen");
        return;
    }

    sceGsSyncV(0);
    sceGsSyncV(0);
    sceGsSyncV(0);
    sceGsSyncV(0);

    if (!scePadPortOpen(1, 0, pad_dma_buffers[1]->bytes.data())) {
        std::println("ERROR: scePadPortOpen");
        return;
    }

    sceGsSyncV(0);
    sceGsSyncV(0);
    sceGsSyncV(0);
    sceGsSyncV(0);
}

// The pad read and stick replacements tell the host when to latch mouse motion and when movement
// keys must also press the d-pad. Button replacements require a deliberate modifier for retail's
// pad 2 debug reads without PAL's pad 2 Start/Select overrides changing ordinary pad 1 buttons.

namespace {

constexpr int kDebugButtonsModifier = PAD_SELECT | PAD_L2;
bool          g_debug_menu_pressed = false;
int           g_held_down = 0;

} // namespace

void PadHoldDown(int mask) {
    g_held_down = mask;
}

int pad_button_read(PAD_STATUS *status, int port, int slot) {
    static thread_local auto data = std::make_unique<unsigned char[]>(32);
    InputLatchPad(port);
    if (port == 0) {
        DualSenseUpdate();
        MenuMouseUpdate();
        g_debug_menu_pressed = InputHostPressed(InputHostAction::DebugMenu);
    }
    if (!scePadRead(port, slot, data.get())) {
        return 0;
    }
    int mode = 0;
    if (data[0] == 0) {
        int button = ((data[2] << 8) | data[3]) ^ 0xffff;
        status->button = button & 0xffff;
        status->right_x = data[4];
        status->right_y = data[5];
        status->left_x = data[6];
        status->left_y = data[7];
        mode = data[1] >> 4;
    }
    return mode;
}

int read_pad(PAD_STATUS *status, int port, int slot) {
    int &phase = status->phase;
    int &state = status->state;

    state = scePadGetState(port, slot);

    if (state == scePadStateDiscon) {
        phase = PAD_PHASE_QUERY;
    }

    bool valid = false;

    switch (phase) {
        case PAD_PHASE_QUERY:
            if (state == scePadStateStable || state == scePadStateFindCTP1) {
                int terminal_id = scePadInfoMode(port, slot, InfoModeCurID, 0);

                if (terminal_id != 0) {
                    status->extended_id = scePadInfoMode(port, slot, InfoModeCurExID, 0);

                    if (status->extended_id > 0) {
                        terminal_id = status->extended_id;
                    }

                    switch (terminal_id) {
                        case PAD_TERMINAL_DIGITAL:
                            phase = PAD_PHASE_ANALOG_CHECK;
                            break;
                        case PAD_TERMINAL_DUALSHOCK:
                            phase = PAD_PHASE_ACTUATOR_CHECK;
                            break;
                        default:
                            phase = PAD_PHASE_READY;
                            break;
                    }
                }
            }

            break;
        case PAD_PHASE_ANALOG_CHECK:
            if (scePadInfoMode(port, slot, InfoModeCurExID, 0) == 0) {
                phase = PAD_PHASE_READY;
                break;
            }

            phase++;
            [[fallthrough]];
        case PAD_PHASE_ANALOG_SET:
            if (scePadSetMainMode(port, slot, 1, 3) == 1) {
                phase++;
            }

            break;
        case PAD_PHASE_ANALOG_WAIT:
            if (scePadGetState(port, slot) != scePadStateExecCmd) {
                phase = PAD_PHASE_QUERY;
            }

            break;
        case PAD_PHASE_ACTUATOR_CHECK:
            if (scePadInfoAct(port, slot, -1, 0) == 0) {
                phase = PAD_PHASE_READY;
            }

            status->actuator[0] = 0;
            status->actuator[1] = 1;

            for (int i = 2; i < 6; i++) {
                status->actuator[i] = 255;
            }

            if (scePadSetActAlign(port, slot, status->actuator)) {
                phase++;
            }

            break;
        case PAD_PHASE_ACTUATOR_WAIT:
            if (scePadGetState(port, slot) != scePadStateExecCmd) {
                phase = PAD_PHASE_READY;
            }

            break;
        default:
            if (state == scePadStateStable || state == scePadStateFindCTP1) {
                status->pad_mode = pad_button_read(status, port, slot);

                if (status->pad_mode != 0) {
                    if (status->previous_pad_mode != 0 && status->pad_mode != status->previous_pad_mode) {
                        status->previous_pad_mode = 0;
                        phase = PAD_PHASE_QUERY;
                    } else {
                        valid = true;
                    }

                    status->previous_pad_mode = status->pad_mode;
                }
            }

            break;
    }

    if (!valid) {
        status->button = 0;
        status->left_y = 128;
        status->left_x = 128;
        status->right_y = 128;
        status->right_x = 128;
    }

    if (status->pad_mode == PAD_TERMINAL_DIGITAL) {
        status->left_y = 128;
        status->left_x = 128;
        status->right_y = 128;
        status->right_x = 128;
    }

    return 0;
}

void CGamePad::UpDate() {
    for (int port = 0; port < 2; port++) {
        previous_pad[port].input.status = pad[port].input.status;
        read_pad(&pad[port].input.status, port, 0);
    }

    for (int i = 0; i < 2; i++) {
        const int threshold = std::max(axis_threshold[i], 0);

        // Both ports compare against the first controller's left stick.
        if (threshold > 0) {
            if (threshold < GetLX()) {
                pad[i].input.status.button |= PAD_RIGHT;
            }

            if (GetLX() < -threshold) {
                pad[i].input.status.button |= PAD_LEFT;
            }

            if (threshold < GetLY()) {
                pad[i].input.status.button |= PAD_DOWN;
            }

            if (GetLY() < -threshold) {
                pad[i].input.status.button |= PAD_UP;
            }
        }
    }

    int         bit = 1;
    PAD_REPEAT &repeat_state = repeat[0];

    for (int i = 0; i < 32; i++, bit <<= 1) {
        if (repeat_state.enabled & bit) {
            if ((pad[0].input.status.button & repeat_state.enabled) & bit) {
                repeat_state.counter[i]++;

                if (repeat_state.counter[i] >= repeat_state.initial_delay[i]) {
                    repeat_state.active |= bit;
                }
            } else {
                repeat_state.counter[i] = 0;
                repeat_state.active &= ~bit;
            }

            if (repeat_state.counter[i] >= repeat_state.repeat_delay[i] && (repeat_state.active & bit)) {
                pad[0].input.status.button &= ~bit;
                repeat_state.counter[i] = 0;
            }
        }
    }

    for (PAD_DATA &port_data : pad) {
        int &button = port_data.input.status.button;

        if ((button & PAD_UP) && (button & PAD_DOWN)) {
            button &= ~(PAD_UP | PAD_DOWN);
        }

        if ((button & PAD_RIGHT) && (button & PAD_LEFT)) {
            button &= ~(PAD_RIGHT | PAD_LEFT);
        }
    }

    if (key_lock2) {
        pad[1].input.status.button = 0;
        pad[1].input.status.right_x = 128;
        pad[1].input.status.right_y = 128;
        pad[1].input.status.left_x = 128;
        pad[1].input.status.left_y = 128;
    }
}

void CGamePad::Step() {
    for (PAD_DATA &port_data : pad) {
        PAD_STATUS &status = port_data.input.status;

        if (!vibration_enabled) {
            status.vibration[0] = 0;
            status.vibration[1] = 0;
        }

        for (int motor = 0; motor < 2; motor++) {
            if (status.vibration_timer[motor] > 0) {
                status.vibration_timer[motor]--;
            } else {
                status.vibration_timer[motor] = 0;
                status.vibration[motor] = 0;
            }
        }
    }

    scePadSetActDirect(0, 0, pad[0].input.status.vibration);
}

int AxisCalibration(int axis) {
    const int offset = axis - 128;

    if (offset < 50 && offset > -50) {
        return 0;
    }

    if (offset > 0) {
        return (offset - 49) * 128 / 78;
    }

    return (offset + 50) * 128 / 78;
}

int CGamePad::GetRX() {
    return AxisCalibration(pad[0].input.status.right_x);
}

int CGamePad::GetRY() {
    return AxisCalibration(pad[0].input.status.right_y);
}

int CGamePad::GetLX() {
    InputNoteLeftStickRead();
    return AxisCalibration(pad[0].input.status.left_x);
}

int CGamePad::GetLY() {
    InputNoteLeftStickRead();
    return AxisCalibration(pad[0].input.status.left_y);
}

int CGamePad::GetRX2() {
    return AxisCalibration(pad[1].input.status.right_x);
}

int CGamePad::GetRY2() {
    return AxisCalibration(pad[1].input.status.right_y);
}

int CGamePad::GetLX2() {
    return AxisCalibration(pad[1].input.status.left_x);
}

int CGamePad::GetLY2() {
    return AxisCalibration(pad[1].input.status.left_y);
}

int CGamePad::AllOn() {
    return pad[1].input.status.button | pad[0].input.status.button | GetRX() | GetRY() | GetLX() | GetLY() | GetRX2() | GetRY2() | GetLX2() | GetLY2();
}

void CGamePad::CancelAutoRepeat(int mask) {
    int         bit = 1;
    PAD_REPEAT &auto_repeat = repeat[0];

    for (int i = 0; i < 32; i++, bit <<= 1) {
        if (mask & bit) {
            auto_repeat.enabled &= ~bit;
            auto_repeat.active &= ~bit;
            auto_repeat.counter[i] = 0;
            auto_repeat.initial_delay[i] = 0;
            auto_repeat.repeat_delay[i] = 0;
        }
    }
}

void CGamePad::SetAutoRepeat(int mask, int initial_delay, int repeat_delay) {
    int         bit = 1;
    PAD_REPEAT &auto_repeat = repeat[0];

    for (int i = 0; i < 32; i++, bit <<= 1) {
        if (!(auto_repeat.enabled & bit) && (mask & bit)) {
            auto_repeat.enabled |= bit;
            auto_repeat.active &= ~bit;
            auto_repeat.counter[i] = 0;
            auto_repeat.initial_delay[i] = initial_delay;
            auto_repeat.repeat_delay[i] = repeat_delay;
        }
    }
}

void CGamePad::KeyLock(int mask) {
    key_lock = mask;
    InputSetMovementLocked(mask != 0);
}

void CGamePad::KeyLock2(int mask) {
    key_lock2 = mask;
}

int CGamePad::GetPadOn() {
    if (key_lock) {
        return 0;
    }

    return pad[0].input.status.button;
}

int CGamePad::GetPadDown() {
    if (key_lock) {
        return 0;
    }

    return pad[0].input.status.button & ~previous_pad[0].input.status.button;
}

int CGamePad::GetPadUp() {
    if (key_lock) {
        return 0;
    }

    return ~pad[0].input.status.button & previous_pad[0].input.status.button;
}

float CGamePad::GetRXf() {
    return static_cast<float>(GetRX()) / 128.0f;
}

float CGamePad::GetRYf() {
    return static_cast<float>(GetRY()) / 128.0f;
}

float CGamePad::GetLXf() {
    return static_cast<float>(GetLX()) / 128.0f;
}

float CGamePad::GetLYf() {
    return static_cast<float>(GetLY()) / 128.0f;
}

float CGamePad::GetLXf2() {
    return static_cast<float>(GetLX2()) / 128.0f;
}

float CGamePad::GetLYf2() {
    return static_cast<float>(GetLY2()) / 128.0f;
}

int CGamePad::On(int mask) {
    if (key_lock) {
        return 0;
    }

    // The pointer holds a button for the tick (the Allies turntable turns on held buttons).
    if ((mask & MenuPointerSyntheticDown()) != 0) {
        return 1;
    }

    return (pad[0].input.status.button & mask) != 0;
}

int CGamePad::On2(int mask) {
    return DebugMode && (pad[0].input.status.button & kDebugButtonsModifier) == kDebugButtonsModifier && On(mask);
}

int CGamePad::Down(int mask) {
    if (key_lock) {
        return 0;
    }

    mask &= ~g_held_down;

    if (mask == 0) {
        return 0;
    }

    // A menu's pointer presses the buttons its clicks stand for.
    if ((mask & MenuPointerSyntheticDown()) != 0) {
        return 1;
    }

    if (InputDeveloperMenu()) {
        if (mask == PAD_CIRCLE) {
            mask = PAD_CROSS;
        } else if (mask == PAD_TRIANGLE) {
            return 0;
        }
    }

    if (DebugMode && (pad[0].input.status.button & kDebugButtonsModifier) == kDebugButtonsModifier) {
        mask &= ~(PAD_CROSS | PAD_CIRCLE);
    }

    if ((mask & PAD_R3) && DebugMode && g_debug_menu_pressed) {
        return 1;
    }

    if ((mask & (PAD_L3 | PAD_R3)) && (!DebugMode || (pad[0].input.status.button & kDebugButtonsModifier) != kDebugButtonsModifier)) {
        return 0;
    }

    return (mask & (pad[0].input.status.button & ~previous_pad[0].input.status.button)) != 0;
}

int CGamePad::Down2(int mask) {
    if (key_lock || !DebugMode || (pad[0].input.status.button & kDebugButtonsModifier) != kDebugButtonsModifier) {
        return 0;
    }

    return (mask & (pad[0].input.status.button & ~previous_pad[0].input.status.button)) != 0;
}

void CGamePad::AutoRepeatOff() {
    CancelAutoRepeat(-1);
}

void CGamePad::MenuModeOn(int threshold) {
    axis_threshold[0] = threshold;
    InputSetMenuNavigation(true);
}

void CGamePad::MenuModeOff() {
    axis_threshold[0] = 0;
    InputSetMenuNavigation(false);
}

void CGamePad::SetVibration(int motor, int intensity, int duration) {
    if (!vibration_enabled) {
        return;
    }

    if (motor < 0 || motor >= 2 || duration < 0 || duration > 1200) {
        return;
    }

    pad[0].input.status.vibration_timer[motor] = duration;

    if (motor == 0) {
        intensity = static_cast<int>(intensity != 0);
    }

    pad[0].input.status.vibration[motor] = static_cast<unsigned char>(intensity);
}

void CGamePad::VibrationEnable(int enable) {
    vibration_enabled = enable;
}

void CGamePad::StopVibration() {
    SetVibration(0, 0, 0);
    SetVibration(1, 0, 0);
    Step();
}

} // namespace dc::game
