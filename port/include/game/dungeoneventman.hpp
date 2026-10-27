#pragma once

#include "common.h"

#include "dungeoneventdata.hpp"

namespace dc::game {

class CDungeonMap;

/**
 * Owns the event definitions loaded for a floor's map parts and the
 * runtime records of the events placed from them.
 */
class CDungeonEventMan {
public:
    CDungeonEvent     slot[64];  /**< Event definitions loaded for the floor's map parts. */
    CDungeonEventData event[96]; /**< What each placed event is doing. */

    /**
     * Returns slot index when it is in use and belongs to parts_id, otherwise nullptr.
     */
    CDungeonEvent *SearchPartsID(int index, int parts_id);

    /**
     * Returns the first unused event definition slot, or nullptr when all are in use.
     */
    CDungeonEvent *SearchSlot();

    /**
     * Starts (enable != 0) or stops every active runtime event running script_no.
     */
    void SearchDataSwitch(int script_no, int enable);

    /**
     * Clears the hold flag of every active runtime event running script_no.
     */
    void SearchItemEventHold(int script_no);

    /**
     * Counts the runtime event records that are not in use.
     */
    int GetDataNum();

    /**
     * Returns the first runtime event record that is not in use, or nullptr.
     */
    CDungeonEventData *SearchDataSlot();

    /**
     * Returns runtime event index when a hit from its configured collision owner lands within its radius.
     */
    CDungeonEventData *CheckCollisionDataHit(int index);

    /**
     * Finds a switched-on event within reach of position or hit by its configured collision owner.
     */
    CDungeonEventData *SearchDataSlotPos(float *position);

    /**
     * Finds a switched-on event near position on the ground plane, ignoring small height differences.
     */
    CDungeonEventData *SearchDataSlotPos2(float *position);

    /**
     * Places the runtime event records for every map part (mode 1: per grid cell, otherwise per part).
     */
    void SetupEvent(CDungeonMap *map, int mode);
};

STATIC_ASSERT(sizeof(CDungeonEventMan) == 0x2800);

} // namespace dc::game
