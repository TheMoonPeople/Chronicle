#include "gamepad.hpp"

#include <libpad.h>

#include "platform/input.hpp"

// Retail's bodies, plus the two things the host's keyboard and mouse need to know about the game's
// reads: when pad 0 is read (the mouse motion since the previous read becomes this read's right
// stick) and whether the game looks at the left stick at all (if not, the movement keys also press
// the d-pad, for the screens that read only the d-pad).

PC_OVERRIDE int pad_button_read(PAD_STATUS *status, int port, int slot) {
    unsigned char data[32];
    InputLatchPad(port);
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
