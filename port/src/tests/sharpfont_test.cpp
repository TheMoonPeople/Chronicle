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

TEST(SharpFont, FontLaysEachLetterOnItsCell) {
    // Chronicle Sharp draws the sheet's letters where the sheet has them: inside the 14x20 cell (a curve's control
    // points may stand a little outside it), its capitals as tall as the game's.
    for (const char *file : {"ChronicleSharp-Bold.ttf", "ChronicleSharp-Regular.ttf"}) {
        std::vector<sharp::Shaped> set = sharp::FontSet(sharp::ReadTtf(dcdata::ReadFile(FontDir() / file)));
        ASSERT_EQ(set.size(), sharp::kOrder.size());
        for (std::size_t i = 0; i < set.size(); i++) {
            ASSERT_FALSE(set[i].contours.empty()) << file << " " << sharp::kOrder[i];
            for (const sharp::Contour &c : set[i].contours) {
                for (const sharp::Pt &p : c.pts) {
                    EXPECT_TRUE(p.x >= -2 && p.x <= sharp::kCellW + 2 && p.y >= -2 && p.y <= sharp::kCellH + 2)
                        << file << " " << sharp::kOrder[i] << " " << p.x << "," << p.y;
                }
            }
        }
        double top = 1e9, bottom = -1e9;
        for (const sharp::Contour &c : set[sharp::kOrder.find('H')].contours) {
            for (const sharp::Pt &p : c.pts) {
                top = std::min(top, p.y);
                bottom = std::max(bottom, p.y);
            }
        }
        EXPECT_GT(bottom - top, 12.0) << file;
    }
}

} // namespace
