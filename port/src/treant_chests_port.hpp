#pragma once

#include "edit.hpp"
#include "objanime.hpp"

// Treant's clearing is two maps: MapNo 33 before the Matataki river floods it and MapNo 13 after
// (Matataki's exit picks one by game flag 20). Both hold the same five chests on map flags 360 to
// 364. Retail keeps each map's flags apart, so a chest opened before the flood is full again after
// it. Here those five flags are one: a chest opened in either map is open in both.
inline constexpr int kTreantClearingDry = 33;
inline constexpr int kTreantClearingFlooded = 13;
inline constexpr int kTreantChestFirstFlag = 360;
inline constexpr int kTreantChestLastFlag = 364;

// Whether flag_no on map_no is one of the clearing's shared chest flags.
bool TreantChestFlag(int map_no, int flag_no);

// The flooded clearing's three more chests: a Gourd, a Fruit of Eden and a Beast Buster, the first
// three chests' items again, each from forest progress 9, on map flags 365 to 367 and away from
// the five. Called with each part's functions as it loads; adds the chests to points beside the
// part's Gourd chest and returns how many it added (none on any other map or part).
int AddTreantChests(CMapParts *parts, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points);
