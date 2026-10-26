#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 1116

#include "hit_machingun_effect.hpp"

#include <libvu0.h>

#include "shot_utils.hpp"

void CHIT_MACHINGUN_EFFECT::Set(float *position) {
    s32 free_index = -1;

    for (s32 i = 0; i < 16; i++) {
        if (this->timer[i] < 0) {
            free_index = i;
            break;
        }
    }

    if (free_index != -1) {
        sceVu0CopyVector(this->position[free_index], position);
        this->timer[free_index] = 17;
    }
}

void CHIT_MACHINGUN_EFFECT::Draw() {
    s32 cell;

    for (s32 i = 0; i < 16; i++) {
        if (this->timer[i] >= 0) {
            cell = 2 - this->timer[i] / 6;
            set3DCellModel(this->position[i], "basefx00", 5.0f, cell << 4, 0x70, 0x10, 0x10, 0x80);
        }
    }
}

void CHIT_MACHINGUN_EFFECT::Step() {
    for (s32 i = 0; i < 16; i++) {
        if (this->timer[i] >= 0) {
            this->timer[i]--;
        }
    }
}
