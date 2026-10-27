#pragma once

#include "common.h"

#include "mapobject.hpp"

namespace dc::game {

/**
 * Selects the distance range used to draw one map-object category.
 */
struct CMapCategoryAttr {
    float lod[4];  /**< Distances at which the category changes level of detail. */
    s32   lowest;  /**< Lowest level of detail that the category may draw. */
    s32   highest; /**< Highest level of detail that the category may draw. */
};

STATIC_ASSERT(sizeof(CMapCategoryAttr) == 0x18);

/**
 * Owns the categories and scenery objects drawn by one map.
 */
class CMap {
public:
    CMapCategoryAttr category[16]; /**< Level-of-detail ranges for the map's object categories. */
    CMapObject       object[10];   /**< Scenery objects placed in this map. */
    s32              draw_on;      /**< Selects level-of-detail drawing when nonzero. */

    /**
     * Places a frame in a numbered object slot and returns that object, or
     * null when the slot number is invalid.
     */
    CMapObject *SetObject(int index, CFrameVu1 *frame, int category_no, int handle);

    /**
     * Places a frame in the first free object slot and returns that object,
     * or null when every slot is occupied.
     */
    CMapObject *SetObject(CFrameVu1 *frame, int category_no, int handle);

    /**
     *
     * Returns the map object in a numbered slot, or null for an invalid slot.
     *
     */
    CMapObject *GetObject(int index);

    /**
     * Draws every placed object, using its category's level-of-detail range
     * when range selection is enabled.
     */
    void Draw();

    /**
     *
     * Resets the category ranges and every object slot to their defaults.
     *
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CMap) == 0xAF0);

} // namespace dc::game
