#pragma once

#include "common.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace dc::quest {

/**
 * The shopkeeper's lines a custom shop can give, by when the item shop screen shows them.
 */
enum class CustomShopLine {
    Greeting,       /**< Opening the shop. */
    Browsing,       /**< The cursor on the shop's goods. */
    Selling,        /**< The cursor on the player's own items. */
    NotEnoughMoney, /**< A purchase the player cannot pay for. */
    NoRoom,         /**< A purchase the player has no room for. */
    Confirm,        /**< Asking to settle up. */
    Thanks,         /**< A purchase made. */
    Goodbye,        /**< Leaving. */
};

/**
 * A shop opened through retail's item shop screen with goods, prices, a shopkeeper and lines of its own
 * (docs/QUEST_FRAMEWORK.md, "Shops"). While it is open the screen's goods, shopkeeper and lines come from it,
 * and its buying prices are laid over retail's price table for the visit. The shopkeeper is a character file
 * of the game's data, a villager's town model ("gedit/e01/chara/p13a.chr") or a shopkeeper's
 * ("commenu/shopchara/p02a.pac"); one the data lacks or too large for the screen falls back to look's own.
 */
struct CustomShop {
    int                                   look = 0; /**< Retail item shop laid out as; gives lines not set here. */
    std::vector<std::pair<int, int>>      goods;    /**< Item and buying price. */
    std::string                           keeper;   /**< Shopkeeper character file; empty means look's own. */
    std::map<CustomShopLine, std::string> lines;    /**< Markup text, as GameTextEncode reads it. */
};

/**
 * Makes shop the one the shop screen opens next and returns the shop number to open it with (EdSetShopNo).
 * It stays until CustomShopEnd.
 */
int CustomShopBegin(CustomShop shop);

/**
 * Closes the custom shop once the shop screen has (CommonShopLoop), putting retail's prices back.
 * Does nothing when no custom shop is open.
 */
void CustomShopEnd();

/**
 * Whether a custom shop is open.
 */
bool CustomShopActive();

/**
 * The item list the shop screen sells: the custom shop's while one is open, else retail shop shop_no's.
 * Lists shorter than twenty items end with -1.
 */
s16 *PortGetItemShopList(int shop_no);

/**
 * Writes the shopkeeper's character file path for shop_kind and shop_no into file_name, NUL-terminated.
 */
void PortItemShopGetPacFileName(int shop_kind, int shop_no, char *file_name);

/**
 * Writes the shopkeeper's texture file name for shop_kind and shop_no into file_name, NUL-terminated.
 */
void PortItemShopGetImgFileName(int shop_kind, int shop_no, char *file_name);

} // namespace dc::quest
