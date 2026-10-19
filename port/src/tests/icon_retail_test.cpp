#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cstring>

#include "icon_retail.hpp"

namespace {
std::vector<std::uint8_t> Triangle() {
    std::vector<std::uint8_t> data;
    auto                      word = [&](std::uint32_t v, int bytes) {for(int i=0;i<bytes;++i){ data.push_back(v>>(8*i));
} };
    word(0x10000, 4);
    word(1, 4);
    word(0, 4);
    word(0, 4);
    word(3, 4);
    for (const auto &p : std::array<std::array<int, 2>, 3>{
             {{-4096, 4096}, {4096, 4096}, {0, -4096}}
    }) {
        word(p[0], 2);
        word(p[1], 2);
        word(0, 2);
        word(0, 2);
        word(0, 2);
        word(0, 2);
        word(-4096, 2);
        word(0, 2);
        word(0, 2);
        word(0, 2);
        word(0, 4);
    }
    for (int i = 0; i < 5; ++i) {
        word(0, 4);
    }
    for (int i = 0; i < 128 * 128; ++i) {
        word(31, 2);
    }
    return data;
}
} // namespace

TEST(RetailIcon, ConvertsAndRendersTriangleUsingOfficialGltfRenderer) {
    const auto raw = Triangle();
    const auto glb = RetailIconToGlb(raw);
    ASSERT_FALSE(glb.empty());
    const auto image = GameIconRender(glb.data(), glb.size(), 64, 0, 0);
    ASSERT_EQ(image.size(), 64u * 64 * 4);
    const auto center = (32 * 64 + 32) * 4;
    EXPECT_GT(image[center], 100);
    EXPECT_EQ(image[center + 1], 0);
    EXPECT_EQ(image[center + 2], 0);
    EXPECT_EQ(image[center + 3], 255);
    EXPECT_TRUE(RetailIconToGlb(std::span(raw).first(15)).empty());
    EXPECT_TRUE(RetailIconToGlb(std::span(raw).first(raw.size() - 1)).empty());
}

TEST(RetailIcon, DiscIconStillLoadsFromExistingPackWithoutReextracting) {
    const char *data = std::getenv("DC_DATA");
    if (!data) {
        GTEST_SKIP() << "Set DC_DATA to extracted retail data";
    }
    const auto source = RetailIconSource(data);
    const auto image = GameIconFromData(source, 64);
    ASSERT_EQ(image.size(), 64u * 64 * 4);
    int opaque = 0;
    for (std::size_t i = 3; i < image.size(); i += 4) {
        opaque += image[i] > 0;
    }
    EXPECT_GT(opaque, 100);
}
