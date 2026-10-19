#pragma once
#include <span>

#include "platform/game_icon.hpp"

// Retail icon decoding stays at Chronicle's data boundary. Rendering/installing uses upstream.
std::vector<std::uint8_t> RetailIconToGlb(std::span<const std::uint8_t> bytes);
GameIconSource            RetailIconSource(const std::filesystem::path &root);
