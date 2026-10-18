#include <gtest/gtest.h>
#include "dungeon_visibility.hpp"
#include "dungeonmap.hpp"

namespace {
constexpr int grid = 16;
struct Floor {
    std::array<MAP_CELL, grid * grid> cells{};
    Floor() { for (auto &cell : cells) cell.parts_no = MAP_PARTS_NONE; }
    void put(int x, int z, int part = MAP_PARTS_ROOM_FLOOR, int direction = 0) {
        cells[x + z * grid].parts_no = part;
        cells[x + z * grid].direction = direction;
    }
    auto view(float x, float z) { return dungeon_visibility::Build<grid>(cells.data(), x * 160, z * 160); }
};
}

TEST(DungeonVisibility, StraightCorridorStaysVisibleInAllDirections) {
    Floor f;
    for (int x = 1; x < 15; ++x) f.put(x, 8, MAP_PARTS_ROAD_STRAIGHT, 1);
    EXPECT_TRUE(f.view(2, 8)[14 + 8 * grid]);
    EXPECT_TRUE(f.view(14, 8)[1 + 8 * grid]); // atan2 seam
    Floor vertical;
    for (int z = 1; z < 15; ++z) vertical.put(8, z, MAP_PARTS_ROAD_STRAIGHT);
    EXPECT_TRUE(vertical.view(8, 2)[8 + 14 * grid]);
    EXPECT_TRUE(vertical.view(8, 14)[8 + 1 * grid]);
}

TEST(DungeonVisibility, DoesNotSeeAroundCorridorBend) {
    Floor f;
    for (int x = 2; x < 11; ++x) f.put(x, 3, MAP_PARTS_ROAD_STRAIGHT, 1);
    f.put(10, 3, MAP_PARTS_ROAD_CORNER, 1); // west/south open
    for (int z = 4; z < 14; ++z) f.put(10, z, MAP_PARTS_ROAD_STRAIGHT);
    auto visible = f.view(3, 3);
    EXPECT_TRUE(visible[10 + 3 * grid]);
    EXPECT_FALSE(visible[10 + 12 * grid]);
}

TEST(DungeonVisibility, AdjacentOccupiedCellsDoNotTransmitThroughWalls) {
    Floor f;
    for (int z = 2; z < 14; ++z) for (int x = 2; x < 14; ++x)
        f.put(x, z, x == 5 ? MAP_PARTS_ROOM_WALL_EAST : MAP_PARTS_ROOM_FLOOR);
    auto visible = f.view(3, 7);
    EXPECT_TRUE(visible[5 + 7 * grid]);
    EXPECT_FALSE(visible[10 + 7 * grid]);
}

TEST(DungeonVisibility, OpenRoomAndDoorRemainVisible) {
    Floor f;
    for (int z = 1; z < 15; ++z) for (int x = 1; x < 15; ++x) f.put(x, z);
    f.put(5, 7, MAP_PARTS_ROOM_DOOR_EAST);
    auto visible = f.view(3, 7);
    for (int z = 1; z < 15; ++z) for (int x = 1; x < 15; ++x)
        EXPECT_TRUE(visible[x + z * grid]) << x << "," << z;
}

TEST(DungeonVisibility, InvalidAndEmptyCameraOriginsFailOpen) {
    Floor f;
    for (auto v : {f.view(-0.6f, 0), f.view(16, 0), f.view(4, 4), f.view(NAN, 0)})
        for (bool cell : v) EXPECT_TRUE(cell);
}

TEST(DungeonVisibility, WallMasksMatchBuilderTables) {
    EXPECT_EQ(dungeon_visibility::Walls(MAP_PARTS_ROAD_STRAIGHT, 1), 9u);
    EXPECT_EQ(dungeon_visibility::Walls(MAP_PARTS_ROAD_CORNER, 3), 12u);
    EXPECT_EQ(dungeon_visibility::Walls(MAP_PARTS_ROOM_WALL_NORTH, 1), 2u);
    EXPECT_EQ(dungeon_visibility::Walls(MAP_PARTS_ROOM_CORNER_NORTH_WEST, 2), 10u);
    EXPECT_EQ(dungeon_visibility::Walls(MAP_PARTS_ROOM_DOOR_NORTH, 0), 0u);
    EXPECT_EQ(dungeon_visibility::Walls(75, 0), 0u);
}
