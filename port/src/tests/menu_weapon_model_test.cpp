#include <gtest/gtest.h>

#include "menu_misc.hpp"

namespace dc::test {

using namespace dc::game;

// Retail clears twelve entries of the ten-entry weapon model table; in the port the extra two landed on
// whatever the linker put after it, once on WepEffectMenuReadBuf, so the windmill slash crashed after the
// weapon menu. The table's own entries are cleared and nothing else is touched.
TEST(MenuWeaponModel, ClearingTheReferencesStaysInTheTable) {
    auto *sentinel = reinterpret_cast<u_long128 *>(0x1000);
    WepEffectMenuReadBuf = sentinel;
    for (int (&entry)[2] : MenuWeaponModelInfo) {
        entry[0] = 7;
        entry[1] = 7;
    }

    InitMenuWeaponModelReference();

    for (const int (&entry)[2] : MenuWeaponModelInfo) {
        EXPECT_EQ(entry[0], -1);
        EXPECT_EQ(entry[1], -1);
    }
    EXPECT_EQ(WepEffectMenuReadBuf, sentinel);
    EXPECT_EQ(GetWepEffectMenuReadBuf(), sentinel);
}

} // namespace dc::test
