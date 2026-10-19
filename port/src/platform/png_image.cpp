#include "png_image.hpp"

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>

// Reuse Kazarov's decoder implementation in platform/glyphs.cpp. Its published
// texture catalog requires RGBA8 files, but Chronicle's existing UI/localization
// contract also accepts RGB, palette and grayscale PNGs.
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

bool PngLoadRgba(const std::filesystem::path &file, int &width, int &height, std::vector<std::uint8_t> &rgba) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return false;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    int                                                        decoded_width = 0, decoded_height = 0, channels = 0;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &decoded_width, &decoded_height, &channels, 4),
        stbi_image_free);
    if (!pixels) {
        return false;
    }
    rgba.assign(pixels.get(), pixels.get() + static_cast<std::size_t>(decoded_width) * decoded_height * 4);
    width = decoded_width;
    height = decoded_height;
    return true;
}
