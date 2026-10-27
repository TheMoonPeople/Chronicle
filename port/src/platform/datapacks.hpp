#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "platform/vfs.hpp"

/**
 * Datapacks (docs/ITEM_FRAMEWORK.md): folders in datapacks/ beside the data folder holding files at the
 * paths they replace. datapacks/datapacks.json lists the active ones, {"schema_version": 1,
 * "load_order": [...]}, later packs winning.
 */
namespace dc::platform::datapacks {

/**
 * Loads datapacks/datapacks.json beside data_root. A bad configuration disables every pack and a
 * listed pack that is missing is skipped, each with a diagnostic.
 */
void Load(const std::filesystem::path &data_root);

/**
 * Forgets every pack.
 */
void Clear();

/**
 * Every pack file at a relative data path, ignoring ASCII case, highest priority first; empty for a
 * path no pack holds or one that is not a safe relative path.
 */
std::vector<vfs::Ref> Find(std::string_view relative);

} // namespace dc::platform::datapacks
