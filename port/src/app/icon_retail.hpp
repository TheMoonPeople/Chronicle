#pragma once

#include <span>

#include "platform/game_icon.hpp"

namespace dc::app {

/**
 * Converts a retail PS2 save icon (as stored in commenu/<language>/savetex.pak) to an in-memory glTF
 * binary; returns an empty vector when the icon is malformed.
 */
std::vector<std::uint8_t> RetailIconToGlb(std::span<const std::uint8_t> bytes);

/**
 * Builds a game icon source that reads the disc's own icon from the data under root and converts it in
 * memory, writing nothing back to the data.
 */
GameIconSource RetailIconSource(const std::filesystem::path &root);

} // namespace dc::app
