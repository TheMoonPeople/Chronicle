#include "title/scfader.hpp"

#include <algorithm>

namespace dc::game {

int CScFader::In() {
    if (value < 128) {
        value = std::min(value + 2, 128);
        return 0;
    }

    return 1;
}

int CScFader::In2() {
    if (value < 128) {
        value = std::min(value + 8, 128);
        return 0;
    }

    return 1;
}

int CScFader::Out() {
    if (value > -4) {
        value = std::max(value - 2, -4);
        return 0;
    }

    return 1;
}

int CScFader::Get(int limit) {
    if (value < limit) {
        if (value < 0) {
            return 0;
        }

        return value;
    }

    return limit;
}

void CScFader::Skip() {
    value = 128;
}

} // namespace dc::game
