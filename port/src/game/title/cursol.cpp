#include "title/cursol.hpp"

#include <cstddef>
#include <utility>

namespace dc::game {

CCursol::CCursol() : y(288.0f), target_y(288.0f), alpha{}, select(TITLE_MENU_NEW_GAME) {}

void CCursol::Init() {
    select = TITLE_MENU_NEW_GAME;
    y = 304.0f;
    target_y = 304.0f;
    alpha.fill(0);
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

    // The selected row fades in and every other row fades out; an out-of-range row leaves all as they are.
    if (select >= TITLE_MENU_NEW_GAME && select <= TITLE_MENU_UNK_4) {
        for (std::size_t row = 0; row < alpha.size(); row++) {
            if (std::cmp_equal(row, select)) {
                if (alpha[row] < 127) {
                    alpha[row] += 8;
                }
            } else if (alpha[row] > 0) {
                alpha[row] -= 8;
            }
        }
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
    return static_cast<int>(y);
}

} // namespace dc::game
