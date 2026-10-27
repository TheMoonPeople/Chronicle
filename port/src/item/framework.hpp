#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

/**
 * Items (docs/ITEM_FRAMEWORK.md). An item is a Def: its ID, its number and the traits that say what it
 * can do, so a system asks whether an item has the trait it cares about rather than what type it is.
 * Like quest/framework.hpp, this header includes no PS2 headers.
 */
namespace dc::item {

/**
 * Using the item uses it up.
 */
struct Consumable {
    bool heals_hp = false;       /**< Restores HP. */
    bool restores_water = false; /**< Restores water. */
    bool grants_status = false;  /**< Grants a status effect. */
    int  volume = 0;             /**< The uses a fresh copy starts with. */
    int  volume_range = 0;       /**< The random variation on volume. */
};

/**
 * The item can be thrown.
 */
struct Throwable {};

/**
 * How copies of an item sit in a quick-use slot.
 */
enum class Stack : uint8_t {
    Multi,  /**< Copies stack in one slot. */
    Single, /**< Each copy sits alone. */
};

/**
 * The item may go in a quick-use slot.
 */
struct QuickSlot {
    Stack stack = Stack::Multi; /**< Whether copies stack there. */
};

/**
 * What the attachment adds to a weapon.
 */
struct Attachment {
    int                 attack = 0;      /**< Attack added. */
    int                 endurance = 0;   /**< Endurance added. */
    int                 speed = 0;       /**< Speed added. */
    int                 magic = 0;       /**< Magic added. */
    std::array<int, 5>  elements = {};   /**< Fire, ice, thunder, wind and holy. */
    std::array<int, 10> vs_monster = {}; /**< One per monster family, in WEAPON_DATA's order. */
};

/**
 * A weapon's starting stats.
 */
struct Weapon {
    int owner = 0;      /**< The character: 0 Toan, 1 Xiao, 2 Goro, 3 Ruby, 4 Ungaga, 5 Osmond. */
    int durability = 0; /**< Durability a new copy starts with. */
    int attack = 0;     /**< Attack a new copy starts with. */
    int endurance = 0;  /**< Endurance a new copy starts with. */
    int speed = 0;      /**< Speed a new copy starts with. */
    int magic = 0;      /**< Magic a new copy starts with. */
    int attack_max = 0; /**< The most attack the weapon reaches. */
    int magic_max = 0;  /**< The most magic the weapon reaches. */
    int holes = 0;      /**< Attachment sockets. */
};

/**
 * Fish notice the bait from radius away.
 */
struct Bait {
    float radius = 0.0f; /**< Distance from which fish notice the bait. */
};

/**
 * A fish the player keeps as an item.
 */
struct Fish {};

/**
 * What a shop charges and pays for the item.
 */
struct Sellable {
    int buy = 0;  /**< Money a shop charges. */
    int sell = 0; /**< Money a shop pays. */
};

/**
 * A story item.
 */
struct KeyItem {};

/**
 * A dungeon's quest item.
 */
struct QuestItem {};

/**
 * An item: an ID in data, a number at run time, and its traits.
 */
struct Def {
    std::string id;          /**< namespace:name, such as vanilla:bread. */
    int         number = -1; /**< The number retail's code and saves know the item by. */

    std::tuple<std::optional<Consumable>, std::optional<Throwable>, std::optional<QuickSlot>,
               std::optional<Attachment>, std::optional<Weapon>, std::optional<Bait>, std::optional<Fish>,
               std::optional<Sellable>, std::optional<KeyItem>, std::optional<QuestItem>>
        traits; /**< Every trait the item has. */

    /**
     * The item's trait T, or null when it does not have it.
     */
    template <class T>
    [[nodiscard]] const T *Get() const {
        const std::optional<T> &trait = std::get<std::optional<T>>(traits);
        return trait ? &*trait : nullptr;
    }

    /**
     * Whether the item has trait T.
     */
    template <class T>
    [[nodiscard]] bool Has() const {
        return Get<T>() != nullptr;
    }

    /**
     * Gives the item a trait, replacing one of the same type.
     */
    template <class T>
    Def &With(T trait) {
        std::get<std::optional<T>>(traits) = std::move(trait);
        return *this;
    }
};

/**
 * Whether an ID is namespace:name, each part lower-case letters, digits and underscores.
 */
bool ValidId(std::string_view id);

/**
 * Adds an item; throws std::logic_error for an invalid ID, an ID or number already registered, or a
 * Weapon that is also Consumable or an Attachment.
 */
void Register(Def item);

/**
 * Forgets every item.
 */
void Clear();

/**
 * The item with an ID, or null.
 */
const Def *Find(std::string_view id);

/**
 * The item with a number, or null.
 */
const Def *Find(int number);

/**
 * Every item, in number order.
 */
std::vector<const Def *> All();

/**
 * Every item with trait T, in number order.
 */
template <class T>
std::vector<const Def *> With() {
    std::vector<const Def *> items;
    for (const Def *item : All()) {
        if (item->Has<T>()) {
            items.push_back(item);
        }
    }
    return items;
}

} // namespace dc::item
