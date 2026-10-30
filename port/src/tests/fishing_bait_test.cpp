#include <gtest/gtest.h>

#include <array>

#include "fishing.hpp"
#include "fishing/framework.hpp"
#include "fishing/host.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "menu_save.hpp"
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

// The baits come from the item registry.
class FishingBait : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        item::Load();
        fishing::Load();
    }

    void TearDown() override {
        item::Load();
        fishing::Load();
        FishingDeleteEsa();
    }

    // A custom bait, as a pack's document would give it, and the fishing framework told of it.
    static int AddLure(bool lure, float default_affinity) {
        const int number = item::kCustomFirst + 40;
        item::Unregister("chronicle:spinner");
        item::Register(item::Def{.id = "chronicle:spinner", .number = number}.With(
            item::Bait{.radius = 30.0f, .lure = lure, .default_affinity = default_affinity}));
        fishing::Load();
        return number;
    }
};

} // namespace

TEST_F(FishingBait, ABaitTheTownsListLacksIsPutOnIt) {
    for (int bait : {ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON, ITEM_BATTAN, ITEM_PETITE_FISH}) {
        auto variables = PalBaitEvent();
        EXPECT_TRUE(ListBaitForScript(variables.data(), bait)) << bait;
        EXPECT_EQ(variables[1].i, bait);
    }
}

TEST_F(FishingBait, ABaitOnTheListIsLeftToTheScript) {
    auto variables = PalBaitEvent();
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_EVY));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);
}

TEST_F(FishingBait, ACancelledPickChangesNothing) {
    auto variables = PalBaitEvent();
    EXPECT_FALSE(ListBaitForScript(variables.data(), -1));
    EXPECT_FALSE(ListBaitForScript(variables.data(), 0));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);
}

TEST_F(FishingBait, AScriptLaidOutOtherwiseIsLeftAlone) {
    auto variables = PalBaitEvent();
    variables[4].i = 12; // not a bait: these variables are not a bait list
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_CARROT));
    EXPECT_EQ(variables[1].i, ITEM_THROBBING_CHERRY);

    variables = PalBaitEvent();
    variables[2].type = RS_FLOAT;
    EXPECT_FALSE(ListBaitForScript(variables.data(), ITEM_CARROT));
}

TEST_F(FishingBait, RetailsThirteenComeFirstInTheirOrder) {
    const std::vector<int> baits = fishing::BaitItems();
    ASSERT_EQ(baits.size(), esa_info.size());
    for (std::size_t index = 0; index < esa_info.size(); index++) {
        EXPECT_EQ(baits[index], esa_info[index].item_no);
        EXPECT_FALSE(fishing::BaitAt(static_cast<int>(index))->lure);
    }
    EXPECT_EQ(fishing::BaitIndex(ITEM_BREAD), -1);
}

TEST_F(FishingBait, ACustomBaitIsPickedAttachedAndGivenBack) {
    const int lure = AddLure(true, 0.4f);
    EXPECT_EQ(fishing::BaitItems().back(), lure);
    const int index = fishing::BaitIndex(lure);
    ASSERT_EQ(index, static_cast<int>(esa_info.size()));
    EXPECT_EQ(fishing::BaitAt(index)->id, "chronicle:spinner");
    EXPECT_TRUE(fishing::BaitAt(index)->lure);

    auto variables = PalBaitEvent();
    EXPECT_TRUE(ListBaitForScript(variables.data(), lure));

    FishingDeleteEsa(); // nothing on the hook to give back
    FishingLoadEsa(lure, nullptr, 40);
    EXPECT_EQ(esa_type, index);
    EXPECT_EQ(FishingGetEsaItemNo(), lure);
}

TEST_F(FishingBait, ThePickerOffersOnlyBaitForFishing) {
    const int lure = AddLure(true, 0.0f);
    EXPECT_TRUE(EventItemListed(ITEM_CARROT, EVENT_ITEM_MODE_BAIT));
    EXPECT_TRUE(EventItemListed(lure, EVENT_ITEM_MODE_BAIT));
    EXPECT_FALSE(EventItemListed(ITEM_BREAD, EVENT_ITEM_MODE_BAIT));
    EXPECT_FALSE(EventItemListed(ITEM_FISHING_ROD, EVENT_ITEM_MODE_BAIT));
    // Outside fishing, every event item, as retail's.
    EXPECT_TRUE(EventItemListed(ITEM_BREAD, EVENT_ITEM_MODE_EVENT));
    EXPECT_FALSE(EventItemListed(ITEM_DUNGEON_START - 1, EVENT_ITEM_MODE_EVENT));
}

TEST_F(FishingBait, TheRodBoardOffersOnlyRods) {
    const int rod = item::kCustomFirst + 41;
    item::Unregister("chronicle:sea_rod");
    item::Register(item::Def{.id = "chronicle:sea_rod", .number = rod}.With(item::Rod{.spring = 18.0f}));
    EXPECT_TRUE(EventItemListed(ITEM_FISHING_ROD, EVENT_ITEM_MODE_ROD));
    EXPECT_TRUE(EventItemListed(rod, EVENT_ITEM_MODE_ROD));
    EXPECT_FALSE(EventItemListed(ITEM_CARROT, EVENT_ITEM_MODE_ROD));

    fishing::ChooseRodNumber(rod);
    EXPECT_EQ(fishing::ChosenRod(), "chronicle:sea_rod");
    fishing::ChooseRodNumber(ITEM_CARROT); // not a rod: nothing changes
    EXPECT_EQ(fishing::ChosenRod(), "chronicle:sea_rod");
    fishing::ChooseRodNumber(ITEM_FISHING_ROD);
    EXPECT_EQ(fishing::ChosenRod(), "");
    item::Unregister("chronicle:sea_rod");
}

TEST_F(FishingBait, ASpeciesThatDoesNotNameABaitLikesItAtItsDefault) {
    AddLure(false, 0.4f);
    const fishing::Species *species = fishing::SpeciesOfKind(5);
    EXPECT_EQ(species->Affinity("chronicle:spinner", fishing::BaitAt(fishing::BaitIndex(item::kCustomFirst + 40))->default_affinity), 0.4f);
    EXPECT_EQ(species->Affinity("vanilla:poisonous_apple", 0.4f), 1.0f); // its own liking wins
    EXPECT_EQ(species->Affinity("vanilla:carrot", 0.0f), 0.0f);
}

} // namespace dc::test
