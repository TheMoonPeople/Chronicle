#pragma once

#include "common.h"

#include "itemdata.hpp"

/**
 * Restriction a dungeon floor places on the party, as CUserStatus::res_limit_zone_current holds it.
 */
// clang-format off
enum ResLimitZone {
    RES_LIMIT_ZONE_NONE             = -1, /**< No restriction. */
    RES_LIMIT_ZONE_TOAN             = 0,  /**< Only Toan may fight. */
    RES_LIMIT_ZONE_XIAO             = 1,  /**< Only Xiao may fight. */
    RES_LIMIT_ZONE_GORO             = 2,  /**< Only Goro may fight. */
    RES_LIMIT_ZONE_RUBY             = 3,  /**< Only Ruby may fight. */
    RES_LIMIT_ZONE_UNGAGA           = 4,  /**< Only Ungaga may fight. */
    RES_LIMIT_ZONE_OSMOND           = 5,  /**< Only Osmond may fight. */
    RES_LIMIT_ZONE_NO_WEAPON_CHANGE = 10, /**< Weapons cannot be changed. */
    RES_LIMIT_ZONE_THIRST           = 11, /**< Thirst drains five times as fast. */
};

// clang-format on

/**
 * Status ailment bits of CUserStatus::ailments.
 */
// clang-format off
enum StatusAilment {
    AILMENT_KNOCKED_OUT = 0x02, /**< Knocked out; treated like zero HP. */
    AILMENT_FREEZE      = 0x04, /**< Frozen in place. */
    AILMENT_STAMINA     = 0x08, /**< Stamina. */
    AILMENT_POISON      = 0x10, /**< Poisoned. */
    AILMENT_CURSE       = 0x20, /**< Cursed. */
    AILMENT_GOO         = 0x40, /**< Covered in goo. */
};

// clang-format on

/**
 * Stores one attachment slot in the dungeon inventory.
 */
struct DNG_CONSUMABLE {
    s16  id; /**< Attachment in the slot, or an id below 81 if the slot is empty. */
    char unk_02[30];
};

STATIC_ASSERT(sizeof(DNG_CONSUMABLE) == 0x20);

/**
 * Stores the dungeon inventory and its quick-use slots.
 */
struct ITEM_PACK {
    s8  num;                /**< Slots the pack holds. */
    s8  item_count;         /**< Dungeon items carried, counting every copy in a quick-use slot. */
    s16 quick_item_slot[3]; /**< Item in each quick-use slot, or -1. */
    s16 quick_item_qty[3];  /**< How many of that item each quick-use slot holds. */
    s16 item[103];          /**< Item in each slot, or -1. */
    s16 item_vol[103];      /**< How much is left in each slot's copy of its item. */
};

STATIC_ASSERT(sizeof(ITEM_PACK) == 0x1AA);

/**
 * Records what has become of one atla of one floor.
 */
struct ATRA_SAVE {
    s32 id;       /**< Atla the record is for, or -1 once no slot asks for it. */
    s32 floor;    /**< Floor the atla lies on counted from one, or -1 or -2 for any upper or lower floor. */
    s32 refcount; /**< How many of the floor's atla slots still ask for this atla. */
};

STATIC_ASSERT(sizeof(ATRA_SAVE) == 0xC);

/**
 * Stores the party's characters, inventory, and dungeon progress.
 */
class CUserStatus {
public:
    /* CDngStatusData extends this layout with its dungeon tail. The fields are
     * public because retail reaches into them from outside both classes. */
    s8        cur_georama; /**< Current town and dungeon index. */
    char      unk_01[1];
    s8        cur_floor;  /**< Floor the party stands on. */
    s8        prev_floor; /**< Floor occupied before the current floor. */
    s8        cur_chara;  /**< Index of the currently controlled party member. */
    s8        party_size; /**< Party members who have joined. */
    s16       max_hp[6];  /**< Maximum life of each party member. */
    s16       hp[6];      /**< Current displayed life of each party member. */
    char      unk_01E[0x25A];
    s32       atra_grid[6][40][8];   /**< Atla each floor slot asks for, or -1 empty, -2 any atla, -3 collected. */
    ATRA_SAVE atra_registry[6][100]; /**< Atla each dungeon's floor slots still ask for. */
    s16       kills[6][100];         /**< Monsters defeated on each dungeon floor. */
    char      unk_4148[200];
    char      res_limit_zone_id[6][25]; /**< Restriction-zone identifiers by dungeon and floor. */
    char      unk_42A6[25];
    s8        floor_reached[7]; /**< Deepest floor reached in each dungeon, or -1 if never entered. */
    char      unk_42C6[2];
    s32       ailments[6];             /**< Status ailments each party member suffers. @see StatusAilment. */
    s16       ailment_frames[6];       /**< Frames left of each party member's status ailment. */
    float     water_max[6];            /**< Most water each character can hold, ten to a drop. */
    float     water_now[6];            /**< Water each character holds now. */
    s32       overflow_flag;           /**< Whether the player carries more items than the pack holds. */
    s32       special_flag_238;        /**< Set when item 238 is picked up. */
    s32       skill_owned[6];          /**< Whether each character has received their event skill. */
    s32       minimap_status;          /**< Current minimap visibility mode. */
    s8        equipped_weapon_slot[6]; /**< Specifies each character's equipped weapon slot. */

    union {
        u16 money;        /**< Gilda the party carries. */
        s16 money_signed; /**< Signed view of the Gilda the party carries. */
    };

    s32 defense[6]; /**< Defense of each party member, raised by their favourite food up to 99. */

    union {
        ITEM_PACK item_pack; /**< Dungeon items the player carries. */

        /**
         * The pack read as halfwords from its start, indexed by 1-based quick-use slot: [slot] is
         * that slot's item and [slot + 3] its count.
         */
        s16 active_item[3];
    };

    char           unk_450A[2];
    WEAPON_HAVE    chara_weapons[6][11]; /**< Specifies the weapons owned by each character. */
    DNG_CONSUMABLE consumable_items[43]; /**< Specifies the stored consumable items. */
    s16            next_hp[6];           /**< Life each party member's gauge is moving towards. */
    s16            life_step[6];         /**< Life each party member's gauge moves by each step, or 0 when still. */
    s32            unk_8A74[6];
    s32            unk_8A8C[6];
    s8             event_flags[50]; /**< Flags the dungeon's event scripts set and read. */
    s16            drink_next[6];   /**< Water each party member's gauge is moving towards. */
    s16            drink_step[6];   /**< Water each party member's gauge moves by each step, or 0 when still. */
    char           unk_8AEE[2];
    float          thirst_damage[6];       /**< Progress towards the next point of life lost to an empty water gauge. */
    s32            water_drain_disable;    /**< 1 while the active character's water does not drain. */
    s32            step_disable;           /**< 1 while the gauges, and the player's status ailments, stand still. */
    s32            res_limit_zone_current; /**< Restriction zone the party is fenced into. @see ResLimitZone. */
    s32            active_item_vol[3];     /**< Uses left of the item in each quick-use slot. */

    /**
     * @mangled ChkEventFlag__11CUserStatusFi
     * @address 0x1BDAC0
     * @size 0x5C
     */
    int ChkEventFlag(int flag_no);

    /**
     * @mangled ClearEventFlag__11CUserStatusFv
     * @address 0x1BDB20
     * @size 0x34
     */
    void ClearEventFlag();

    /**
     * @mangled AddDrink__11CUserStatusFisf
     * @address 0x1BE510
     * @size 0x1FC
     */
    void AddDrink(int chara_no, s16 amount, float ratio);

    /**
     * @mangled AddNowLife__11CUserStatusFisf
     * @address 0x1BE710
     * @size 0x178
     */
    void AddNowLife(int chara_no, s16 amount, float ratio);

    /**
     * @mangled CheckLife__11CUserStatusFv
     * @address 0x1BE890
     * @size 0x68
     */
    int CheckLife();

    /**
     * @mangled SetNextLife__11CUserStatusFisf
     * @address 0x1BE900
     * @size 0x144
     */
    void SetNextLife(int chara_no, s16 value, float ratio);

    /**
     * Drains the active character's water, then moves every water and HP gauge one step.
     *
     * @mangled Step__11CUserStatusFi
     * @address 0x1BEA50
     * @size 0x388
     */
    void Step(int mode);

    /**
     * @mangled Init__11CUserStatusFv
     * @address 0x1BEDE0
     * @size 0x108
     */
    void Init();
};

STATIC_ASSERT(sizeof(CUserStatus) == 0x8B20);
