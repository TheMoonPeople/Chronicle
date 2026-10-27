#include "common.h"

#include <libvu0.h>

#include <cstdint>

#include "edit.hpp"
#include "editmapscript.hpp"

void CommandWATER_SHAKE(void **arguments) {
    EDIT_WATER_INFO *info = water_info;

    if (info == nullptr) {
        return;
    }

    // The scan has no upper bound, so each slot's address is formed on the integer address rather than by
    // indexing info->wave, which would let the compiler assume the loop stops within its four entries.
    for (std::uintptr_t index = 0;; index++) {
        std::uintptr_t address = reinterpret_cast<std::uintptr_t>(info) + (index * sizeof(sceVu0FVECTOR));
        auto          *wave = reinterpret_cast<EDIT_WATER_WAVE_VIEW *>(address);

        if (wave->power == 0.0f && wave->range == 0.0f) {
            wave->row = static_cast<float>(*static_cast<int *>(arguments[0]));
            wave->column = static_cast<float>(*static_cast<int *>(arguments[1]));
            wave->range = *static_cast<float *>(arguments[3]);
            wave->power = *static_cast<float *>(arguments[2]);
            return;
        }
    }
}
