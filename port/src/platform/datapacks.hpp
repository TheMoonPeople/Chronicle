#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "platform/vfs.hpp"

/**
 * Datapacks (docs/ITEM_FRAMEWORK.md): folders in datapacks/ beside the data folder holding files at the
 * paths they replace. datapacks/datapacks.json lists the active ones, {"schema_version": 1,
 * "load_order": [...]}, later packs winning. A listed pack the player's datapacks/ lacks is looked for
 * among the packs the game ships.
 */
namespace dc::platform::datapacks {

/**
 * Where the packs the game ships are: assets/datapacks beside the executable.
 */
std::filesystem::path ShippedDirectory();

/**
 * Loads datapacks/datapacks.json beside data_root, a listed pack from that datapacks/ or else from
 * shipped. A bad configuration disables every pack and a listed pack found in neither is skipped, each
 * with a diagnostic.
 */
void Load(const std::filesystem::path &data_root, const std::filesystem::path &shipped = ShippedDirectory());

/**
 * Forgets every pack.
 */
void Clear();

/**
 * Every pack file at a relative data path, ignoring ASCII case, highest priority first; empty for a
 * path no pack holds or one that is not a safe relative path.
 */
std::vector<vfs::Ref> Find(std::string_view relative);

/**
 * The names of the files directly in a relative directory across every pack, ignoring ASCII case, in
 * name order; empty when no pack has the directory or it is not a safe relative path.
 */
std::vector<std::string> Members(std::string_view directory);

} // namespace dc::platform::datapacks
