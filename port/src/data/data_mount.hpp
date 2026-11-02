#pragma once

#include <memory>

#include "platform/vfs.hpp"

namespace dc::data {

/**
 * The game's data (the extracted folder or data.zip, as the data reader finds it) as a Kazarov mount,
 * for Kazarov's loaders that look files up in order through vfs: a datapack's file, then this one.
 * Entries are added as they are looked up.
 */
std::shared_ptr<const vfs::Mount> DataMount();

} // namespace dc::data
