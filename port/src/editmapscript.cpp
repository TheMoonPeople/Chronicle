#include "common.h"

#include <libvu0.h>

#include <cstdint>

#include "edit.hpp"
#include "editmapscript.hpp"

// Retail's CommandWATER_SHAKE, with the wave slot's address formed on the whole pointer.

PC_OVERRIDE void CommandWATER_SHAKE(void **arguments) {
    EDIT_WATER_INFO *info = water_info;

    if (info != NULL) {
        int index = 0;

        while (1) {
            std::uintptr_t offset = index * sizeof(sceVu0FVECTOR);
            offset += (std::uintptr_t) info;
            EDIT_WATER_WAVE_VIEW *wave = (EDIT_WATER_WAVE_VIEW *) offset;

            if (wave->power == 0.0f && wave->range == 0.0f) {
                wave->row = (float) *(int *) arguments[0];
                wave->column = (float) *(int *) arguments[1];
                wave->range = *(float *) arguments[3];
                wave->power = *(float *) arguments[2];
                break;
            }

            index++;
        }
    }
}
