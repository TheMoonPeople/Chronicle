#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/png_image.hpp"
#include "platform_fixture.hpp"

namespace dc::test {

using namespace dc::platform;

namespace {

struct PngFile {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("dc_png_cache_" + std::to_string(ProcessId()) + ".png");

    ~PngFile() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::vector<std::uint8_t> Encode(const std::vector<std::uint8_t> &rgba, unsigned width, unsigned height) {
        if (!gfx::WritePng(path, rgba.data(), width, height)) {
            ADD_FAILURE() << "could not create PNG fixture";
            return {};
        }
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }
};

} // namespace

TEST(PngImage, CachePreservesPixelsAndCallerOwnership) {
    PngFile                         file;
    const std::vector<std::uint8_t> expected{255, 0, 23, 255, 12, 255, 64, 255};
    const auto                      encoded = file.Encode(expected, 2, 1);
    int                             width = 0;
    int                             height = 0;
    bool                            hit = true;
    std::vector<std::uint8_t>       rgba;
    ASSERT_TRUE(PngDecodeRgba(encoded, width, height, rgba, &hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(rgba, expected);
    rgba.assign(4, 0);
    ASSERT_TRUE(PngDecodeRgba(encoded, width, height, rgba, &hit));
    EXPECT_TRUE(hit);
    EXPECT_EQ(width, 2);
    EXPECT_EQ(height, 1);
    EXPECT_EQ(rgba, expected);

    const auto edited = file.Encode({0, 1, 2, 255}, 1, 1);
    ASSERT_TRUE(PngDecodeRgba(edited, width, height, rgba, &hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(rgba, (std::vector<std::uint8_t>{0, 1, 2, 255}));
    EXPECT_EQ(width, 1);
    EXPECT_EQ(height, 1);

    auto damaged = encoded;
    damaged[0] = 0;
    const auto previous = rgba;
    EXPECT_FALSE(PngDecodeRgba(damaged, width, height, rgba, &hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(rgba, previous);
    EXPECT_EQ(width, 1);
    EXPECT_EQ(height, 1);
}

TEST(PngImage, CacheEvictsWithoutChangingDecodedOutput) {
    PngFile                   file;
    std::vector<std::uint8_t> pixels(512 * 512 * 4, 255);
    std::vector<std::uint8_t> rgba;
    const auto                first = file.Encode(pixels, 512, 512);
    int                       width = 0;
    int                       height = 0;
    bool                      hit = false;
    ASSERT_TRUE(PngDecodeRgba(first, width, height, rgba, &hit));
    ASSERT_FALSE(hit);
    const auto capacity = PngDecodeCacheGetStats().capacity_bytes;
    const auto iterations = capacity / pixels.size() + 1;
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        pixels[0] = static_cast<std::uint8_t>(iteration);
        ASSERT_TRUE(PngDecodeRgba(file.Encode(pixels, 512, 512), width, height, rgba, &hit));
        ASSERT_EQ(rgba, pixels);
        ASSERT_LE(PngDecodeCacheGetStats().resident_bytes, capacity);
    }
    ASSERT_TRUE(PngDecodeRgba(first, width, height, rgba, &hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(rgba, std::vector<std::uint8_t>(512 * 512 * 4, 255));
}

TEST(PngImage, EncodedInputMayAliasOutput) {
    PngFile                         file;
    const std::vector<std::uint8_t> expected{19, 42, 67, 255};
    auto                            bytes = file.Encode(expected, 1, 1);
    const auto                      encoded = bytes;
    int                             width = 0;
    int                             height = 0;
    bool                            hit = true;
    ASSERT_TRUE(PngDecodeRgba(bytes, width, height, bytes, &hit));
    EXPECT_FALSE(hit);
    EXPECT_EQ(bytes, expected);
    bytes = encoded;
    ASSERT_TRUE(PngDecodeRgba(bytes, width, height, bytes, &hit));
    EXPECT_TRUE(hit);
    EXPECT_EQ(bytes, expected);
}

} // namespace dc::test
