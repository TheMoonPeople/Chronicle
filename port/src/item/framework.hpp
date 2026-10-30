#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
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
    float radius = 0.0f;           /**< Distance from which fish notice the bait. */
    bool  lure = false;            /**< Back to the bag after a catch rather than eaten, and never lost. */
    float default_affinity = 0.0f; /**< The liking of a species that doesn't name it. */
};

/**
 * A fishing rod's five stats, Dark Cloud 2's rod numbers under names for what each does. Only the
 * fishing stages that read rods (Enhanced's brisk and tension) use them; the defaults, retail's rod's,
 * change nothing.
 */
struct Rod {
    float heft = 50.0f;            /**< The fish caught run larger: 0.8 to 1.2 times. */
    float control = 100.0f / 3.0f; /**< The stick held the fish's way tires it more. */
    float spring = 10.0f;          /**< The fish pulls less, above 10. */
    float balance = 100.0f;        /**< Scales heft, control and spring: 0.8 to 1 times. */
    float lure = 0.0f;             /**< Fewer rolls of nothing biting, and less bait lost to a fish that gets away. */
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
 * What one copy of an item carries that retail has no field for, by name.
 */
using State = std::map<std::string, double, std::less<>>;

/**
 * An item: an ID in data, a number at run time, and its traits.
 */
struct Def {
    std::string id;                /**< namespace:name, such as vanilla:bread. */
    int         number = -1;       /**< The number retail's code and saves know the item by. */
    State       state;             /**< What a new copy starts with; empty where every copy is alike. */
    int         state_version = 0; /**< The version of state's keys, for migrating saved copies. */

    std::tuple<std::optional<Consumable>, std::optional<Throwable>, std::optional<QuickSlot>,
               std::optional<Attachment>, std::optional<Weapon>, std::optional<Bait>, std::optional<Rod>, std::optional<Fish>,
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
 * Forgets one item; nothing for an ID not registered.
 */
void Unregister(std::string_view id);

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

// A copy with state of its own travels through retail's code under a handle, a number of its own
// where retail keeps item numbers (docs/ITEM_FRAMEWORK.md, "Numbers and copies"). Handles stop short
// of 0x8000 because the game looks a description up at the number + 500, an s16.
inline constexpr int kHandleFirst = 0x1000; /**< The first handle. */
inline constexpr int kHandleEnd = 0x7E00;   /**< One past the last. */

/**
 * One copy of an item with state of its own.
 */
struct Copy {
    std::string   id;           /**< Its item's ID. */
    int           number = -1;  /**< Its item's number, or -1 while no item has the ID. */
    std::uint64_t instance = 0;      /**< Its identity for good, kept in the save; never 0. */
    State         state;             /**< Its state. */
    int           state_version = 0; /**< The state_version of the definition that wrote its state. */
};

/**
 * Makes a copy of an item that has state, with a new instance and the item's starting state. Returns
 * its handle, or -1 for an item without state or with every handle taken.
 */
int NewCopy(const Def &item);

/**
 * Puts back a copy kept in a save under a free handle. The item need not be registered. Returns the
 * handle, or -1 with every handle taken.
 */
int RestoreCopy(std::string_view id, std::uint64_t instance, State state, int state_version);

/**
 * The copy under a handle, or null.
 */
Copy *FindCopy(int handle);

/**
 * The number of the item behind a number: a handle's item's, else the number itself; -1 for a handle
 * nothing holds or whose item is not registered.
 */
int Definition(int number);

/**
 * Frees every handle for which held is false.
 */
void ReclaimCopies(const std::function<bool(int handle)> &held);

/**
 * How many handles are taken.
 */
int CopyCount();

/**
 * Gives each copy its item's number again, after the items registered anew.
 */
void RebindCopies();

/**
 * Forgets every copy, and restarts instances at next.
 */
void ClearCopies(std::uint64_t next = 1);

/**
 * The instance the next new copy gets, for the save to keep.
 */
std::uint64_t NextInstance();

/**
 * Takes an instance no copy has, for a copy found twice in the save.
 */
std::uint64_t TakeInstance();

} // namespace dc::item
