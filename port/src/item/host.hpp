#pragma once

#include <filesystem>

/**
 * Retail's item tables as gameplay documents (docs/ITEM_FRAMEWORK.md): ComItemInfo with each item's
 * model name, ITEM_LIST, AttachList, WeaponList, the baits and PriceList.
 */
namespace dc::item {

/**
 * Fills every table from the highest-priority usable datapack document, or as compiled where there is
 * none, then registers every vanilla item as the tables now hold it. datapacks::Load runs first.
 */
void Load();

/**
 * Writes the tables' documents as compiled under directory/gameplay/, pretty-printed, as a starting
 * point for a datapack. Returns false when one cannot be written.
 */
bool Export(const std::filesystem::path &directory);

} // namespace dc::item
