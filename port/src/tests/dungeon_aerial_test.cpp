#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include "gfx_fixture.hpp"
#include "dungeon_aerial_cache.hpp"
#include "draw3d.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"

TEST(DungeonAerialCache, DiscoveryIdentityAndChestsInvalidateFloor) {
    auto floor = std::make_unique<CDungeonMap>();
    const auto key = [&] { return DungeonAerialFloorKey(*floor, 0, 400, 8, 0); };
    floor->mask[5] = 1;
    auto first = key();
    floor->mask[5] = 0; floor->mask[6] = 1;
    EXPECT_NE(first, key()); // same revealed count, different cells
    first = key();
    floor->boxes[dungeon_grid::boxes - 1].used = 1;
    floor->boxes[dungeon_grid::boxes - 1].closed = 1;
    EXPECT_NE(first, key());
    first = key();
    floor->boxes[dungeon_grid::boxes - 1].closed = 0;
    EXPECT_NE(first, key());
    first = key();
    EXPECT_EQ(first, key());
    EXPECT_NE(first, DungeonAerialFloorKey(*floor, 0, 40, 8, 0));
    EXPECT_NE(first, DungeonAerialFloorKey(*floor, 0, 400, 12, 0));
}

TEST(DungeonAerialProjection, WallTopsStayOnTheirFloorTileAndPerspectiveIsRestored) {
    dc::test::GfxFixture gfx(640, 480, 1, dc::test::GfxOptions{.offscreen = true});
    const RenderInfo saved = mgRenderInfo;
    MGSetRenderInfo(1000, 3600, 8000);
    float perspective[4][4], overhead[4][4], restored[4][4];
    Draw3DEyeToClip(mgRenderInfo, perspective);
    {
        MGPortOrthographicScope scope(4000);
        Draw3DEyeToClip(mgRenderInfo, overhead);
        const auto project = [&](float x, float z, int axis) {
            return (overhead[0][axis] * x + overhead[2][axis] * z + overhead[3][axis]) /
                   (overhead[0][3] * x + overhead[2][3] * z + overhead[3][3]);
        };
        EXPECT_FLOAT_EQ(project(3000, 4000, 0), project(3000, 3600, 0));
        EXPECT_FLOAT_EQ(project(-3000, 4000, 0), project(-3000, 3600, 0));
        EXPECT_NEAR(project(0, 3600, 2), 1.0f, 0.00001f);
        EXPECT_NEAR(project(0, 8000, 2), 0.0f, 0.00001f);
        {
            MGPortOrthographicScope nested(2000);
            EXPECT_EQ(MGPortOrthographicDepth(), 2000);
        }
        EXPECT_EQ(MGPortOrthographicDepth(), 4000);
    }
    EXPECT_EQ(MGPortOrthographicDepth(), 0);
    Draw3DEyeToClip(mgRenderInfo, restored);
    EXPECT_EQ(std::memcmp(perspective, restored, sizeof restored), 0);
    mgRenderInfo = saved;
}
