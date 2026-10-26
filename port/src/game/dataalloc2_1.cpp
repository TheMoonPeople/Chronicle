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
