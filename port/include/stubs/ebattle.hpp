#pragma once

// The port's EdGetRXf, EdGetRYf and EyeCamera (port/src/ebattle.cpp) ask this unit's static
// check_key_mode whether the editor's input mode owns the pad, through a global forwarder; that also
// keeps clang from dropping the static as unused.

static int check_key_mode(int mode);

int PortEdCheckKeyMode(int mode) {
    return check_key_mode(mode);
}
