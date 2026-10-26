#include "title/cursol.hpp"

#include <cmath>

#include "mathutil.hpp"

CCursol::CCursol() {
    select = TITLE_MENU_NEW_GAME;
    y = 288.0f;
    target_y = 288.0f;
    alpha[0] = alpha[1] = alpha[2] = alpha[3] = alpha[4] = 0;
}

void CCursol::Init() {
    select = TITLE_MENU_NEW_GAME;
    y = 304.0f;
    target_y = 304.0f;
    alpha[0] = alpha[1] = alpha[2] = alpha[3] = alpha[4] = 0;
}

int CCursol::Move() {
    if (target_y > y) {
        y = y + (target_y - y) / 4.0f;

        if (target_y - y < 1.0f) {
            y = target_y;
            arrived = true;
        } else {
            arrived = false;
        }
    }

    if (target_y < y) {
        y = y - (y - target_y) / 4.0f;

        if (y - target_y < 1.0f) {
            y = target_y;
            arrived = true;
        } else {
            arrived = false;
        }
    }

    switch (select) {
        case TITLE_MENU_NEW_GAME:
            if (alpha[0] < 127) {
                alpha[0] += 8;
            }

            if (alpha[1] > 0) {
                alpha[1] -= 8;
            }

            if (alpha[2] > 0) {
                alpha[2] -= 8;
            }

            if (alpha[3] > 0) {
                alpha[3] -= 8;
            }

            if (alpha[4] > 0) {
                alpha[4] -= 8;
            }

            break;
        case TITLE_MENU_LOAD:
            if (alpha[0] > 0) {
                alpha[0] -= 8;
            }

            if (alpha[1] < 127) {
                alpha[1] += 8;
            }

            if (alpha[2] > 0) {
                alpha[2] -= 8;
            }

            if (alpha[3] > 0) {
                alpha[3] -= 8;
            }

            if (alpha[4] > 0) {
                alpha[4] -= 8;
            }

            break;
        case TITLE_MENU_OPTION:
            if (alpha[0] > 0) {
                alpha[0] -= 8;
            }

            if (alpha[1] > 0) {
                alpha[1] -= 8;
            }

            if (alpha[2] < 127) {
                alpha[2] += 8;
            }

            if (alpha[3] > 0) {
                alpha[3] -= 8;
            }

            if (alpha[4] > 0) {
                alpha[4] -= 8;
            }

            break;
        case TITLE_MENU_ATTRACT:
            if (alpha[0] > 0) {
                alpha[0] -= 8;
            }

            if (alpha[1] > 0) {
                alpha[1] -= 8;
            }

            if (alpha[2] > 0) {
                alpha[2] -= 8;
            }

            if (alpha[3] < 127) {
                alpha[3] += 8;
            }

            if (alpha[4] > 0) {
                alpha[4] -= 8;
            }

            break;
        case TITLE_MENU_UNK_4:
            if (alpha[0] > 0) {
                alpha[0] -= 8;
            }

            if (alpha[1] > 0) {
                alpha[1] -= 8;
            }

            if (alpha[2] > 0) {
                alpha[2] -= 8;
            }

            if (alpha[3] > 0) {
                alpha[3] -= 8;
            }

            if (alpha[4] < 127) {
                alpha[4] += 8;
            }

            break;
    }

    if (target_y == y) {
        return 1;
    }

    return 0;
}

void CCursol::Set(float new_target_y) {
    target_y = new_target_y;
}

int CCursol::GetSelect() {
    return select;
}

int CCursol::GetPos() {
    return (int) y;
}
