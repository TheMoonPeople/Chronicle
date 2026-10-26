#include "fader.hpp"

int Fader::In() {
    if (this->value < 0x80) {
        this->value += 4;

        if (this->value > 0x80) {
            this->value = 0x80;
        }

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
    float level = (float) this->value / 128.0f;
    level *= (float) maximum;
    return (int) level;
}
