#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "dungeonparts.hpp"

namespace dungeon_visibility {
// Only ordinary room/corridor walls are occluders. Doors, dividers, stairs and
// unfamiliar parts stay open: their geometry/state may allow a view through.
inline unsigned Walls(int part, int rotation) {
    constexpr unsigned road[5][4] = {
        {6, 9, 6, 9}, {5, 3, 10, 12}, {0, 0, 0, 0},
        {1, 2, 8, 4}, {7, 11, 14, 13}
    };
    if (part >= 0 && part <= MAP_PARTS_ROAD_DEAD_END)
        return road[part][static_cast<unsigned>(rotation) % 4];
    unsigned mask = 0;
    if (part >= MAP_PARTS_ROOM_WALL_NORTH && part <= MAP_PARTS_ROOM_WALL_WEST) {
        constexpr unsigned walls[] = {1, 2, 8, 4};
        mask = walls[part - MAP_PARTS_ROOM_WALL_NORTH];
    } else if (part >= MAP_PARTS_ROOM_CORNER_NORTH_WEST && part <= MAP_PARTS_ROOM_CORNER_SOUTH_WEST) {
        constexpr unsigned corners[] = {5, 3, 10, 12};
        mask = corners[part - MAP_PARTS_ROOM_CORNER_NORTH_WEST];
    }
    for (unsigned r = 0; r < static_cast<unsigned>(rotation) % 4; ++r)
        mask = ((mask & 1) ? 2 : 0) | ((mask & 2) ? 8 : 0) |
               ((mask & 8) ? 4 : 0) | ((mask & 4) ? 1 : 0);
    return mask;
}

// Conservative 2D portal visibility. Propagate angular windows through whole
// open cell edges, in increasing Manhattan distance from the camera. Straight
// sight rays are monotone on both grid axes; walking around a corner cannot
// propagate visibility. Each quadrant is separate to handle the atan2 seam.
// A one-cell halo retains walls/trim/models extending across a tile boundary.
template<int Grid, class Cell>
std::array<bool, Grid * Grid> Build(const Cell *cells, float x, float z) {
    std::array<bool, Grid * Grid> visible{};
    const auto fail_open = [&] { visible.fill(true); return visible; };
    if (!std::isfinite(x) || !std::isfinite(z)) return fail_open();
    const float gx = (x + 80.0f) / 160.0f, gz = (z + 80.0f) / 160.0f;
    if (gx < 0 || gz < 0 || gx >= Grid || gz >= Grid) return fail_open();
    const int sx = static_cast<int>(gx), sz = static_cast<int>(gz);
    const int start = sx + sz * Grid;
    if (cells[start].parts_no == MAP_PARTS_NONE) return fail_open();
    // At a boundary the camera can straddle either piece. Avoid ambiguous
    // zero-length portal projections and use the existing renderer there.
    if (gx - sx < 0.002f || gx - sx > 0.998f ||
        gz - sz < 0.002f || gz - sz > 0.998f) return fail_open();
    constexpr float pi = 3.14159265358979323846f;
    constexpr int dx[] = {0, 1, -1, 0}, dz[] = {-1, 0, 0, 1};
    constexpr unsigned bit[] = {1, 2, 4, 8}, opposite[] = {8, 4, 2, 1};
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
        std::array<float, Grid * Grid> low, high;
        low.fill(pi + 1); high.fill(-pi - 1);
        std::array<int, Grid * Grid> queue;
        int head = 0, tail = 1;
        queue[0] = start;
        low[start] = -pi + quadrant * (pi / 2);
        high[start] = low[start] + pi / 2;
        visible[start] = true;
        while (head < tail) {
            const int here = queue[head++], col = here % Grid, row = here / Grid;
            const int depth = std::abs(col - sx) + std::abs(row - sz);
            const unsigned walls = Walls(cells[here].parts_no, cells[here].direction);
            for (int side = 0; side < 4; ++side) {
                const int nc = col + dx[side], nr = row + dz[side];
                if (nc < 0 || nr < 0 || nc >= Grid || nr >= Grid || (walls & bit[side])) continue;
                if (std::abs(nc - sx) + std::abs(nr - sz) != depth + 1) continue;
                const int next = nc + nr * Grid;
                if (cells[next].parts_no == MAP_PARTS_NONE) continue;
                float ax, az, bx, bz;
                if (dx[side]) {
                    ax = bx = col + (dx[side] > 0 ? 1.0f : 0.0f);
                    az = static_cast<float>(row); bz = row + 1.0f;
                } else {
                    az = bz = row + (dz[side] > 0 ? 1.0f : 0.0f);
                    ax = static_cast<float>(col); bx = col + 1.0f;
                }
                float a = std::atan2(az - gz, ax - gx), b = std::atan2(bz - gz, bx - gx);
                if (a > b) std::swap(a, b);
                const auto pass = [&](float left, float right) {
                    left -= 0.00001f; right += 0.00001f;
                    left = std::max(left, low[here]); right = std::min(right, high[here]);
                    if (left > right) return;
                    visible[next] = true; // retain the first wall facing the camera
                    if (Walls(cells[next].parts_no, cells[next].direction) & opposite[side]) return;
                    if (low[next] > high[next]) queue[tail++] = next;
                    low[next] = std::min(low[next], left);
                    high[next] = std::max(high[next], right);
                };
                if (b - a > pi) { pass(-pi, a); pass(b, pi); }
                else pass(a, b);
            }
        }
    }
    const auto core = visible;
    for (int row = 0; row < Grid; ++row) for (int col = 0; col < Grid; ++col) {
        if (!core[col + row * Grid]) continue;
        for (int r = std::max(0, row - 1); r <= std::min(Grid - 1, row + 1); ++r)
            for (int c = std::max(0, col - 1); c <= std::min(Grid - 1, col + 1); ++c)
                visible[c + r * Grid] = true;
    }
    return visible;
}
} // namespace dungeon_visibility
