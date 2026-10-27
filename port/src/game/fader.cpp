#include "fader.hpp"

#include <algorithm>

namespace dc::game {

int Fader::In() {
    if (this->value < 0x80) {
        this->value = std::min(this->value + 4, 0x80);

        return 0;
    }

    return 1;
}

int Fader::Out() {
    if (this->value > 0) {
        this->value -= 4;
        return 0;
    }

    return 1;
}

int Fader::Get(int maximum) {
    float level = static_cast<float>(this->value) / 128.0f;
    level *= static_cast<float>(maximum);
    return static_cast<int>(level);
}

} // namespace dc::game
