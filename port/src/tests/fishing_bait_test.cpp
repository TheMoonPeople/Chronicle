#include <gtest/gtest.h>

#include <array>

#include "fishing.hpp"
#include "itemdata.hpp"
#include "runscript.hpp"

namespace dc::test {

using namespace dc::game;

namespace {

// The variables of PAL retail's bait-changing event (program 134 of gedit/e01): the picker's choice, then
// the script's list of eight baits, then its loop counter.
std::array<RS_STACKDATA, 10> PalBaitEvent() {
    constexpr std::array         listed = {ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE,
                                           ITEM_MELLOW_BANANA, ITEM_EVY, ITEM_MIMI, ITEM_PRICKLY};
    std::array<RS_STACKDATA, 10> variables{};
    for (RS_STACKDATA &variable : variables) {
        variable.type = RS_INT;
        variable.i = 0;
    }
    for (std::size_t slot = 0; slot < listed.size(); slot++) {
        variables[1 + slot].i = listed[slot];
    }
    return variables;
}

} // namespace

TEST(FishingBait, ABaitTheTownsListLacksIsPutOnIt) {
    for (int bait : {ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH}) {
        auto variables = PalBaitEvent();
        EXPECT_TRUE(ListBaitForScript(variables.data(), bait)) << bait;
        EXPECT_EQ(variables[1].i, bait);
    }
}

TEST(FishingBait, ABaitOnTheListIsLeftToTheScript) {
    auto variables = PalBaitEvent();
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_EVY));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);
}

TEST(FishingBait, ACancelledPickChangesNothing) {
    auto variables = PalBaitEvent();
    EXPECT_FALSE(ListBaitForScript(variables.data(), -1));
    EXPECT_FALSE(ListBaitForScript(variables.data(), 0));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);
}

TEST(FishingBait, AScriptLaidOutOtherwiseIsLeftAlone) {
    auto variables = PalBaitEvent();
    variables[4].i = 12; // not a bait: these variables are not a bait list
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_CARROT));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);

    variables = PalBaitEvent();
    variables[2].type = RS_FLOAT;
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_CARROT));
}

} // namespace dc::test
