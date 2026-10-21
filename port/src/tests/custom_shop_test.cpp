#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "clsmes.hpp"
#include "custom_shop.hpp"
#include "dataread_port.hpp"
#include "gametext.hpp"
#include "itemdata.hpp"
#include "menuetc.hpp"
#include "platform/paths.hpp"
#include "shop.hpp"

// ps2/src/shop.cpp's prices, as custom_shop.cpp declares them.
struct ITEM_MONEY_ENTRY {
    s16 buy_price;
    s16 sell_price;
};

extern ITEM_MONEY_ENTRY PriceList[296];

namespace {

std::array<s16, 20> Goods(int shop_no) {
    std::array<s16, 20> goods{};
    std::copy_n(GetItemShopList(shop_no), goods.size(), goods.begin());
    return goods;
}

s16 &BuyPrice(int item) {
    return PriceList[item - ITEM_ATTACH_START].buy_price;
}

std::string PacName(int shop_kind, int shop_no, std::string_view expected) {
    auto name = std::make_unique<char[]>(expected.size() + 1);
    ItemShopGetPacFileName(shop_kind, shop_no, name.get());
    return name.get();
}

// Whether the game's data is here. Looking a file up indexes PathsDataRoot() first, and that stops
// the program (InitCDFile) where it is not a directory or holds nothing.
bool HaveGameData() {
    std::error_code              error;
    const std::filesystem::path &root = PathsDataRoot();
    return std::filesystem::is_directory(root, error) && !std::filesystem::is_empty(root, error);
}

std::string ImageName(int shop_kind, int shop_no, std::string_view expected) {
    auto name = std::make_unique<char[]>(expected.size() + 1);
    ItemShopGetImgFileName(shop_kind, shop_no, name.get());
    return name.get();
}

class CustomShopTest : public ::testing::Test {
protected:
    void TearDown() override { CustomShopEnd(); }
};

TEST_F(CustomShopTest, SellsItsGoodsAtItsPricesOnlyWhileOpen) {
    const std::array<s16, 20> retail = Goods(0);
    const s16                 price = BuyPrice(ITEM_TRAM_OIL);

    CustomShop shop;
    shop.look = 0;
    shop.goods = {
        {ITEM_TRAM_OIL, 600}
    };
    EXPECT_EQ(CustomShopBegin(shop), 0);
    EXPECT_TRUE(CustomShopActive());
    EXPECT_EQ(GetItemShopList(0)[0], ITEM_TRAM_OIL);
    EXPECT_EQ(GetItemShopList(0)[1], -1);
    EXPECT_EQ(BuyPrice(ITEM_TRAM_OIL), 600);

    CustomShopEnd();
    EXPECT_FALSE(CustomShopActive());
    EXPECT_EQ(Goods(0), retail);
    EXPECT_EQ(BuyPrice(ITEM_TRAM_OIL), price);
}

TEST_F(CustomShopTest, LeavesRetailShopsAsTheyAre) {
    const std::string pac = PacName(SHOP_KIND_ITEM, 0, "commenu/shopchara/p02a.pac");
    EXPECT_EQ(pac, "commenu/shopchara/p02a.pac");
    EXPECT_EQ(ImageName(SHOP_KIND_ITEM, 0, "p02a01.img"), "p02a01.img");
    // Retail read past its eighteen lists for the numbers ShopNoInput lets through.
    EXPECT_EQ(Goods(20), Goods(0));
}

TEST_F(CustomShopTest, AKeeperTheDataDoesNotHaveIsLooksOwn) {
    if (!HaveGameData()) {
        GTEST_SKIP() << "the game's data is not here";
    }
    CustomShop shop;
    shop.look = 0;
    shop.keeper = "gedit/none/chara/zz99a.chr";
    CustomShopBegin(shop);
    EXPECT_EQ(PacName(SHOP_KIND_ITEM, 0, "commenu/shopchara/p02a.pac"), "commenu/shopchara/p02a.pac");
    EXPECT_EQ(ImageName(SHOP_KIND_ITEM, 0, "p02a01.img"), "p02a01.img");
}

TEST_F(CustomShopTest, ATownModelStandsBehindTheCounter) {
    const char *villager = "gedit/e01/chara/p13a.chr";
    if (!HaveGameData() || DataFileSize(villager) == 0) {
        GTEST_SKIP() << "the game's data is not here";
    }
    CustomShop shop;
    shop.look = 0;
    shop.keeper = villager;
    CustomShopBegin(shop);
    EXPECT_EQ(PacName(SHOP_KIND_ITEM, 0, villager), villager);
    EXPECT_EQ(ImageName(SHOP_KIND_ITEM, 0, "p13a01.img"), "p13a01.img");
    // The storage shop is not the custom shop's.
    EXPECT_EQ(PacName(SHOP_KIND_CHARGE, 0, "commenu/shopchara/p13a.pac"), "commenu/shopchara/p13a.pac");
}

TEST_F(CustomShopTest, ItsLinesAreTheShopkeepersWhileOpen) {
    CustomShop shop;
    shop.look = 3;
    shop.lines[CustomShopLine::Greeting] = "Hello there.";
    shop.lines[CustomShopLine::Confirm] = "All set?";
    CustomShopBegin(shop);

    // Item shop 3's lines: 4000 + 3 * 100, and an offset for the moment.
    short *greeting = CommonMenuMes3.GetTextLineDataTop(4300);
    ASSERT_NE(greeting, nullptr);
    EXPECT_EQ(GameTextDecode(greeting), "Hello there.");
    for (int confirm : {4370, 4371, 4372}) {
        short *text = CommonMenuMes3.GetTextLineDataTop(confirm);
        ASSERT_NE(text, nullptr);
        EXPECT_EQ(GameTextDecode(text), "All set?");
    }
}

} // namespace
