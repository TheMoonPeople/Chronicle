#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 744

#include "dataalloc.hpp"

CDataAlloc2<1>::CDataAlloc2(int limit) {
    this->limit = limit;
    used = 0;
    buffer = 0;

    /* A negative limit leaves the base alone rather than pointing it at nothing. */
    if (limit < 0) {
        return;
    }

    base = buffer;
}
