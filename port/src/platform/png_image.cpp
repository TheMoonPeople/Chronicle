#include "png_image.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>

// The decoder itself is compiled once, in Kazarov's platform/glyphs.cpp.
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace dc::platform {

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

bool PngDecodeRgba(std::span<const std::uint8_t> bytes, int &width, int &height,
                   std::vector<std::uint8_t> &rgba) {
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    int                                                              decoded_width = 0;
    int                                                              decoded_height = 0;
    int                                                              channels = 0;
    const std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decoded_width, &decoded_height, &channels,
                              4),
        stbi_image_free);
    if (!pixels) {
        return false;
    }
    const std::span<const unsigned char> decoded(pixels.get(),
                                                 static_cast<std::size_t>(decoded_width) * decoded_height * 4);
    rgba.assign(decoded.begin(), decoded.end());
    width = decoded_width;
    height = decoded_height;
    return true;
}

} // namespace dc::platform
