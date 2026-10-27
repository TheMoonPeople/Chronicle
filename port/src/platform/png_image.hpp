#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace dc::platform {

/**
 * Reads and decodes any standard PNG file (RGB, RGBA, paletted, ...) into tightly packed RGBA8.
 * Returns false, leaving the outputs untouched, if the file cannot be read or decoded.
 */
bool PngLoadRgba(const std::filesystem::path &file, int &width, int &height, std::vector<std::uint8_t> &rgba);

/**
 * Decodes an in-memory PNG into tightly packed RGBA8.
 * Returns false, leaving the outputs untouched, if the data is empty, too large or not a valid PNG.
 */
bool PngDecodeRgba(std::span<const std::uint8_t> bytes, int &width, int &height, std::vector<std::uint8_t> &rgba);

} // namespace dc::platform

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::platform;
#endif
// DC_TRANSITIONAL_END
