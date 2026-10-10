#include <gtest/gtest.h>

#include "../fish_port.hpp"

TEST(FishSize, KeepsTheEndsAndOnlyLifts) {
    const float ranges[][2] = {{10.0f, 30.0f}, {10.0f, 16.0f}, {6.0f, 10.0f}, {8.0f, 8.3f}};
    for (const auto &[base, max] : ranges) {
        ASSERT_EQ(SmoothFishSize(base, base, max), base);
        ASSERT_EQ(SmoothFishSize(max, base, max), max);
        ASSERT_EQ(SmoothFishSize(0.75f * base, base, max), 0.75f * base); // below the usual size
        float previous = base;
        for (int i = 1; i < 1000; i++) {
            float size = base + (max - base) * i / 1000.0f;
            float lifted = SmoothFishSize(size, base, max);
            ASSERT_GE(lifted, size);
            ASSERT_LT(static_cast<int>(lifted * 10.0f), static_cast<int>(max * 10.0f)); // only a clamped roll shows the largest
            ASSERT_GE(lifted, previous);
            previous = lifted;
        }
    }
}

TEST(FishSize, LiftGrowsWithTheRange) {
    // Baron Garayan's 200 cm, Mardan Garayan's 60 and Niler's 40.
    ASSERT_NEAR(FishSizeLift(20.0f), 0.87f, 0.01f);
    ASSERT_NEAR(FishSizeLift(6.0f), 0.52f, 0.01f);
    ASSERT_NEAR(FishSizeLift(4.0f), 0.21f, 0.01f);
    ASSERT_EQ(FishSizeLift(0.3f), 0.0f);
}
