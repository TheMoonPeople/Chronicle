#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

// Kazarov's imagefile::LoadPng takes only the RGBA8 files of its own texture catalog. Translations,
// the UI framework's plates and the renderer's own screenshots (RGB) are any standard PNG, so these
// are decoded here, to RGBA.
bool PngLoadRgba(const std::filesystem::path &file, int &width, int &height, std::vector<std::uint8_t> &rgba);
