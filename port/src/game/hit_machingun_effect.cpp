#include "hit_machingun_effect.hpp"

#include <libvu0.h>

#include "shot_utils.hpp"

namespace dc::game {

void CHIT_MACHINGUN_EFFECT::Set(float *position) {
    for (s32 i = 0; i < 16; i++) {
        if (this->timer[i] < 0) {
            sceVu0CopyVector(this->position[i], position);
            this->timer[i] = 17;
            return;
        }
    }
}

void CHIT_MACHINGUN_EFFECT::Draw() {
    for (s32 i = 0; i < 16; i++) {
        if (this->timer[i] >= 0) {
            const s32 cell = 2 - this->timer[i] / 6;
            set3DCellModel(this->position[i], "basefx00", 5.0f, cell << 4, 0x70, 0x10, 0x10, 0x80);
        }
    }
}

void CHIT_MACHINGUN_EFFECT::Step() {
    for (s32 &frames_left : this->timer) {
        if (frames_left >= 0) {
            frames_left--;
        }
    }
}

} // namespace dc::game
