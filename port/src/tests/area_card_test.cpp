#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "gfx/context.hpp"
#include "gfx_fixture.hpp"
#include "localize/localize.hpp"
#include "mainselect.hpp"
#include "render/texture_registry.hpp"
#include "text/caption_text.hpp"
#include "text/captionfont.hpp"
#include "ui/area_card.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::localize;
using namespace dc::render;
using namespace dc::text;
using namespace dc::ui;

namespace {

struct CardFiles {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("dc_area_card_test_" + std::to_string(dc::test::ProcessId()));

    ~CardFiles() {
        area_card::Clear();
        LocalizeReset();
        LocalizeSetDirectories({});
        std::error_code error;
        std::filesystem::remove_all(dir, error);
    }
};

std::filesystem::path CaptionFontPath() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / "tools" / "font" /
           "ChronicleSharp-Bold.ttf";
}

void CheckTranslatedCard(const char *text) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    // GitHub's macOS runner GPU misreads the card's lettering; real Apple GPUs do not.
    if (std::string_view(gfx::detail::g.properties.deviceName).contains("Paravirtual")) {
        GTEST_SKIP() << "the CI runner's virtual GPU misreads the card";
    }
    CardFiles files;
    std::filesystem::create_directories(files.dir / "textures" / "plates");
    LocalizeReset();
    LocalizeSetDirectories({files.dir});
    LanguageCode = 3;
    std::ofstream(files.dir / "fr_fr.json") << R"({"area.mt01":")" << text << R"("})";
    std::ofstream(files.dir / "textures" / "plates" / "mt01.json")
        << R"({"layout":{"underline":0},"style":{"fill":[255,255,255],"edge_width":0,"glow_width":0}})";
    const uint8_t black[] = {0, 0, 0, 255};
    ASSERT_TRUE(gfx::WritePng(files.dir / "textures" / "plates" / "mt01.png", black, 1, 1));
    ASSERT_TRUE(capfont::Load(CaptionFontPath()));
    ASSERT_TRUE(caption::Ready());

    PortDecodedTexture source;
    source.width = 256;
    source.height = 64;
    source.levels.emplace_back(256 * 64 * 4, 0);
    for (int y = 12; y < 34; y++) {
        for (int first : {20, 100, 160}) {
            for (int x = first; x < first + 21; x++) {
                auto *pixel = &source.levels[0][static_cast<std::size_t>((y * 256) + x) * 4];
                pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
            }
        }
    }
    ASSERT_TRUE(area_card::Prepare("mt01", source));
    fixture.Frame({0, 0, 0, 0x80}, [] { area_card::Draw(100, 200, 356, 264, 1); });
    const auto actual = fixture.pixels;

    caption::LineStyle style;
    style.fill[0] = style.fill[1] = style.fill[2] = 255;
    style.edge_width = 0;
    style.glow_width = 0;
    style.ink_fit = true;
    style.thin = caption::ThinReady();
    fixture.Frame({0, 0, 0, 0x80}, [&] {
        EXPECT_TRUE(caption::DrawLine(text, 200.5f, 212, 22, 10, 161, style, 1, 1, nullptr));
    });
    EXPECT_EQ(actual, fixture.pixels) << "A translation must fit the painted name's complete off-center box";

    int left = 640;
    int right = -1;
    int top = 480;
    int bottom = -1;
    for (int y = 0; y < 480; y++) {
        for (int x = 0; x < 640; x++) {
            if (actual[static_cast<std::size_t>((y * 640) + x) * 4] > 4) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    EXPECT_NEAR(left, 120, 1);
    EXPECT_NEAR(right + 1, 281, 1);
    EXPECT_NEAR(top, 212, 1);
    EXPECT_NEAR(bottom + 1, 234, 1);
}

} // namespace

TEST(AreaCard, TranslatedLettersUseTheWholeRetailBox) {
    CheckTranslatedCard("III");
}

TEST(AreaCard, TranslatedWordsKeepTheRetailBoxAlignment) {
    CheckTranslatedCard("I WIDE I");
}

TEST(CaptionLine, PaintedSlotsKeepTheirWidthsAndFractionalHeight) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    ASSERT_TRUE(capfont::Load(CaptionFontPath()));
    caption::LineStyle style;
    style.edge_width = 0;
    style.ink_fit = true;
    style.slots = {
        {100, 102},
        {200, 320}
    };
    caption::LineBounds bounds;
    fixture.Frame({0, 0, 0, 0x80}, [&] {
        EXPECT_TRUE(caption::DrawLine("II", 200, 100.25f, 23.5f, 0.56f, 220, style, 1, 1, &bounds));
    });
    EXPECT_FLOAT_EQ(bounds.left, 100);
    EXPECT_FLOAT_EQ(bounds.right, 320);
    for (const auto &slot : style.slots) {
        int left = 640;
        int right = -1;
        int top = 480;
        int bottom = -1;
        for (int y = 0; y < 480; y++) {
            for (int x = static_cast<int>(slot[0]) - 20; x < static_cast<int>(slot[1]) + 20; x++) {
                if (fixture.Pixel(x, y)[0] > 4) {
                    left = std::min(left, x);
                    right = std::max(right, x);
                    top = std::min(top, y);
                    bottom = std::max(bottom, y);
                }
            }
        }
        EXPECT_NEAR(left, slot[0], 1);
        EXPECT_NEAR(right + 1, slot[1], 1);
        EXPECT_NEAR(top, 100.25f, 1);
        EXPECT_NEAR(bottom + 1, 123.75f, 1);
    }
}

} // namespace dc::test
