#pragma once

#include "common.h"

/**
 * Steps of a dropped item, as CRandomItem::state holds them.
 */
// clang-format off
enum RandomItemState {
    RANDOM_ITEM_RISING  = 0, /**< Rising. */
    RANDOM_ITEM_WAITING = 1, /**< Waiting to be taken. */
    RANDOM_ITEM_TAKEN   = 2, /**< Shrinking away. */
};

// clang-format on

class CTexture;

/**
 * Keeps the gold and items that defeated monsters leave lying on the floor.
 */
class CRandomItem {
public:
    CTexture *gold_texture; /**< Texture that a dropped pile of gold draws with. */
    u8        unk_004[0xC];
    float     position[32][4];    /**< World position of each dropped item. */
    float     distance[32];       /**< Distance from each item to the player. */
    s32       id[32];             /**< Slot identifier, or -1 where the slot is free. */
    s32       amount[32];         /**< Gold value, or -1 when the slot contains an item. */
    s32       state[32];          /**< Appearance and pickup animation state. */
    float     phase[32];          /**< Phase of each item's appearance or pickup animation. */
    float     bob_phase;          /**< Shared phase used to make active items hover. */
    s32       pickup_event[32];   /**< Event each dropped item raises when the party walks onto it, or -1. */
    s32       pickup_blocked[32]; /**< Nonzero while inventory capacity prevents pickup. */
    s32       item_no[32];        /**< Item number, or -1 when the slot contains gold. */
    u8        unk_614[0xC];

    /**
     * Draws the items lying on the floor.
     *
     * @mangled Draw__11CRandomItemFv
     * @address 0x1D6BE0
     * @size 0x1BC
     */
    void Draw();

    /**
     * Draws the corner-map marks of the items lying on the floor.
     *
     * @mangled MapSymbolDraw__11CRandomItemFv
     * @address 0x1D6DA0
     * @size 0x148
     */
    void MapSymbolDraw();

    /**
     * Raises the pickup event for an item the party has walked onto.
     *
     * @mangled checkEvent__11CRandomItemFv
     * @address 0x1D6EF0
     * @size 0x50
     */
    int checkEvent();

    /**
     * Drops items that ended up somewhere they may not lie.
     *
     * @mangled checkErr__11CRandomItemFv
     * @address 0x1D6F40
     * @size 0x60
     */
    int checkErr();

    /**
     * Reports which lying item the party is standing on.
     *
     * @mangled CheckPosition__11CRandomItemFv
     * @address 0x1D6FA0
     * @size 0x250
     */
    int CheckPosition();

    /**
     * Puts one item on the floor at a position.
     *
     * @mangled Set__11CRandomItemFPfiii
     * @address 0x1D71F0
     * @size 0xD8
     */
    void Set(float *drop_position, int slot_id, int gold, int item);

    /**
     * Finds a free slot among the thirty-two lying items.
     *
     * @mangled CheckID__11CRandomItemFv
     * @address 0x1D72D0
     * @size 0x4C
     */
    int CheckID();

    /**
     * Reports whether one item number is already lying on the floor.
     *
     * @mangled CheckItemNo__11CRandomItemFi
     * @address 0x1D7320
     * @size 0x58
     */
    int CheckItemNo(int item);

    /**
     * Advances the lying items' bob and sparkle by a frame.
     *
     * @mangled Step__11CRandomItemFv
     * @address 0x1D7380
     * @size 0x158
     */
    void Step();
};

STATIC_ASSERT(sizeof(CRandomItem) == 0x620);
