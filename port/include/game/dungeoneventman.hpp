#pragma once

#include "common.h"

#include "dungeoneventdata.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CDungeonMap;

/**
 * Owns the definition slots and runtime records for dungeon events.
 */
class CDungeonEventMan {
public:
    CDungeonEvent     slot[64];  /**< Event definitions loaded for the floor's map parts. */
    CDungeonEventData event[96]; /**< What each placed event is doing. */

    /**
     * Returns a used event slot whose parts identifier matches the requested value.
     *
     * @mangled SearchPartsID__16CDungeonEventManFii
     * @address 0x1CC8F0
     * @size 0x44
     */
    CDungeonEvent *SearchPartsID(int index, int parts_id);

    /**
     * Returns the first unused event slot.
     *
     * @mangled SearchSlot__16CDungeonEventManFv
     * @address 0x1CC940
     * @size 0x44
     */
    CDungeonEvent *SearchSlot();

    /**
     * Enables or disables every runtime event for a requested script number.
     *
     * @mangled SearchDataSwitch__16CDungeonEventManFii
     * @address 0x1CC990
     * @size 0xD0
     */
    void SearchDataSwitch(int script_no, int enable);

    /**
     * Releases the hold flag for runtime events with a requested script number.
     *
     * @mangled SearchItemEventHold__16CDungeonEventManFi
     * @address 0x1CCA60
     * @size 0x64
     */
    void SearchItemEventHold(int script_no);

    /**
     * Counts unused runtime event records.
     *
     * @mangled GetDataNum__16CDungeonEventManFv
     * @address 0x1CCAD0
     * @size 0x58
     */
    int GetDataNum();

    /**
     * Returns the first unused runtime event record.
     *
     * @mangled SearchDataSlot__16CDungeonEventManFv
     * @address 0x1CCB30
     * @size 0x64
     */
    CDungeonEventData *SearchDataSlot();

    /**
     * Returns this runtime event when one of its configured collision owners reaches it.
     *
     * @mangled CheckCollisionDataHit__16CDungeonEventManFi
     * @address 0x1CCBA0
     * @size 0x130
     */
    CDungeonEventData *CheckCollisionDataHit(int index);

    /**
     * Finds a runnable event at a position or through its configured collision owners.
     *
     * @mangled SearchDataSlotPos__16CDungeonEventManFPf
     * @address 0x1CCCD0
     * @size 0x108
     */
    CDungeonEventData *SearchDataSlotPos(float *position);

    /**
     * Finds a runnable event near a position after flattening small height differences.
     *
     * @mangled SearchDataSlotPos2__16CDungeonEventManFPf
     * @address 0x1CCDE0
     * @size 0x154
     */
    CDungeonEventData *SearchDataSlotPos2(float *position);

    /**
     * Builds the runtime event records for a dungeon map.
     *
     * @mangled SetupEvent__16CDungeonEventManFP11CDungeonMapi
     * @address 0x1CCF40
     * @size 0x474
     */
    void SetupEvent(CDungeonMap *map, int mode);
};

STATIC_ASSERT(sizeof(CDungeonEventMan) == 0x2800);
