#include <gtest/gtest.h>

#include <cmath>

#include "fishing.hpp"

TEST(FishingLine, APinnedFloatStaysFinite) {
    float rod[4] = {0.0f, 20.0f, 0.0f, 1.0f};
    float uki[4] = {10.0f, 0.0f, 10.0f, 1.0f};
    FishLineInit(rod);
    FishLineSetUki(uki, 1.0f);
    for (int frame = 0; frame < 4; frame++) {
        FishLineStep(rod, rod);
    }
    FishLineSetUki(uki, -1.0f);
    FishLineStep(rod, rod);

    float after[4];
    FishLineGetUki(after);
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(std::isfinite(after[i])) << i;
    }
}
