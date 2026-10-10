#include "gamepad.hpp"
#include "gamepad_port.hpp"

#include <libpad.h>

#include "dualsense.hpp"
#include "mainselect.hpp"
#include "menu_pointer.hpp"
#include "platform/input.hpp"

// The pad read and stick replacements tell the host when to latch mouse motion and when movement
// keys must also press the d-pad. Button replacements require a deliberate modifier for retail's
// pad 2 debug reads without PAL's pad 2 Start/Select overrides changing ordinary pad 1 buttons.

constexpr int kDebugButtonsModifier = PAD_SELECT | PAD_L2;

static bool g_debug_menu_pressed = false;

static int g_held_down = 0;

void PadHoldDown(int mask) {
    g_held_down = mask;
}

PC_OVERRIDE int pad_button_read(PAD_STATUS *status, int port, int slot) {
    unsigned char data[32];
    InputLatchPad(port);
    if (port == 0) {
        DualSenseUpdate();
        MenuMouseUpdate();
        g_debug_menu_pressed = InputHostPressed(InputHostAction::DebugMenu);
    }
    if (!scePadRead(port, slot, data)) {
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

PC_OVERRIDE int CGamePad::GetLX() {
    InputNoteLeftStickRead();
    return AxisCalibration(pad[0].input.status.left_x);
}

PC_OVERRIDE int CGamePad::GetLY() {
    InputNoteLeftStickRead();
    return AxisCalibration(pad[0].input.status.left_y);
}

PC_OVERRIDE int CGamePad::On(int mask) {
    if (key_lock) {
        return 0;
    }

    // The pointer holds a button for the tick (the Allies turntable turns on held buttons).
    if ((mask & MenuPointerSyntheticDown()) != 0) {
        return 1;
    }

    return (pad[0].input.status.button & mask) != 0;
}

PC_OVERRIDE int CGamePad::On2(int mask) {
    return DebugMode && (pad[0].input.status.button & kDebugButtonsModifier) == kDebugButtonsModifier && On(mask);
}

PC_OVERRIDE int CGamePad::Down(int mask) {
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

PC_OVERRIDE int CGamePad::Down2(int mask) {
    if (key_lock || !DebugMode || (pad[0].input.status.button & kDebugButtonsModifier) != kDebugButtonsModifier) {
        return 0;
    }

    return (mask & (pad[0].input.status.button & ~previous_pad[0].input.status.button)) != 0;
}

PC_OVERRIDE void CGamePad::KeyLock(int mask) {
    key_lock = mask;
    InputSetMovementLocked(mask != 0);
}

PC_OVERRIDE void CGamePad::MenuModeOn(int threshold) {
    axis_threshold[0] = threshold;
    InputSetMenuNavigation(true);
}

PC_OVERRIDE void CGamePad::MenuModeOff() {
    axis_threshold[0] = 0;
    InputSetMenuNavigation(false);
}
