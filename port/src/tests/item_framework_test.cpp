#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "item/framework.hpp"

namespace dc::test {

namespace {

class ItemFramework : public ::testing::Test {
protected:
    void SetUp() override { item::Clear(); }

    void TearDown() override { item::Clear(); }

    static item::Def Item(std::string id, int number) { return {.id = std::move(id), .number = number}; }
};

TEST_F(ItemFramework, ItemsAreFoundByIdAndByNumber) {
    item::Register(Item("test:bread", 148).With(item::Sellable{.buy = 20, .sell = 10}));
    const item::Def *bread = item::Find("test:bread");
    ASSERT_NE(bread, nullptr);
    EXPECT_EQ(item::Find(148), bread);
    EXPECT_EQ(item::Find("test:cake"), nullptr);
    EXPECT_EQ(item::Find(149), nullptr);
    ASSERT_NE(bread->Get<item::Sellable>(), nullptr);
    EXPECT_EQ(bread->Get<item::Sellable>()->buy, 20);
    EXPECT_FALSE(bread->Has<item::Bait>());
}

TEST_F(ItemFramework, TheRegistryRefusesItemsThatCannotWork) {
    item::Register(Item("test:bread", 148));
    EXPECT_THROW(item::Register(Item("test:bread", 149)), std::logic_error);
    EXPECT_THROW(item::Register(Item("test:cake", 148)), std::logic_error);
    EXPECT_THROW(item::Register(Item("bread", 150)), std::logic_error);
    EXPECT_THROW(item::Register(Item("Test:Cake", 150)), std::logic_error);
    EXPECT_THROW(item::Register(Item("test:", 150)), std::logic_error);
    EXPECT_THROW(item::Register(Item("test:sword", 150).With(item::Weapon{}).With(item::Consumable{})), std::logic_error);
    EXPECT_THROW(item::Register(Item("test:sword", 150).With(item::Weapon{}).With(item::Attachment{})), std::logic_error);
    EXPECT_EQ(item::All().size(), 1u);
}

TEST_F(ItemFramework, ItemsWithATraitComeInNumberOrder) {
    item::Register(Item("test:minon", 188).With(item::Bait{.radius = 30.0f}));
    item::Register(Item("test:bread", 148));
    item::Register(Item("test:carrot", 186).With(item::Bait{.radius = 20.0f}).With(item::Consumable{}));
    const auto baits = item::With<item::Bait>();
    ASSERT_EQ(baits.size(), 2u);
    EXPECT_EQ(baits[0]->id, "test:carrot");
    EXPECT_EQ(baits[1]->id, "test:minon");
    EXPECT_EQ(item::All().front()->id, "test:bread");

    item::Clear();
    EXPECT_TRUE(item::All().empty());
    EXPECT_EQ(item::Find("test:bread"), nullptr);
}

} // namespace

} // namespace dc::test
