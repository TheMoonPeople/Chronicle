#include <gtest/gtest.h>

#include <set>
#include <string>

#include "mainselect.hpp"
#include "menu_draw.hpp"
#include "render/texture_registry.hpp"
#include "text/caption_text.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::render;
using namespace dc::text;

TEST(Caption, ColoursAreThePaintedEnglishOnes) {
    EXPECT_EQ(caption::kFill[0], 232);
    EXPECT_EQ(caption::kFill[1], 226);
    EXPECT_EQ(caption::kFill[2], 183);
    EXPECT_EQ(caption::kOutline[0], 1);
    EXPECT_EQ(caption::kOutline[1], 0);
    EXPECT_EQ(caption::kOutline[2], 201);
}

TEST(Caption, EveryIconHasItsOwnString) {
    const auto            strings = caption::Strings();
    std::set<std::string> keys;
    ASSERT_EQ(strings.size(), static_cast<size_t>(caption::kIcons));
    for (const auto &[key, english] : strings) {
        EXPECT_TRUE(keys.insert(key).second) << key;
        EXPECT_EQ(key.rfind("menu.caption.", 0), 0U) << key;
        EXPECT_FALSE(english.empty()) << key;
    }
}

TEST(Caption, PreparePaintedLeavesOtherTexturesAlone) {
    PortDecodedTexture texture;
    texture.width = 4;
    texture.height = 4;
    texture.format = gfx::TextureFormat::Rgba8;
    texture.levels.emplace_back(std::size_t{4} * 4 * 4, 200);
    caption::PreparePainted("other", texture);
    for (uint8_t byte : texture.levels[0]) {
        EXPECT_EQ(byte, 200);
    }
}

TEST(Caption, PaintedBoundsFollowEachLanguagesSpriteAndTextureScale) {
    const int language = LanguageCode;
    for (int lang : {LANG_ENGLISH_US, LANG_ENGLISH_UK, LANG_FRENCH, LANG_GERMAN, LANG_ITALIAN, LANG_SPANISH}) {
        LanguageCode = lang;
        for (int scale : {1, 2}) {
            for (auto format : {gfx::TextureFormat::Index8, gfx::TextureFormat::Rgba8}) {
                PortDecodedTexture texture;
                texture.width = 640 * scale;
                texture.height = 240 * scale;
                texture.format = format;
                const int bytes = format == gfx::TextureFormat::Index8 ? 1 : 4;
                texture.palette[1] = 0xFFE8E2B7;
                texture.levels.emplace_back(texture.width * texture.height * bytes, 0);
                const MENU_ICON_INFO &info = *GetMenuIconInfo(0);
                for (int y = (info.label_v + 4) * scale; y < (info.label_v + 24) * scale; y++) {
                    for (int x = (info.label_u + 3) * scale; x < (info.label_u + 33) * scale; x++) {
                        const size_t at = (static_cast<size_t>(y) * texture.width + x) * bytes;
                        texture.levels[0][at + bytes - 1] = bytes == 1 ? 1 : 255;
                    }
                }
                const auto original = texture.levels;
                caption::PreparePainted("stayframe", texture);
                const auto bounds = caption::PaintedBounds(0);
                ASSERT_TRUE(bounds.has_value()) << lang << "/" << scale;
                const float rows = static_cast<float>(info.label_height - 1) / info.label_height;
                EXPECT_FLOAT_EQ(bounds->left, 3);
                EXPECT_FLOAT_EQ(bounds->top, 4 * rows);
                EXPECT_FLOAT_EQ(bounds->width, 30);
                EXPECT_FLOAT_EQ(bounds->height, 20 * rows);
                EXPECT_EQ(texture.levels, original);
            }
        }
    }
    LanguageCode = language;
}

TEST(Caption, MissingPaintedWordsClearThePreviousBounds) {
    PortDecodedTexture texture;
    texture.width = 640;
    texture.height = 240;
    texture.format = gfx::TextureFormat::Rgba8;
    texture.levels.emplace_back(640 * 240 * 4, 255);
    caption::PreparePainted("stayframe", texture);
    ASSERT_TRUE(caption::PaintedBounds(0));
    std::fill(texture.levels[0].begin(), texture.levels[0].end(), 0);
    caption::PreparePainted("stayframe", texture);
    EXPECT_FALSE(caption::PaintedBounds(0));
    EXPECT_FALSE(caption::PaintedBounds(-1));
    EXPECT_FALSE(caption::PaintedBounds(caption::kIcons));
}

} // namespace dc::test
