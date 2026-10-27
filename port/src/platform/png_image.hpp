#pragma once

#include <cstddef>
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
 * Decodes an in-memory PNG into tightly packed RGBA8, reusing a recent decode of the same bytes
 * unless DC_PNG_CACHE is set to something other than 1. bytes may view rgba itself. cache_hit, when
 * given, says whether the pixels came from that cache. Returns false, leaving the outputs untouched,
 * if the data is empty, too large or not a valid PNG.
 */
bool PngDecodeRgba(std::span<const std::uint8_t> bytes, int &width, int &height, std::vector<std::uint8_t> &rgba,
                   bool *cache_hit = nullptr);

/**
 * The size of PngDecodeRgba's cache.
 */
struct PngDecodeCacheStats {
    std::size_t entries;        /**< Decoded images held. */
    std::size_t resident_bytes; /**< Their compressed keys and decoded pixels, without allocator bookkeeping. */
    std::size_t capacity_bytes; /**< The most resident_bytes may reach. */
};

/**
 * Returns the current size of PngDecodeRgba's cache.
 */
PngDecodeCacheStats PngDecodeCacheGetStats();

} // namespace dc::platform
