#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <numeric>
#include <vector>

#include "../../../tools/dcdata/sharpfont.hpp"

namespace {

namespace sharp = dcdata::sharp;

std::filesystem::path FontDir() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() / "tools" / "font";
}

std::vector<sharp::Pt> Square(double x0, double y0, double x1, double y1) {
    return {
        {x0, y0},
        {x1, y0},
        {x1, y1},
        {x0, y1}
    };
}

double Sum(const std::vector<double> &v) {
    return std::accumulate(v.begin(), v.end(), 0.0);
}

TEST(SharpFont, CoverageIsTheExactArea) {
    std::vector<double> c = sharp::Coverage({Square(2.5, 3.25, 8.5, 9.75)}, 20, 14);
    EXPECT_NEAR(Sum(c), 6 * 6.5, 1e-9);
    EXPECT_NEAR(c[3 * 14 + 2], 0.5 * 0.75, 1e-9); // the corner pixel: half its width, three quarters of its height
    EXPECT_NEAR(c[5 * 14 + 5], 1.0, 1e-9);
    // Wound the other way, a square still covers (non-zero), and one inside another does not count twice.
    std::vector<sharp::Pt> back = Square(2.5, 3.25, 8.5, 9.75);
    std::reverse(back.begin(), back.end());
    EXPECT_NEAR(Sum(sharp::Coverage({back}, 20, 14)), 6 * 6.5, 1e-9);
    EXPECT_NEAR(Sum(sharp::Coverage({Square(2, 2, 10, 10), Square(4, 4, 6, 6)}, 20, 14)), 64, 1e-9);
}

TEST(SharpFont, WeightMovesBothEdgesOfACorner) {
    std::vector<sharp::Pt> square = Square(2, 2, 10, 10);
    for (double outer : {1.0, -1.0}) {
        std::vector<sharp::Pt> moved = sharp::Offset(square, std::vector<double>(4, 0.5), outer);
        double                 area = Sum(sharp::Coverage({moved}, 20, 14));
        EXPECT_TRUE(std::abs(area - 81) < 1e-9 || std::abs(area - 49) < 1e-9) << area; // 9x9 grown, 7x7 thinned
    }
}

TEST(SharpFont, WrittenFontReadsBack) {
    sharp::Ttf font = sharp::ReadTtf(dcdata::ReadFile(FontDir() / "DarkCloudCompendium.ttf"));
    ASSERT_TRUE(font.cmap.contains(U'A'));
    const int      a = font.cmap.at(U'A');
    sharp::Contour box;
    box.pts = {
        {0,   0  },
        {300, 0  },
        {300, 500},
        {0,   500}
    };
    box.on = {true, true, true, true};
    std::vector<std::uint8_t> bytes = sharp::WriteTtf(font, {
                                                                {a, {box}}
    },
                                                      1.0, 650, 750, 250);
    sharp::Ttf                back = sharp::ReadTtf(bytes);
    EXPECT_EQ(back.cmap, font.cmap);
    ASSERT_EQ(back.glyphs.size(), font.glyphs.size());
    ASSERT_EQ(back.glyphs[static_cast<std::size_t>(a)].contours.size(), 1u);
    const sharp::Contour &read = back.glyphs[static_cast<std::size_t>(a)].contours[0];
    ASSERT_EQ(read.pts.size(), 4u);
    EXPECT_EQ(read.pts[2].x, 300);
    EXPECT_EQ(read.pts[2].y, 500);
    EXPECT_EQ(back.glyphs[static_cast<std::size_t>(a)].advance, 650);
    // Every other glyph keeps its points.
    const int b = font.cmap.at(U'B');
    EXPECT_EQ(back.glyphs[static_cast<std::size_t>(b)].contours.size(), font.glyphs[static_cast<std::size_t>(b)].contours.size());
    EXPECT_FALSE(back.tables.contains("fpgm"));
    // The whole file sums to the magic number the head table's adjustment makes it.
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        sum += static_cast<std::uint32_t>(bytes[i]) << 24 | bytes[i + 1] << 16 | bytes[i + 2] << 8 | bytes[i + 3];
    }
    EXPECT_EQ(sum, 0xB1B0AFBAu);
}

TEST(SharpFont, WarpFindsALetterDrawnByAWarp) {
    // A cell drawn from the font's own o, moved and slanted: the warp fit should land on it.
    sharp::Ttf                  font = sharp::ReadTtf(dcdata::ReadFile(FontDir() / "DarkCloudCompendium.ttf"));
    std::vector<sharp::Contour> o = font.Of(U'o').contours;
    sharp::Letter               source = sharp::MakeLetter(o, std::vector<double>(14 * 20, 0.0));
    sharp::Model                truth{&source, 0.017, 5, 4, 1, 1, true};
    std::vector<double>         v(static_cast<std::size_t>(truth.Size()), 0.0);
    v[0] = 1.05;
    v[1] = 0.95;
    v[2] = 0.15;
    v[3] = 2.5;
    v[4] = 15.5;
    v[5] = 0.2;
    std::vector<double> cell = sharp::Coverage(truth.Polys(v), 20, 14);
    sharp::Letter       letter = sharp::MakeLetter(o, cell);
    double              ox, oy;
    sharp::Model        model{&letter, sharp::StartScale(letter, ox, oy), 5, 4, 1, 1, true};
    std::vector<double> fit = sharp::FitWarp(model);
    std::vector<double> drawn = sharp::Coverage(model.Polys(fit), 20, 14);
    double              error = 0;
    for (std::size_t i = 0; i < drawn.size(); i++) {
        error += std::abs(drawn[i] - cell[i]);
    }
    EXPECT_LT(error / static_cast<double>(drawn.size()), 0.01);
}

} // namespace
