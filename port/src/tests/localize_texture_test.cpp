#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "localize.hpp"
#include "localize_texture.hpp"
#include "mainselect.hpp"
#include "platform_fixture.hpp"
#include "texture_port.hpp"

namespace {

namespace fs = std::filesystem;

// A 4x2 PNG: red, red, blue, blue over red, red, blue, transparent blue.
const unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x7f, 0xa8, 0x7d, 0x63, 0x00, 0x00, 0x00, 0x15, 0x49,
    0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x84, 0xa1, 0xd4, 0x7f, 0x06, 0x34, 0x01, 0x06,
    0x00, 0x06, 0x2a, 0x0e, 0xf2, 0x17, 0xef, 0xa6, 0x26, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82};

struct TextureDir {
    // Its own per process: ctest runs each test in a process of its own, side by side, and each clears its folder.
    fs::path dir = fs::temp_directory_path() / ("dc_localize_texture_test_" + std::to_string(dc::test::ProcessId()));

    TextureDir() {
        std::error_code error;
        fs::remove_all(dir, error);
        fs::create_directories(dir / "textures" / "fr_fr");
        LocalizeReset();
        LocalizeSetDirectories({dir});
        LanguageCode = 3;
    }

    ~TextureDir() {
        LocalizeReset();
        LocalizeSetDirectories({});
        std::error_code error;
        fs::remove_all(dir, error);
    }

    void Put(const std::string &name, const unsigned char *bytes, size_t size) {
        std::ofstream(dir / "textures" / "fr_fr" / name, std::ios::binary)
            .write(reinterpret_cast<const char *>(bytes), static_cast<std::streamsize>(size));
    }
};

PortDecodedTexture Indexed(unsigned width, unsigned height) {
    PortDecodedTexture texture;
    texture.width = width;
    texture.height = height;
    texture.format = gfx::TextureFormat::Index8;
    texture.levels.push_back(std::vector<uint8_t>(static_cast<size_t>(width) * height, 7));
    return texture;
}

TEST(LocalizeTexture, ResampleAveragesAreasAndKeepsAlphaColorsClean) {
    // 2x2 of red, red / blue, transparent green -> one pixel: the transparent green does not tint it.
    const uint8_t source[] = {255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 255, 0, 0};
    auto          out = LocalizeResample(source, 2, 2, 1, 1);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[1], 0);   // no green
    EXPECT_EQ(out[3], 191); // three quarters opaque
    EXPECT_NEAR(out[0], 170, 1);
    EXPECT_NEAR(out[2], 85, 1);
    // Same size is a copy (a transparent pixel's colour is not kept).
    auto same = LocalizeResample(source, 2, 2, 2, 2);
    EXPECT_EQ(std::vector<uint8_t>(same.begin(), same.begin() + 12), std::vector<uint8_t>(source, source + 12));
    EXPECT_EQ(same[15], 0);
}

TEST(LocalizeTexture, ShadowFallsDownRightOfTheLetteringAndNotUnderIt) {
    const int            w = 32;
    const int            h = 32;
    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4, 0);
    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    for (int y = 10; y < 18; y++) {
        for (int x = 10; x < 18; x++) {
            size_t i = static_cast<size_t>(y) * w + x;
            mask[i] = 255;
            rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = rgba[i * 4 + 3] = 255; // a white square
        }
    }
    auto alpha = [&](const std::vector<uint8_t> &image, int x, int y) { return image[(static_cast<size_t>(y) * w + x) * 4 + 3]; };

    std::vector<uint8_t> none = rgba;
    LocalizeCastShadow(none, w, h, mask, 0, 4.0f);
    EXPECT_EQ(none, rgba) << "0% casts nothing";

    std::vector<uint8_t> shadowed = rgba;
    LocalizeCastShadow(shadowed, w, h, mask, 50, 4.0f);
    EXPECT_GT(alpha(shadowed, 20, 20), 120) << "below and right of the square it is dark";
    EXPECT_EQ(shadowed[(static_cast<size_t>(20) * w + 20) * 4], 0) << "and black";
    EXPECT_EQ(alpha(shadowed, 5, 5), 0) << "none up and left";
    EXPECT_EQ(alpha(shadowed, 25, 5), 0) << "nor far to the side";
    for (int y = 10; y < 18; y++) {
        for (int x = 10; x < 18; x++) {
            size_t i = (static_cast<size_t>(y) * w + x) * 4;
            ASSERT_EQ(shadowed[i], 255) << "the letters keep their colour";
            ASSERT_EQ(shadowed[i + 3], 255);
        }
    }
    // A stronger setting is darker, and an opaque picture is darkened rather than made see-through.
    std::vector<uint8_t> deep = rgba;
    LocalizeCastShadow(deep, w, h, mask, 100, 4.0f);
    EXPECT_GE(alpha(deep, 20, 20), alpha(shadowed, 20, 20));
    std::vector<uint8_t> backdrop(static_cast<size_t>(w) * h * 4, 255);
    LocalizeCastShadow(backdrop, w, h, mask, 50, 4.0f);
    EXPECT_LT(backdrop[(static_cast<size_t>(20) * w + 20) * 4], 120);
    EXPECT_EQ(alpha(backdrop, 20, 20), 255);
    EXPECT_EQ(backdrop[(static_cast<size_t>(5) * w + 5) * 4], 255);
}

TEST(LocalizeTexture, PathPrefersTheSizedFileAndFollowsTheLanguage) {
    TextureDir files;
    EXPECT_TRUE(LocalizeTexturePath("mt01", 384, 128).empty());
    files.Put("mt01.png", kPng, sizeof kPng);
    EXPECT_EQ(LocalizeTexturePath("mt01", 384, 128).filename(), "mt01.png");
    files.Put("mt01_384x128.png", kPng, sizeof kPng);
    EXPECT_EQ(LocalizeTexturePath("mt01", 384, 128).filename(), "mt01_384x128.png");
    EXPECT_EQ(LocalizeTexturePath("mt01", 256, 64).filename(), "mt01.png");
    LanguageCode = 4; // German has no folder
    EXPECT_TRUE(LocalizeTexturePath("mt01", 384, 128).empty());
}

TEST(LocalizeTexture, SwapsAnIndexedTextureForThePicture) {
    TextureDir files;
    files.Put("card.png", kPng, sizeof kPng);
    PortDecodedTexture texture = Indexed(8, 4); // twice the picture's size, same shape
    ASSERT_TRUE(LocalizeTexture("card", texture));
    EXPECT_EQ(texture.format, gfx::TextureFormat::Rgba8);
    EXPECT_FALSE(texture.four_bit);
    EXPECT_TRUE(texture.has_alpha);
    ASSERT_EQ(texture.levels[0].size(), 8u * 4 * 4);
    EXPECT_EQ(texture.levels[0][0], 255); // (0,0) is red
    EXPECT_EQ(texture.levels[0][2], 0);
    EXPECT_EQ(texture.levels[0][(0 * 8 + 4) * 4 + 2], 255); // (4,0) is blue
    EXPECT_EQ(texture.levels[0][(3 * 8 + 7) * 4 + 3], 0);   // (7,3) is the transparent corner
}

TEST(LocalizeTexture, LeavesAWrongShapedPictureAndOtherTexturesAlone) {
    TextureDir files;
    files.Put("card.png", kPng, sizeof kPng);
    PortDecodedTexture square = Indexed(4, 4); // the picture is 2:1
    EXPECT_FALSE(LocalizeTexture("card", square));
    EXPECT_EQ(square.format, gfx::TextureFormat::Index8);
    PortDecodedTexture other = Indexed(8, 4);
    EXPECT_FALSE(LocalizeTexture("other", other));
    EXPECT_EQ(other.levels[0][0], 7);
}

} // namespace
