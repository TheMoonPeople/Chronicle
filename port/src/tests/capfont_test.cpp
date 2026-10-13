#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "captionfont.hpp"

namespace {

struct Rendered {
    std::vector<unsigned char> alpha;
    int                        w = 0;
    int                        h = 0;
    int                        x0 = 0;
    int                        y0 = 0;
};

bool LoadTestFont() {
    const char *path = std::getenv("DC_TEST_FONT");
    if (path != nullptr && *path != '\0') {
        return capfont::Load(path);
    }
    return capfont::Load(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / "tools" / "font" /
                         "DarkCloudCompendium.ttf");
}

Rendered Render(char32_t ch, int em_px = 40) {
    Rendered out;
    EXPECT_TRUE(capfont::RenderGlyph(ch, em_px, out.alpha, out.w, out.h, out.x0, out.y0)) << static_cast<unsigned>(ch);
    if (std::getenv("DC_TEST_FONT_DUMP") != nullptr) {
        std::printf("U+%04X  %dx%d at (%d,%d)\n", static_cast<unsigned>(ch), out.w, out.h, out.x0, out.y0);
        for (int row = 0; row < out.h; row++) {
            for (int col = 0; col < out.w; col++) {
                const unsigned char a = out.alpha[static_cast<size_t>(row) * out.w + col];
                std::putchar(a > 160 ? '#' : a > 64 ? '+'
                                                    : ' ');
            }
            std::putchar('\n');
        }
    }
    return out;
}

} // namespace

TEST(CaptionFont, AccentedLettersAreBuiltFromTheFontsOwnGlyphs) {
    if (!LoadTestFont()) {
        GTEST_SKIP() << "DC_TEST_FONT is not set";
    }
    const Rendered e = Render(U'e');
    for (char32_t accented : {U'é', U'è', U'ê', U'ë'}) {
        const Rendered r = Render(accented);
        EXPECT_GT(r.w, 0);
        EXPECT_LT(r.y0, e.y0) << "the mark stands above the letter";
        EXPECT_EQ(r.y0 + r.h, e.y0 + e.h) << "the letter keeps its bottom";
        EXPECT_NEAR(r.x0 + r.w / 2.0, e.x0 + e.w / 2.0, 2.0) << "the mark is centred on the letter";
    }
    const Rendered c = Render(U'c');
    const Rendered cedilla = Render(U'ç');
    EXPECT_GT(cedilla.y0 + cedilla.h, c.y0 + c.h) << "the cedilla hangs below the baseline";
    EXPECT_EQ(cedilla.y0, c.y0);

    const Rendered upper = Render(U'É');
    const Rendered plain_upper = Render(U'E');
    EXPECT_LT(upper.y0, plain_upper.y0);
}

TEST(CaptionFont, InvertedMarksAndLigaturesAreBuilt) {
    if (!LoadTestFont()) {
        GTEST_SKIP() << "DC_TEST_FONT is not set";
    }
    const Rendered bang = Render(U'!');
    const Rendered inverted = Render(U'¡');
    EXPECT_EQ(inverted.w, bang.w);
    EXPECT_EQ(inverted.h, bang.h);
    EXPECT_EQ(inverted.alpha.front(), bang.alpha[bang.alpha.size() - static_cast<size_t>(bang.w)]);

    const Rendered oe = Render(U'œ');
    const Rendered o = Render(U'o');
    EXPECT_GT(oe.w, o.w);
    EXPECT_LT(oe.w, 2 * o.w);
    Render(U'Œ');
    Render(U'Ñ');
    Render(U'ñ');
    Render(U'ä');
}

TEST(CaptionFont, WhatItCannotBuildIsLeftToTheBitmap) {
    if (!LoadTestFont()) {
        GTEST_SKIP() << "DC_TEST_FONT is not set";
    }
    Rendered out;
    EXPECT_FALSE(capfont::RenderGlyph(U'ß', 40, out.alpha, out.w, out.h, out.x0, out.y0));
    EXPECT_FALSE(capfont::RenderGlyph(U'~' + 0x1000, 40, out.alpha, out.w, out.h, out.x0, out.y0));
}
