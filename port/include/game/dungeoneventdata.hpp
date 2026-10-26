#pragma once

#include "common.h"

#include <libvu0.h>

class CFrame;

/**
 * Names one event that a dungeon floor lays out, as the floor's event file
 * declares it.
 */
class CDungeonEvent {
public:
    char    name[16];        /**< Name of the frame in the map part's collision that places the event. */
    CFrame *placement_frame; /**< Root frame positioned and rotated with the map part. */
    CFrame *trigger_frame;   /**< Child frame whose transform locates and faces the event. */
    float   radius;          /**< Distance at which the event starts. */
    s32     parts_id;        /**< Map-part identifier that owns this event definition. */
    s32     script_no;       /**< Script that the event runs, or -1 where it runs none. */
    s32     fade;            /**< 1 where the event fades the picture out before it runs. */
    s32     hold;            /**< Value that the runtime event's hold flag starts from. */
    s32     chara_no;        /**< Character that has to start the event, or -1 for any. */
    s32     key_id;          /**< Identifier that the character's collision hit has to carry, such as a door key, or -1 for any. */
    s32     switch_on;       /**< Whether the event starts switched on. */
    s32     ext_mem;         /**< 1 where the script runs out of the memory freed from the monster models, 0 for the system memory. */
    s32     enabled;         /**< Whether this definition slot is in use. */
};

STATIC_ASSERT(sizeof(CDungeonEvent) == 0x40);

/**
 * Records what one event of the floor is doing.
 */
class CDungeonEventData {
public:
    CDungeonEvent *event; /**< Event that the floor laid out, or zero for a free slot. */
    u8             unk_04[0xC];
    sceVu0FVECTOR  pos;        /**< World position that the event plays at. */
    sceVu0FVECTOR  dir;        /**< Way that the event faces. */
    s32            hold;       /**< Holds the event back from running until it is cleared. */
    s32            switch_on;  /**< Whether a script has the event switched on. */
    s32            enabled;    /**< Whether the slot holds an event that can run. */
    s32            chara_done; /**< Character that has already run the event, or -1 for none. */

    /**
     * Initializes this runtime slot from a dungeon event definition.
     *
     * @mangled Set__17CDungeonEventDataFP13CDungeonEvent
     * @address 0x1CC820
     * @size 0x5C
     */
    void Set(CDungeonEvent *source);

    /**
     * Reports whether this event slot is enabled and ready to run.
     *
     * @mangled CheckSwitch__17CDungeonEventDataFv
     * @address 0x1CC880
     * @size 0x48
     */
    int CheckSwitch();

    /**
     * Disables this event slot.
     *
     * @mangled Stop__17CDungeonEventDataFv
     * @address 0x1CC8D0
     * @size 0xC
     */
    void Stop();

    /**
     * Enables this event slot.
     *
     * @mangled Start__17CDungeonEventDataFv
     * @address 0x1CC8E0
     * @size 0x10
     */
    void Start();
};

STATIC_ASSERT(sizeof(CDungeonEventData) == 0x40);
