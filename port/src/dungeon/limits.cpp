#include "dungeon/limits.hpp"

#include <array>

#include "dngstatusdata.hpp"
#include "userstatus.hpp"

namespace dc::dungeon {

using namespace dc::game;

namespace {

/**
 * A floor a dungeon's script always limits.
 */
struct FixedLimit {
    int dungeon;
    int floor; /**< Counted from 0, as cur_floor. */
    int limit; /**< A ResLimitZone. */
};

// Retail's floor scripts (dun/script/dNN/event.stb, program 100) set these with _SET_LIMMIT_ZONE as
// the floor starts, after _SET_RES_LIMMIT_ZONE has applied any rolled one, so they win over it.
constexpr std::array kFixedLimits = {
    FixedLimit{0, 6,  RES_LIMIT_ZONE_NO_WEAPON_CHANGE},
    FixedLimit{0, 10, RES_LIMIT_ZONE_XIAO            },
    FixedLimit{1, 3,  RES_LIMIT_ZONE_XIAO            },
    FixedLimit{1, 11, RES_LIMIT_ZONE_GORO            },
    FixedLimit{3, 4,  RES_LIMIT_ZONE_GORO            },
    FixedLimit{3, 12, RES_LIMIT_ZONE_UNGAGA          },
};

/**
 * The floors of each dungeon the scripts build from a fixed map, the boss's included, where
 * _SET_RES_LIMMIT_ZONE is not reached, so a limit rolled for them never applies. Counted from 0.
 */
constexpr std::array<std::array<int, 3>, 6> kFixedMapFloors = {
    {
     {3, 7, 14},
     {8, 16, -1},
     {8, 17, -1},
     {8, 17, -1},
     {7, 14, -1},
     {24, -1, -1},
     }
};

} // namespace

int FloorLimit(const CDngStatusData &status, int dungeon, int floor) {
    if (dungeon < 0 || dungeon >= static_cast<int>(kFixedMapFloors.size()) || floor < 0) {
        return RES_LIMIT_ZONE_NONE;
    }
    for (const FixedLimit &fixed : kFixedLimits) {
        if (fixed.dungeon == dungeon && fixed.floor == floor) {
            return fixed.limit;
        }
    }
    for (const int fixed_map : kFixedMapFloors[dungeon]) {
        if (fixed_map == floor) {
            return RES_LIMIT_ZONE_NONE;
        }
    }
    if (floor >= static_cast<int>(std::size(status.res_limit_zone_id[dungeon]))) {
        return RES_LIMIT_ZONE_NONE;
    }
    // char is unsigned on arm64, where -1 would come back as 255.
    return static_cast<signed char>(status.res_limit_zone_id[dungeon][floor]);
}

int FloorLimitMessage(int limit, int &insert) {
    insert = -1;
    // As CDngMessageMan::LimmitZone picks them; a character's name is message 0x32 on.
    if (limit >= RES_LIMIT_ZONE_TOAN && limit <= RES_LIMIT_ZONE_OSMOND) {
        insert = 0x32 + limit;
        return 0xB;
    }
    if (limit == RES_LIMIT_ZONE_NO_WEAPON_CHANGE) {
        return 0xD;
    }
    if (limit == RES_LIMIT_ZONE_THIRST) {
        return 0xE;
    }
    return -1;
}

} // namespace dc::dungeon
