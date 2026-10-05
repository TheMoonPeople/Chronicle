#include "menu_draw.hpp"

#include <cstdint>

// Retail rounds up to 64 bytes through int, which only holds an address below 2 GiB.
PC_OVERRIDE u_long128 *MenuCalcBufAlignment(u_long128 *buffer) {
    auto address = reinterpret_cast<std::uintptr_t>(buffer);
    return reinterpret_cast<u_long128 *>((address + 63) & ~std::uintptr_t{63});
}
