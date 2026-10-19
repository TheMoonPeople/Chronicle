#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "editloop.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "platform_fixture.hpp"
#include "texture_assets.hpp"
#include "texture_port.hpp"

extern s32 mode;

namespace {

namespace fs = std::filesystem;

// A 4x2 PNG: red, red, blue, blue over red, red, blue, transparent blue.
const unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x7f, 0xa8, 0x7d, 0x63, 0x00, 0x00, 0x00, 0x15, 0x49,
    0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x84, 0xa1, 0xd4, 0x7f, 0x06, 0x34, 0x01, 0x06,
    0x00, 0x06, 0x2a, 0x0e, 0xf2, 0x17, 0xef, 0xa6, 0x26, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82};

struct Assets {
    fs::path dir = fs::temp_directory_path() / ("dc_texture_assets_test_" + std::to_string(dc::test::ProcessId()));
    s32      saved_mode = mode;
    s32      saved_menu = main_select_menu_no;
    s32      saved_language = LanguageCode;
    char     saved_map[sizeof EditMapName];

    Assets() {
        std::memcpy(saved_map, EditMapName, sizeof saved_map);
        std::error_code error;
        fs::remove_all(dir, error);
        fs::create_directories(dir / "first");
        fs::create_directories(dir / "second");
        TextureAssetsSetRoots({dir / "first", dir / "second"});
    }

    ~Assets() {
        TextureAssetsSetRoots({});
        mode = saved_mode;
        main_select_menu_no = saved_menu;
        LanguageCode = saved_language;
        std::memcpy(EditMapName, saved_map, sizeof saved_map);
        std::error_code error;
        fs::remove_all(dir, error);
    }

    // `where` is "first" or "second", `path` e.g. "d02/textures/card.png".
    void Put(const std::string &where, const std::string &path, const unsigned char *bytes = kPng,
             size_t size = sizeof kPng) {
        fs::path file = dir / where / path;
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char *>(bytes), static_cast<std::streamsize>(size));
    }

    void InDungeon(int index) {
        mode = GAME_MODE_DUNGEON;
        main_select_menu_no = index;
    }

    void InTown(const char *map) {
        mode = GAME_MODE_EDIT;
        std::strncpy(EditMapName, map, sizeof EditMapName - 1);
        EditMapName[sizeof EditMapName - 1] = 0;
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

TEST(TextureAssets, TheAreaComesFromTheGameMode) {
    Assets files;
    files.InDungeon(0);
    EXPECT_EQ(TextureAssetsArea(), "d01");
    files.InDungeon(6);
    EXPECT_EQ(TextureAssetsArea(), "d07");
    files.InDungeon(7);
    EXPECT_EQ(TextureAssetsArea(), "");
    files.InTown("e03");
    EXPECT_EQ(TextureAssetsArea(), "e03");
    mode = GAME_MODE_TITLE;
    EXPECT_EQ(TextureAssetsArea(), "");
}

TEST(TextureAssets, TheAreaFolderComesBeforeTheSharedOneAndTheSaveFolderBeforeTheExecutable) {
    Assets files;
    files.Put("second", "d02/textures/card.png");
    files.Put("first", "textures/card.png");
    EXPECT_EQ(TextureAssetPath("d02", "", "card", 4, 2), files.dir / "first" / "textures" / "card.png");
    EXPECT_EQ(TextureAssetPath("d03", "", "card", 4, 2), files.dir / "first" / "textures" / "card.png");
    files.Put("first", "d02/textures/card.png");
    EXPECT_EQ(TextureAssetPath("d02", "", "card", 4, 2), files.dir / "first" / "d02" / "textures" / "card.png");
    EXPECT_EQ(TextureAssetPath("d02", "", "other", 4, 2), fs::path());
}

TEST(TextureAssets, ALanguagesPictureComesAfterTheAreasAndBeforeTheSharedOne) {
    Assets files;
    files.Put("first", "textures/menu.png");
    files.Put("first", "a_fre/textures/menu.png");
    EXPECT_EQ(TextureAssetPath("", "a_fre", "menu", 4, 2), files.dir / "first" / "a_fre" / "textures" / "menu.png");
    EXPECT_EQ(TextureAssetPath("", "a_eng", "menu", 4, 2), files.dir / "first" / "textures" / "menu.png");
    files.Put("first", "d01/textures/menu.png");
    EXPECT_EQ(TextureAssetPath("d01", "a_fre", "menu", 4, 2), files.dir / "first" / "d01" / "textures" / "menu.png");
    LanguageCode = 3;
    EXPECT_EQ(TextureAssetsLanguage(), "a_fre");
    LanguageCode = 9;
    EXPECT_EQ(TextureAssetsLanguage(), "");
}

TEST(TextureAssets, ASizedNameIsTriedFirstAndAnUnsafeNameFindsNothing) {
    Assets files;
    files.Put("first", "textures/card.png");
    files.Put("first", "textures/card_8x4.png");
    EXPECT_EQ(TextureAssetPath("", "", "card", 8, 4), files.dir / "first" / "textures" / "card_8x4.png");
    EXPECT_EQ(TextureAssetPath("", "", "card", 2, 1), files.dir / "first" / "textures" / "card.png");
    EXPECT_EQ(TextureAssetPath("", "", "../card", 2, 1), fs::path());
    EXPECT_EQ(TextureAssetPath("../first", "", "card", 2, 1), files.dir / "first" / "textures" / "card.png");
}

TEST(TextureAssets, AnAreasPictureIsUsedOnlyInThatArea) {
    Assets files;
    files.Put("first", "d02/textures/card.png");
    PortDecodedTexture texture = Indexed(8, 4);
    files.InDungeon(0);
    EXPECT_FALSE(TextureAssetApply("card", texture));
    EXPECT_EQ(texture.format, gfx::TextureFormat::Index8);
    files.InDungeon(1);
    EXPECT_TRUE(TextureAssetApply("card", texture));
    EXPECT_EQ(texture.format, gfx::TextureFormat::Rgba8);
}

TEST(TextureAssets, ASmallerPictureIsScaledUpToTheTexturesSize) {
    Assets files;
    files.Put("first", "textures/card.png");
    files.InDungeon(0);
    PortDecodedTexture texture = Indexed(8, 4);
    ASSERT_TRUE(TextureAssetApply("card", texture));
    EXPECT_EQ(texture.width, 8u);
    EXPECT_EQ(texture.height, 4u);
    EXPECT_EQ(texture.logical_width, 8u);
    EXPECT_EQ(texture.logical_height, 4u);
    EXPECT_TRUE(texture.has_alpha);
    ASSERT_EQ(texture.levels[0].size(), 8u * 4 * 4);
    EXPECT_EQ(texture.levels[0][0], 255);
    EXPECT_EQ(texture.levels[0][(0 * 8 + 4) * 4 + 2], 255);
    EXPECT_EQ(texture.levels[0][(3 * 8 + 7) * 4 + 3], 0);
}

TEST(TextureAssets, AHigherResolutionPictureKeepsItsPixelsAndTheGameKeepsTheOriginalSize) {
    Assets files;
    files.Put("first", "textures/card.png");
    files.InDungeon(0);
    PortDecodedTexture texture = Indexed(2, 1); // the picture is twice that a side
    texture.levels.push_back(std::vector<uint8_t>(1, 7));
    ASSERT_TRUE(TextureAssetApply("card", texture));
    EXPECT_EQ(texture.width, 4u);
    EXPECT_EQ(texture.height, 2u);
    EXPECT_EQ(texture.logical_width, 2u);
    EXPECT_EQ(texture.logical_height, 1u);
    ASSERT_EQ(texture.levels.size(), 2u);
    EXPECT_EQ(texture.levels[0].size(), 4u * 2 * 4);
    EXPECT_EQ(texture.levels[1].size(), 2u * 1 * 4);
    EXPECT_EQ(texture.levels[0][0], 255);                  // (0,0) red
    EXPECT_EQ(texture.levels[0][(0 * 4 + 2) * 4 + 2], 255); // (2,0) blue
    // A picture is not applied twice.
    EXPECT_FALSE(TextureAssetApply("card", texture));
}

TEST(TextureAssets, AWrongShapedOrUnreadablePictureIsLeftAlone) {
    Assets files;
    files.Put("first", "textures/card.png");
    const unsigned char junk[] = {1, 2, 3};
    files.Put("first", "textures/junk.png", junk, sizeof junk);
    files.InDungeon(0);
    PortDecodedTexture square = Indexed(4, 4); // the picture is 2:1
    EXPECT_FALSE(TextureAssetApply("card", square));
    EXPECT_EQ(square.format, gfx::TextureFormat::Index8);
    EXPECT_EQ(square.logical_width, 0u);
    PortDecodedTexture other = Indexed(8, 4);
    EXPECT_FALSE(TextureAssetApply("junk", other));
    EXPECT_FALSE(TextureAssetApply("missing", other));
    EXPECT_EQ(other.levels[0][0], 7);
}

} // namespace
