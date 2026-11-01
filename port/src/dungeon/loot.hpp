#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <vector>

/**
 * What a dungeon's floors give (docs/DUNGEON.md, "Loot"): the treasure boxes' item lists, the clown's two
 * offers, and how often each item is turned down when drawn. Read from gameplay/dungeon/loot.json as the
 * item documents are, patched by each pack's loot.patch.json, or as compiled (dungeonparts.cpp's tables).
 */
namespace dc::dungeon {

/**
 * The floor a box list is for: one floor, counted from one, or either half of the dungeon.
 */
inline constexpr int kLowerFloors = 256; /**< The floors before the dungeon's midpoint (floorNum). */
inline constexpr int kUpperFloors = 255; /**< The floors from its midpoint on. */

/**
 * One list a treasure box draws from: small boxes draw what is not a weapon, large boxes weapons.
 */
struct ChestList {
    int              floor = kLowerFloors;
    std::vector<int> items; /**< Item numbers. */
};

/**
 * The clown's two offers, each drawn from its own list, before and from the midpoint.
 */
struct ClownLists {
    std::array<std::vector<int>, 2> lower;
    std::array<std::vector<int>, 2> upper;
};

/**
 * One dungeon's loot, for its floors and for its back floors.
 */
struct DungeonLoot {
    std::map<int, int>                    turned_down; /**< Item number to its chance out of a hundred of being turned down; 0 when left out. */
    std::array<std::vector<ChestList>, 2> chests;      /**< The floors' lists, then the back floors'. */
    std::array<ClownLists, 2>             clown;       /**< The floors' offers, then the back floors'. */
};

/**
 * Reads the loot: the highest-priority usable document, then the data's own, or as compiled; then every
 * pack's patch. The items load first (item IDs name the loot's items).
 */
void LoadLoot();

/**
 * A dungeon's loot (0 to 6, as selectMapNo numbers them); dungeon 0's for any other number.
 */
const DungeonLoot &Loot(int dungeon);

/**
 * The chance out of a hundred that a dungeon turns an item down when a box or the clown draws it.
 */
int TurnedDown(int dungeon, int item);

/**
 * Writes gameplay/dungeon/loot.json as compiled under directory. False when it cannot be written.
 */
bool ExportLoot(const std::filesystem::path &directory);

} // namespace dc::dungeon
