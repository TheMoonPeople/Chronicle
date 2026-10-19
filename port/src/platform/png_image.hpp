#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

// Chronicle localization and UI accept standard PNG color types and normalize
// them to RGBA, including RGB screenshots from the shared renderer.
bool PngLoadRgba(const std::filesystem::path &file, int &width, int &height, std::vector<std::uint8_t> &rgba);
