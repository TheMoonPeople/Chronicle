#pragma once

namespace dc::game {

/**
 * Dungeons in the order the game numbers them (selectMapNo); dungeon n loads
 * the dun/d0<n+1>*.cfg maps.
 */
enum Dungeon {
    DUNGEON_DIVINE_BEAST_CAVE = 0, /**< Divine Beast Cave. */
    DUNGEON_WISE_OWL_FOREST = 1,   /**< Wise Owl Forest. */
    DUNGEON_SHIPWRECK = 2,         /**< Shipwreck. */
    DUNGEON_SUN_MOON_TEMPLE = 3,   /**< Sun and Moon Temple. */
    DUNGEON_MOON_SEA = 4,          /**< Moon Sea. */
    DUNGEON_GALLERY_OF_TIME = 5,   /**< Gallery of Time; Dark Heaven Castle in the map files. */
    DUNGEON_DEMON_SHAFT = 6,       /**< Demon Shaft. */
    DUNGEON_COUNT = 7,             /**< Number of dungeons. */
};

/**
 * Map number of dungeon 0; dungeon n is map DUNGEON_MAP_BASE + n.
 */
inline constexpr int DUNGEON_MAP_BASE = 200;

/**
 * Whether map_no is one of the dungeons' map numbers.
 */
constexpr bool IsDungeonMap(int map_no) {
    return map_no >= DUNGEON_MAP_BASE && map_no < DUNGEON_MAP_BASE + DUNGEON_COUNT;
}

} // namespace dc::game
