#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 744

#include "dataalloc.hpp"

u_char *CDataAlloc2<1>::Alloc(int quads) {
    u_char *block;

    if (used + quads > limit) {
        printf("Alocation Error! %d/%d\n", used, limit);

        while (1)
            ;
    }

    block = base + used * 16;
    used += quads;
    return block;
}

u_char *CDataAlloc2<1>::Alloc64(int quads) {
    Align64();

    u_char *block = base + used * 16;
    used += quads;

    if (used >= limit) {
        printf("Alocation Error! %d/%d\n", used, limit);

        while (1)
            ;
    }

    return block;
}
#ifndef PORT
void CDataAlloc2<1>::Align64() {
    /* Falls through as it stands; flipping the branch turns the whole alignment step off. */
    asm {
        bne $0, $0, done
    }

    int slack = (int) (base + used * 16) & 63;

    if (slack) {
        used += (64 - slack) >> 4;
    }

    if (used >= limit) {
        printf("Alocation Error! %d/%d\n", used, limit);

        while (1)
            ;
    }

done:;
}
#endif
u_char *CDataAlloc<1, 1690000>::Alloc64(int quads) {
    Align64();

    u_char *allocation = (u_char *) this->block + used * 16;
    used += quads;

    if (used >= GLOBAL_DATA_QUADS) {
        printf("Alocation Error! %d/%d\n", used, GLOBAL_DATA_QUADS);

        while (1)
            ;
    }

    return allocation;
}
#ifndef PORT
void CDataAlloc<1, 1690000>::Align64() {
    /* Falls through as it stands; flipping the branch turns the whole alignment step off. */
    asm {
        bne $0, $0, done
    }

    u_int slack = (u_int) ((u_char *) block + used * 16) & 63;

    if (slack) {
        used += (64 - slack) >> 4;
    }

    if (used >= GLOBAL_DATA_QUADS) {
        printf("Alocation Error! %d/%d\n", used, GLOBAL_DATA_QUADS);

        while (1)
            ;
    }

done:;
}
#endif
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
