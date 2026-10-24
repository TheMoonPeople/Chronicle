#pragma once

#include <span>

#include "platform/game_icon.hpp"

// Kazarov draws the icon from glTF, but dcdata extracts the disc's as it is: a PS2 model in each language's
// commenu/<language>/savetex.pak. It is converted here, in memory, with nothing written to the data.
std::vector<std::uint8_t> RetailIconToGlb(std::span<const std::uint8_t> bytes);
GameIconSource            RetailIconSource(const std::filesystem::path &root);
