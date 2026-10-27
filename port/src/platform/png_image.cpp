#include "png_image.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string_view>
#include <utility>

#include "platform/load_work_profile.hpp"

// The decoder itself is compiled once, in Kazarov's platform/glyphs.cpp.
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace dc::platform {

namespace {

constexpr std::size_t kDecodeCacheBytes = 64 * 1024 * 1024;

using Pixels = std::unique_ptr<unsigned char, decltype(&stbi_image_free)>;

struct DecodedPng {
    std::size_t               hash;
    std::vector<std::uint8_t> encoded;
    Pixels                    pixels;
    int                       width;
    int                       height;
    std::size_t               rgba_bytes;

    std::size_t ResidentBytes() const { return encoded.size() + rgba_bytes; }
};

std::mutex            decode_cache_mutex;
std::list<DecodedPng> decode_cache;
std::size_t           decode_cache_bytes = 0;

bool DecodeCacheEnabled() {
    static const bool enabled = [] {
        const char *value = std::getenv("DC_PNG_CACHE");
        return value == nullptr || std::string_view(value) == "1";
    }();
    return enabled;
}

bool Matches(const DecodedPng &entry, std::size_t hash, std::span<const std::uint8_t> bytes) {
    return entry.hash == hash && std::ranges::equal(bytes, entry.encoded);
}

// Keeps the decoder's allocation rather than copying the pixels again. The encoded bytes are part
// of the key, so a hash collision or an edited file cannot return stale pixels.
void CacheDecoded(std::size_t hash, std::span<const std::uint8_t> bytes, Pixels pixels, int width, int height,
                  std::size_t rgba_bytes) {
    if (rgba_bytes > kDecodeCacheBytes || bytes.size() > kDecodeCacheBytes - rgba_bytes) {
        return;
    }
    try {
        DecodedPng entry{
            .hash = hash,
            .encoded = {bytes.begin(), bytes.end()},
            .pixels = std::move(pixels),
            .width = width,
            .height = height,
            .rgba_bytes = rgba_bytes,
        };
        const std::size_t     size = entry.ResidentBytes();
        const std::lock_guard lock(decode_cache_mutex);
        // Another thread may have decoded the same image outside the lock.
        if (std::ranges::any_of(decode_cache, [&](const DecodedPng &cached) { return Matches(cached, hash, bytes); })) {
            return;
        }
        while (decode_cache_bytes > kDecodeCacheBytes - size) {
            decode_cache_bytes -= decode_cache.back().ResidentBytes();
            decode_cache.pop_back();
        }
        decode_cache.push_front(std::move(entry));
        decode_cache_bytes += size;
    } catch (const std::bad_alloc &) {
        // The caller already has its pixels; caching is optional under memory pressure.
    }
}

} // namespace

PngDecodeCacheStats PngDecodeCacheGetStats() {
    const std::lock_guard lock(decode_cache_mutex);
    return {
        .entries = decode_cache.size(),
        .resident_bytes = decode_cache_bytes,
        .capacity_bytes = kDecodeCacheBytes,
    };
}

bool PngLoadRgba(const std::filesystem::path &file, int &width, int &height, std::vector<std::uint8_t> &rgba) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    return PngDecodeRgba(bytes, width, height, rgba);
}

bool PngDecodeRgba(std::span<const std::uint8_t> bytes, int &width, int &height, std::vector<std::uint8_t> &rgba,
                   bool *cache_hit) {
    const load_profile::Span profile(load_profile::Kind::Png);
    if (cache_hit != nullptr) {
        *cache_hit = false;
    }
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const bool  cache = DecodeCacheEnabled() && bytes.size() < kDecodeCacheBytes;
    std::size_t hash = 0;
    if (cache) {
        hash = std::hash<std::string_view>{}({reinterpret_cast<const char *>(bytes.data()), bytes.size()});
        const std::lock_guard lock(decode_cache_mutex);
        for (auto entry = decode_cache.begin(); entry != decode_cache.end(); ++entry) {
            if (!Matches(*entry, hash, bytes)) {
                continue;
            }
            rgba.assign(entry->pixels.get(), entry->pixels.get() + entry->rgba_bytes);
            width = entry->width;
            height = entry->height;
            decode_cache.splice(decode_cache.begin(), decode_cache, entry);
            if (cache_hit != nullptr) {
                *cache_hit = true;
            }
            return true;
        }
    }
    int    decoded_width = 0;
    int    decoded_height = 0;
    int    channels = 0;
    Pixels pixels(stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decoded_width, &decoded_height,
                                        &channels, 4),
                  stbi_image_free);
    if (!pixels) {
        return false;
    }
    const std::size_t rgba_bytes = static_cast<std::size_t>(decoded_width) * decoded_height * 4;
    // Caches before publishing: bytes may view rgba itself.
    std::vector<std::uint8_t> decoded(pixels.get(), pixels.get() + rgba_bytes);
    if (cache) {
        CacheDecoded(hash, bytes, std::move(pixels), decoded_width, decoded_height, rgba_bytes);
    }
    rgba = std::move(decoded);
    width = decoded_width;
    height = decoded_height;
    return true;
}

} // namespace dc::platform
