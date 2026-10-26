#include "dataalloc.hpp"

CDataAlloc2<1>::CDataAlloc2(int limit) : buffer(nullptr), used(0), limit(limit) {
    // A negative limit leaves the base untouched.
    if (limit < 0) {
        return;
    }

    base = buffer;
}
