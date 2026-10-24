#pragma once

#include "common.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

// A shop the port opens through retail's item shop screen, with goods, prices, a shopkeeper and
// lines of its own (docs/QUEST_FRAMEWORK.md, "Shops"). Retail's shops keep theirs: the screen asks
// for its goods, shopkeeper and lines through functions the port answers for the custom shop while
// it is open, and only the buying prices, which the screen reads from retail's table, are laid over
// that table for the visit.

// The shopkeeper's lines a custom shop can give, by when the screen shows them.
enum class CustomShopLine {
    Greeting,       // Opening the shop.
    Browsing,       // The cursor on the shop's goods.
    Selling,        // The cursor on the player's own items.
    NotEnoughMoney, // A purchase the player cannot pay for.
    NoRoom,         // A purchase the player has no room for.
    Confirm,        // Asking to settle up.
    Thanks,         // A purchase made.
    Goodbye,        // Leaving.
};

struct CustomShop {
    // The retail item shop the screen is laid out as: where its shopkeeper stands, and the lines this
    // shop does not give.
    int look = 0;
    // Item and buying price.
    std::vector<std::pair<int, int>> goods;
    // The shopkeeper: a character file of the game's data, a villager's town model
    // ("gedit/e01/chara/p13a.chr") or a shopkeeper's ("commenu/shopchara/p02a.pac"). Empty, or a file
    // the data does not have or that is too large for the screen, is look's own shopkeeper.
    std::string keeper;
    // Markup text, as GameTextEncode reads it.
    std::map<CustomShopLine, std::string> lines;
};

// Makes shop the one the shop screen opens next: gives the shop number to open it with
// (EdSetShopNo). It stays until CustomShopEnd.
int CustomShopBegin(CustomShop shop);

// The shop screen closed (CommonShopLoop): retail's prices are put back.
void CustomShopEnd();

bool CustomShopActive();

s16 *PortGetItemShopList(int shop_no);
void PortItemShopGetPacFileName(int shop_kind, int shop_no, char *file_name);
void PortItemShopGetImgFileName(int shop_kind, int shop_no, char *file_name);
