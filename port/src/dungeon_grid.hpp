#pragma once
#include <type_traits>
#include "dungeonmap.hpp"

namespace dungeon_grid {
inline constexpr int cells = std::extent_v<decltype(CDungeonMap::cells)>;
inline constexpr int boxes = std::extent_v<decltype(CDungeonMap::boxes)>;
inline constexpr int side = [] { int n = 1; while (n * n < cells) ++n; return n; }();
static_assert(side * side == cells);
}
