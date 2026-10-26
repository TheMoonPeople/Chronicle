#pragma once

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
