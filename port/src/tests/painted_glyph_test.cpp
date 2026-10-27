#include <gtest/gtest.h>

#include <filesystem>

#include "gfx_fixture.hpp"
#include "text/captionfont.hpp"

namespace dc::test {

using namespace dc::text;

namespace {

std::filesystem::path FontFile() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / "tools" / "font" /
           "ChronicleSharp-Bold.ttf";
}

capfont::Paint Look() {
    return capfont::Paint{
        .depth_x = 0.5f,
        .depth_y = 0.75f,
        .stroke = 0.2f,
        .strokes = 3,
        .outline = 1.0f,
        .fill = {232, 230, 216},
        .body = {52,  56,  192},
        .ring = {0,   17,  121},
    };
}

bool Same(const capfont::Glyph &a, const capfont::Glyph &b) {
    return a.u0 == b.u0 && a.v0 == b.v0 && a.u1 == b.u1 && a.v1 == b.v1;
}

} // namespace

TEST(PaintedGlyph, MadeOnceThenTakenFromTheAtlas) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    ASSERT_TRUE(capfont::Load(FontFile()));
    capfont::Glyph first;
    capfont::Glyph again;
    ASSERT_TRUE(capfont::GetPaintedGlyph(U'A', -1, 30.0f, 36.0f, Look(), first));
    const unsigned generation = capfont::Generation();
    ASSERT_TRUE(capfont::GetPaintedGlyph(U'A', -1, 30.0f, 36.0f, Look(), again));
    EXPECT_TRUE(Same(first, again));
    EXPECT_EQ(capfont::Generation(), generation);
    // Another size is another glyph.
    ASSERT_TRUE(capfont::GetPaintedGlyph(U'A', -1, 32.0f, 36.0f, Look(), again));
    EXPECT_FALSE(Same(first, again));
}

TEST(PaintedGlyph, EveryFieldOfTheLookIsItsOwnGlyph) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    ASSERT_TRUE(capfont::Load(FontFile()));
    capfont::Paint glow = Look();
    glow.shadow_alpha = 0.5f;
    glow.shadow_soft = 4.0f;
    const std::vector<capfont::PlacedLetter> word = {
        {.ch = U'E', .x = 0.0f,  .y = 0.0f, .w = 20.0f, .h = 36.0f},
        {.ch = U'Q', .x = 22.0f, .y = 0.0f, .w = 22.0f, .h = 36.0f},
    };
    capfont::Glyph plain;
    capfont::Glyph banded;
    ASSERT_TRUE(capfont::GetPaintedWord(word, glow, plain));
    capfont::Paint band = glow;
    band.band_alpha = 0.5f;
    band.band_y = 18.0f;
    band.band_fade = 8.0f;
    ASSERT_TRUE(capfont::GetPaintedWord(word, band, banded));
    EXPECT_FALSE(Same(plain, banded));
    ASSERT_TRUE(capfont::GetPaintedGlyph(U'E', -1, 20.0f, 36.0f, glow, plain));
    ASSERT_TRUE(capfont::GetPaintedGlyph(U'E', -1, 20.0f, 36.0f, band, banded));
    EXPECT_FALSE(Same(plain, banded));
}

TEST(PaintedGlyph, AGlyphTheFontHasNotIsRefused) {
    dc::test::GfxFixture fixture(640, 480, 1.0f, dc::test::GfxOptions{.offscreen = true});
    ASSERT_TRUE(capfont::Load(FontFile()));
    capfont::Glyph glyph;
    EXPECT_FALSE(capfont::GetPaintedGlyph(U'一', -1, 30.0f, 36.0f, Look(), glyph));
    EXPECT_FALSE(capfont::GetPaintedGlyph(U'一', -1, 30.0f, 36.0f, Look(), glyph));
}

} // namespace dc::test
