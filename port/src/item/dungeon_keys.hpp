#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>

#include "item/framework.hpp"

namespace dc::item {

/**
 * The dungeon keys by item ID, from gameplay/items/dungeon_keys.json: the highest-priority usable
 * document, then the data's own, or as compiled. Load gives each item its DungeonKey trait, and reports
 * an ID no loaded item has.
 */
std::map<std::string, DungeonKey, std::less<>> LoadDungeonKeys();

/**
 * Writes gameplay/items/dungeon_keys.json as compiled under directory. False when it cannot be written.
 */
bool ExportDungeonKeys(const std::filesystem::path &directory);

} // namespace dc::item
