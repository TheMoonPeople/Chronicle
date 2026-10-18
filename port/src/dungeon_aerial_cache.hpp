#pragma once
#include <bit>
#include <cstdint>
#include "dungeon_grid.hpp"

// Hash actual revealed cells, rather than their count. Unsigned arithmetic is
// intentional: the old signed multiply hash overflowed on every normal floor.
inline std::uint64_t DungeonAerialFloorKey(const CDungeonMap &map, int flags, float ceiling,
                                         float model_scale, float player_y) {
    std::uint64_t key = 14695981039346656037ull;
    const auto mix = [&](std::uint64_t value) { key = (key ^ value) * 1099511628211ull; };
    mix(static_cast<std::uint32_t>(map.map_seed)); mix(flags);
    mix(std::bit_cast<std::uint32_t>(ceiling));
    mix(std::bit_cast<std::uint32_t>(model_scale));
    mix(std::bit_cast<std::uint32_t>(player_y));
    for (int i = 0; i < dungeon_grid::cells; ++i) {
        mix(static_cast<std::uint32_t>(map.cells[i].parts_no));
        mix(map.cells[i].direction); mix(map.mask[i] != 0);
    }
    for (int i = 0; i < dungeon_grid::boxes; ++i) {
        const auto &box = map.boxes[i];
        mix(box.used); mix(box.closed); mix(box.kind);
        if (box.used && box.closed) for (int axis = 0; axis < 3; ++axis)
            mix(std::bit_cast<std::uint32_t>(box.pos[axis]));
    }
    for (const auto &atla : map.atra) {
        mix(atla.used);
        if (atla.used) for (int axis = 0; axis < 3; ++axis)
            mix(std::bit_cast<std::uint32_t>(atla.pos[axis]));
    }
    return key;
}
