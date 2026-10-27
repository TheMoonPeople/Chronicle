#pragma once

#include "common.h"

namespace dc::game {

/**
 * Steps of a dropped item, as CRandomItem::state holds them.
 */
enum class RandomItemState : s32 {
    RISING = 0,  /**< Rising out of the defeated monster. */
    WAITING = 1, /**< Waiting to be taken. */
    TAKEN = 2,   /**< Shrinking away after pickup. */
};

class CTexture;

/**
 * Keeps the gold and items that defeated monsters leave lying on the floor.
 */
class CRandomItem {
public:
    CTexture       *gold_texture; /**< Texture that a dropped pile of gold draws with. */
    u8              unk_004[0xC];
    float           position[32][4];    /**< World position of each dropped item. */
    float           distance[32];       /**< Distance from each item to the player. */
    s32             id[32];             /**< Slot identifier, or -1 where the slot is free. */
    s32             amount[32];         /**< Gold value, or -1 when the slot contains an item. */
    RandomItemState state[32];          /**< Appearance and pickup animation state. */
    float           phase[32];          /**< Phase of each item's appearance or pickup animation. */
    float           bob_phase;          /**< Shared phase used to make active items hover. */
    s32             pickup_event[32];   /**< Event each dropped item raises when the party walks onto it, or -1. */
    s32             pickup_blocked[32]; /**< Nonzero while inventory capacity prevents pickup. */
    s32             item_no[32];        /**< Item number, or -1 when the slot contains gold. */
    u8              unk_614[0xC];

    /**
     * Draws the items lying on the floor.
     */
    void Draw();

    /**
     * Draws the corner-map marks of the items lying on the floor.
     */
    void MapSymbolDraw();

    /**
     * Takes the first pending pickup event, returning its item number or -1.
     */
    int checkEvent();

    /**
     * Returns 1 once for an item whose pickup was newly refused, else 0.
     */
    int checkErr();

    /**
     * Picks up whatever the player stands on, returning the gold gained.
     */
    int CheckPosition();

    /**
     * Puts one item on the floor at a position.
     */
    void Set(float *drop_position, int slot_id, int gold, int item);

    /**
     * Finds a free slot among the thirty-two lying items, or -1.
     */
    int CheckID();

    /**
     * Returns the item number if it is already lying on the floor, else 0.
     */
    int CheckItemNo(int item);

    /**
     * Advances the lying items' rise, hover and pickup animations by a frame.
     */
    void Step();
};

STATIC_ASSERT(sizeof(CRandomItem) == 0x620);

} // namespace dc::game
