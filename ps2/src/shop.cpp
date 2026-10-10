#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 761

#include "shop.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battlemenu.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "editloop.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop_menu.hpp"
#include "snd.hpp"
#include "stockitem.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "weaponlevelup.hpp"

/** Number of goods on each charge shop's board. */
s16 ChargeShopMax[3] = {60, 30, 30};

/** Arena the shopkeeper's model and the item shop's board tables are read into. */
CDataAlloc2<1> ShopCashBuffer(-1);

/**
 * Arena the shopkeeper's model data is built in, carved from the menu buffer past ShopCashBuffer.
 */
CDataAlloc2<1> ShopCashBuffer2(-1);

/** Shared UI state for the charge shop and item shop screens. */
ShopMenuWork ShopMenu;

/** The icon being carried between the shop's board and the player's. */
ShopIconMove ShopDataMove;

/** Player status the shop currently open is reading and writing. */
CUserStatus *ShopUserStatusPt;

/** Texture the shop board frame, tags and tickets are drawn from. */
CTexture *ShopBoard;

/** Stock inventory the shop currently open is reading and writing. */
CStockItem *ShopStockPt;

/** Per-slot state of the charge shop's personal board. */
s32 *ShopBoardInfo;

/** Per-slot state of the personal item board. @see ShopSlotState */
s32 *ItemBoardInfo;

/** Per-slot state of the personal weapon board, indexed by character then slot. */
s32 (*WeaponBoardInfo)[10];

/** Per-slot state of the personal attachment board. */
s32 *AttachBoardInfo;

/** The charge shop's goods list, one entry per personal board slot. */
SHOP_ITEMLIST *ShopListPt;

/** Working copy of the item shop's board, allocated out of the shop's arena. */
SHOP_ITEMLIST *ShopWorkBuf;

/** Screen position of the shop's help window. */
float ShopHelpWinPos[2];

/** Height of the shop's help window. */
float ShopHelpWinH;

/** Width of the shop's help window. */
float ShopHelpWinW;

IHAVEITEM   *ShopHaveItemPt;
WEAPON_HAVE *ShopHaveWepPt;
ATTACH_LIST *ShopHaveAttachPt;

/** Which shop is open. @see ShopKind */
s16 ChargeOrShopFlag;

static int ChargeSelectKey();

/**
 * State of the fishing prize exchange screen.
 */
struct FishMenuWork {
    s16        tex_block;  /**< Texture block the exchange's textures are entered into. */
    s16        tex_block2; /**< Second texture block lent to the exchange. */
    s16        ready;      /**< Nonzero once the exchange's contents may be drawn. */
    s16        warning;    /**< Which refusal the exchange shows, as an offset from its first refusal message. */
    s32        point;      /**< Fishing points the player has left to spend. */
    s32        cursor_y;   /**< Screen Y of the cursor, eased toward its row. */
    s16        mode;       /**< How the exchange was opened. */
    s16        confirm;    /**< Choice the confirmation window's cursor is on: 0 yes, 1 no. */
    s16        cursor;     /**< Prize the cursor is on. */
    s16        fade_mode;  /**< What the exchange is doing. @see FishExchangeMode */
    s16        top;        /**< Prize the list shows first. */
    u8         unk_1A[6];
    s32        fade_count; /**< Frames the current fade has run for. */
    u_long128 *buffer;     /**< Buffer the exchange's files are read into. */
};

STATIC_ASSERT(sizeof(FishMenuWork) == 0x28);

/** State of the fishing prize exchange screen. */
FishMenuWork FishMenu;

/**
 * State of the fishing record screen.
 */
struct FishRecordMenuWork {
    s32 cursor;     /**< Rank the cursor is on. */
    s32 top;        /**< Rank the board shows first. */
    s32 mode;       /**< The mode the record view was opened in. */
    s32 fade_mode;  /**< What the record view is doing. @see FishRecordMode */
    s32 fade_count; /**< Frames the current fade has run for. */
    s32 cursor_y;   /**< Screen Y of the cursor, eased toward its row. */
    s32 ready;      /**< Nonzero once the record view's contents may be drawn. */
    s32 tex_block;  /**< Texture block the record view's textures are entered into. */
    s32 tex_block2; /**< Second texture block the record view's textures are entered into. */
};

STATIC_ASSERT(sizeof(FishRecordMenuWork) == 0x24);

/** State of the fishing record screen. */
FishRecordMenuWork FishRecordMenu;

/** Texture the fishing exchange's fish icons are drawn from. */
CTexture *FishMenuTex;

/**
 * Gives the dungeon item pack a player status carries.
 */
static inline ITEM_PACK *ShopUserItemPack(CUserStatus *user_status) {
    return &user_status->item_pack;
}

/** An item's buy and sell price. */
struct ITEM_MONEY_ENTRY {
    s16 buy_price;  /**< Money the shop charges for the item. */
    s16 sell_price; /**< Money the shop pays for the item. */
};

/** The buy and sell price of every item from ITEM_ATTACH_START on, indexed by item number less ITEM_ATTACH_START. */
ITEM_MONEY_ENTRY PriceList[296] = {
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {300,  150 },
    {260,  130 },
    {260,  130 },
    {300,  150 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {3000, 600 },
    {5000, 1000},
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {300,  150 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {400,  200 },
    {440,  220 },
    {380,  190 },
    {400,  200 },
    {1000, 500 },
    {1000, 500 },
    {1000, 500 },
    {1000, 500 },
    {1000, 500 },
    {1000, 500 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {10,   5   },
    {30,   15  },
    {60,   30  },
    {20,   10  },
    {130,  65  },
    {300,  150 },
    {80,   40  },
    {120,  60  },
    {100,  50  },
    {300,  150 },
    {60,   30  },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {80,   40  },
    {4,    2   },
    {100,  50  },
    {100,  50  },
    {100,  50  },
    {100,  50  },
    {100,  50  },
    {100,  50  },
    {80,   40  },
    {90,   45  },
    {120,  60  },
    {80,   40  },
    {2,    1   },
    {43,   22  },
    {2,    1   },
    {50,   25  },
    {20,   10  },
    {100,  50  },
    {20,   10  },
    {1000, 500 },
    {100,  50  },
    {800,  400 },
    {800,  400 },
    {500,  250 },
    {200,  100 },
    {2,    1   },
    {500,  250 },
    {300,  150 },
    {450,  225 },
    {400,  200 },
    {420,  210 },
    {380,  190 },
    {2,    1   },
    {1000, 1000},
    {300,  150 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {400,  200 },
    {400,  200 },
    {400,  200 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {300,  150 },
    {300,  150 },
    {180,  90  },
    {2,    1   },
    {500,  250 },
    {800,  400 },
    {800,  400 },
    {1000, 500 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {50,   25  },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {20,   10  },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {300,  75  },
    {500,  125 },
    {2500, 625 },
    {700,  175 },
    {800,  200 },
    {1000, 250 },
    {400,  100 },
    {2000, 500 },
    {3000, 750 },
    {2000, 500 },
    {2400, 600 },
    {500,  125 },
    {3000, 750 },
    {3000, 750 },
    {2,    1   },
    {3000, 750 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    0   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {800,  200 },
    {920,  230 },
    {2,    1   },
    {900,  225 },
    {990,  248 },
    {2500, 625 },
    {1500, 375 },
    {2,    1   },
    {1000, 250 },
    {2000, 500 },
    {940,  235 },
    {1500, 375 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {360,  90  },
    {600,  150 },
    {1000, 250 },
    {400,  100 },
    {900,  225 },
    {1000, 250 },
    {1200, 300 },
    {2,    1   },
    {2,    1   },
    {500,  125 },
    {600,  150 },
    {2,    1   },
    {900,  225 },
    {2,    1   },
    {2,    1   },
    {500,  125 },
    {700,  175 },
    {800,  200 },
    {850,  212 },
    {1000, 250 },
    {400,  100 },
    {900,  225 },
    {1500, 375 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {850,  212 },
    {200,  50  },
    {2,    1   },
    {1000, 250 },
    {2,    1   },
    {2,    1   },
    {400,  100 },
    {700,  175 },
    {600,  150 },
    {720,  180 },
    {800,  200 },
    {1200, 300 },
    {1400, 350 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {400,  100 },
    {300,  75  },
    {2,    1   },
    {720,  180 },
    {2,    1   },
    {2,    1   },
    {800,  200 },
    {1000, 250 },
    {1200, 300 },
    {1500, 375 },
    {1700, 425 },
    {2000, 500 },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {2,    1   },
    {300,  75  },
    {1800, 450 },
    {800,  200 },
    {1000, 250 },
    {2,    1   },
    {2,    1   },
    {3000, 750 },
    {3200, 800 },
    {3300, 825 },
    {3400, 850 },
    {4000, 1000},
    {5500, 1375},
    {6500, 1625},
    {2,    1   },
    {2,    1   },
    {1500, 375 },
    {3400, 850 },
    {1000, 250 },
};

/**
 * Gives the goods list one item shop sells from.
 *
 * One shop's twenty-item goods list, eighteen shops long, each ended by -1.
 */
static s16 ItemShopList2[18][20] = {
    {ITEM_BREAD, ITEM_REGULAR_WATER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_ESCAPE_POWDER, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ANTIDOTE_DRINK, ITEM_SOAP, ITEM_BOMB, ITEM_DRAN_S_FEATHER, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_ATTACH_DINOSLAYER, ITEM_GOLD_BULLION},
    {ITEM_BREAD, ITEM_CHEESE, ITEM_TASTY_WATER, ITEM_REVIVAL_POWDER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_ESCAPE_POWDER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ANTIDOTE_DRINK, ITEM_SOAP, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, ITEM_WEAPON_WISE_OWL_SWORD, ITEM_MIMI, ITEM_PRICKLY, -1},
    {ITEM_FLAPPING_FISH, ITEM_FLAPPING_FISH, ITEM_FLAPPING_FISH, ITEM_EVY, ITEM_EVY, ITEM_PETITE_FISH, ITEM_PETITE_FISH, ITEM_ICE_BLOCK, ITEM_ICE_BLOCK, ITEM_ICE_BLOCK, ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_STAND_IN_POWDER, -1},
    {ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_STAMINA_DRINK, ITEM_ANTIDOTE_DRINK, ITEM_HOLY_WATER, -1},
    {ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_BREAD, ITEM_STAMINA_DRINK, ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, -1},
    {ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_STAMINA_DRINK, ITEM_CHEESE, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, -1},
    {ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_STAMINA_DRINK, ITEM_PREMIUM_CHICKEN, ITEM_BOMB, ITEM_ATTACH_SEA_KILLER, ITEM_ATTACH_MAGE_SLAYER, ITEM_ATTACH_UNDEAD_BUSTER, -1},
    {ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS, ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_REPAIR_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, -1},
    {ITEM_WEAPON_KITCHEN_KNIFE, ITEM_WEAPON_GLADIUS, ITEM_WEAPON_BONE_SLINGSHOT, ITEM_WEAPON_BIG_BUCKS_HAMMER, ITEM_WEAPON_PLATINUM_RING, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ESCAPE_POWDER, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, -1},
    {ITEM_PREMIUM_WATER, ITEM_CHEESE, ITEM_PREMIUM_CHICKEN, ITEM_MIGHTY_HEALING, ITEM_BOMB, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ESCAPE_POWDER, ITEM_ATTACH_WIND, ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_SKY_HUNTER, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, ITEM_POTATO_CAKE, -1},
    {ITEM_REGULAR_WATER, ITEM_TASTY_WATER, ITEM_PREMIUM_WATER, ITEM_PREMIUM_CHICKEN, ITEM_BOMB, ITEM_ATTACH_BEAST_BUSTER, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ESCAPE_POWDER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_DRAN_S_FEATHER, ITEM_WEAPON_JACKAL, ITEM_WEAPON_SNAIL, ITEM_GOLD_BULLION, -1},
    {ITEM_PREMIUM_WATER, ITEM_PREMIUM_CHICKEN, ITEM_MIGHTY_HEALING, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ESCAPE_POWDER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, -1},
    {ITEM_FIRE_GEM, ITEM_ICE_GEM, ITEM_THUNDER_GEM, ITEM_WIND_GEM, ITEM_HOLY_GEM, ITEM_ATTACH_FIRE, ITEM_ATTACH_ICE, ITEM_ATTACH_THUNDER, ITEM_ATTACH_WIND, ITEM_ATTACH_HOLY, ITEM_ATTACH_SEA_KILLER, ITEM_ATTACH_MAGE_SLAYER, ITEM_GOLD_BULLION, -1},
    {ITEM_BREAD, ITEM_REGULAR_WATER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_REVIVAL_POWDER, ITEM_STAND_IN_POWDER, ITEM_ESCAPE_POWDER, ITEM_ANTIDOTE_DRINK, ITEM_GOLD_BULLION, -1},
    {ITEM_BREAD, ITEM_CHEESE, ITEM_TASTY_WATER, ITEM_REVIVAL_POWDER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_STAND_IN_POWDER, ITEM_ESCAPE_POWDER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ANTIDOTE_DRINK, ITEM_SOAP, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, -1},
    {ITEM_PREMIUM_WATER, ITEM_CHEESE, ITEM_HOLY_WATER, ITEM_SOAP, ITEM_MIGHTY_HEALING, ITEM_BOMB, ITEM_STAND_IN_POWDER, ITEM_REVIVAL_POWDER, ITEM_ESCAPE_POWDER, ITEM_REPAIR_POWDER, ITEM_AUTO_REPAIR_POWDER, ITEM_DRAN_S_FEATHER, ITEM_GOLD_BULLION, -1},
    {ITEM_ATTACH_GARNET, ITEM_ATTACH_AMETHYST, ITEM_ATTACH_AQUAMARINE, ITEM_ATTACH_DIAMOND, ITEM_ATTACH_EMERALD, ITEM_ATTACH_PEARL, ITEM_ATTACH_RUBY, ITEM_ATTACH_PERIDOT, ITEM_ATTACH_SAPPHIRE, ITEM_ATTACH_OPAL, ITEM_ATTACH_TOPAZ, ITEM_ATTACH_TURQUOISE, -1},
    {ITEM_ATTACH_ATTACK, ITEM_ATTACH_ENDURANCE, ITEM_ATTACH_SPEED, ITEM_ATTACH_MAGICAL_POWER, ITEM_ATTACH_DINOSLAYER, ITEM_ATTACH_UNDEAD_BUSTER, ITEM_ATTACH_SEA_KILLER, ITEM_ATTACH_STONE_BREAKER, ITEM_ATTACH_PLANT_BUSTER, ITEM_ATTACH_BEAST_BUSTER, ITEM_ATTACH_SKY_HUNTER, ITEM_ATTACH_MAGE_SLAYER, -1},
};

static void DrawSellTicket_2(int x, int y, int clip_top, int clip_bottom, int alpha);

static void SetShopTalkMsgPos();

static void DrawCheckButton(int x, int y, int alpha);

static void ItemShopGoodInitialize(int shop_no);

static void ShopCancelGoodReturn2();

s16 *GetItemShopList(int shop_no) {
    return ItemShopList2[shop_no];
}

void InitShopItemListData(SHOP_ITEMLIST *item_list) {
    if (item_list != NULL) {
        item_list->item_no = 0;
        memset(&item_list->data, 0, sizeof(item_list->data));
    }
}

void ShopIconMove::IconMoveTarSet(int slot_no, int icon_no, int item_no, MENU_ITEMDATA *item_data, float start_x, float start_y, int to_stock) {
    this->to_stock = to_stock;
    this->item_no = item_no;
    this->slot_no = slot_no;
    this->icon_no = icon_no;
    memcpy(&this->data, item_data, sizeof(this->data));
    this->pos_x = start_x;
    this->pos_y = start_y;
}

int ShopIconMove::IconAutoMove(int item_shop, int force_arrive) {
    if (item_no < ITEM_ATTACH_START) {
        return -1;
    }

    int col_x = icon_no % 5 * 0x28;
    int row_y = icon_no / 5 * 0x28;
    int target_x;
    int target_y;

    if (item_shop) {
        target_x = col_x + 0x16A;
        target_y = row_y + 0x86;
    } else {
        target_x = col_x + 0x16A;
        target_y = row_y + 0x90;
    }

    if (to_stock) {
        target_x = col_x + 0x3E;
        target_y = row_y + 0x90;
    }

    pos_x += (float) (target_x - (int) pos_x) / 4.0f;
    pos_y += (float) (target_y - (int) pos_y) / 4.0f;

    int arrived = false;

    if (force_arrive || (float) abs((int) (pos_x - (float) target_x)) < 4.0f) {
        pos_x = target_x;
        arrived++;
    }

    if (force_arrive || (float) abs((int) (pos_y - (float) target_y)) < 4.0f) {
        pos_y = target_y;
        arrived++;
    }

    if (arrived >= 2) {
        s32 *board_info;

        switch (WhatIsKindofItem(item_no)) {
            case BOARD_PAGE_ITEM:
                if (to_stock) {
                    s16 item = item_no;
                    s16 volume = data.volume;
                    ShopStockPt->SetItemToPos(slot_no, &item, &volume);
                } else {
                    ITEM_PACK *pack = ShopUserItemPack(ShopUserStatusPt);
                    pack->item[slot_no] = item_no;
                    pack->item_vol[slot_no] = data.volume;
                    board_info = &ItemBoardInfo[slot_no];
                }

                break;
            case BOARD_PAGE_ATTACH:
                if (to_stock) {
                    ShopStockPt->SetAttachToPos(slot_no, &data.attach);
                } else {
                    DNG_CONSUMABLE *attach = ShopUserStatusPt->consumable_items;
                    memcpy(&attach[slot_no], &data.attach, sizeof(DNG_CONSUMABLE));
                    attach[slot_no].id = item_no;
                    board_info = &AttachBoardInfo[slot_no];
                }

                break;
            case BOARD_PAGE_WEAPON:
                if (to_stock) {
                    ShopStockPt->SetWepToPos(slot_no, &data.weapon);
                } else {
                    int          chara_no = slot_no / 10;
                    CUserStatus *status = ShopUserStatusPt;
                    WEAPON_HAVE *row = status->chara_weapons[chara_no];
                    WEAPON_HAVE *weapon = &row[slot_no % 10];
                    memcpy(weapon, &data.weapon, sizeof(WEAPON_HAVE));
                    weapon->item_no = item_no;
                    board_info = &WeaponBoardInfo[0][slot_no];
                }

                break;
        }

        if (ChargeOrShopFlag) {
            *board_info = SHOP_SLOT_PLAYER_ITEM;
        }

        spare = -1;
        slot_no = -1;
        icon_no = -1;
        item_no = -1;
        pos_y = 0.0f;
        pos_x = 0.0f;
        memset(&data, 0, sizeof(data));
    }

    return 0;
}

void ShopIconMove::IconAutoMoveDraw() {
    if (item_no >= ITEM_ATTACH_START) {
        int kind = WhatIsKindofItem(item_no);

        if (ShopMenu.board.page == kind) {
            int num = 0;
            int item = item_no;

            if (item == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                num = data.param[1];
            }

            if (item >= ITEM_ATTACH_STAT_START && item < ITEM_ATTACH_GEM_START) {
                num = data.param[item - ITEM_ATTACH_STAT_START + 4];
            }

            DrawIconParts(item, pos_x, pos_y, 0, 0x280, 0x80, num);
        }
    }
}

/**
 * Returns an item's price, buying or selling, from the item table.
 *
 * @mangled GetItemMoney__Fii
 * @address 0x1E6EF0
 * @size 0x44
 */
static s16 GetItemMoney(int item_no, int sell) {
    if (sell != 0) {
        return PriceList[item_no - ITEM_ATTACH_START].sell_price;
    }

    return PriceList[item_no - ITEM_ATTACH_START].buy_price;
}

int ShopNoInput(int *tex_block, int shop_no, int mode) {
    if (shop_no < 0 || shop_no > 200) {
        shop_no = 0;
    }

    if (shop_no < 100) {
        if (shop_no > 20) {
            shop_no = 0;
        }

        InitItemShop2(tex_block, shop_no, mode);
        ChargeOrShopFlag = SHOP_KIND_ITEM;
    } else {
        shop_no -= 100;

        if (shop_no > 5) {
            shop_no = 0;
        }

        InitChargeShop(tex_block, shop_no, mode);
        ChargeOrShopFlag = SHOP_KIND_CHARGE;
    }

    return 1;
}

void InitAllHaveData() {
    InitHaveData(ShopHaveItemPt);
    InitHaveWep(ShopHaveWepPt);
    InitHaveAttach(ShopHaveAttachPt);
    ShopHaveItemPt->slot_state = SHOP_SLOT_EMPTY;
}

int CommonShopLoop() {
    int done = 0;

    ShopMenu.key_used = 0;

    switch (ChargeOrShopFlag) {
        case SHOP_KIND_CHARGE:
            done = ChargeShopLoop();
            break;
        case SHOP_KIND_ITEM:
            done = ItemShopLoop2();
            break;
    }

    return done;
}

/**
 * Places, scales and turns the shopkeeper's model for one shop.
 *
 * @mangled ShopPolySetInit__Fii
 * @address 0x1E7080
 * @size 0x150
 */
static void ShopPolySetInit(int shop_kind, int shop_no) {
    float place[2][18][3] = {
        {
         {-7.84f, -15.5f, 0.9f},
         {-7.3f, -9.555f, 0.25f},
         {-7.04f, -21.21f, 1.0f},
         {-7.56f, -18.967f, 1.0f},
         {-7.0f, -14.0f, 1.0f},
         {-7.3359f, -15.818726f, 0.95f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f},
         },
        {
         {-7.01f, -17.0f, 0.9f},
         {-7.21f, -7.8f, 0.5f},
         {-7.46f, -19.6f, 1.0f},
         {-8.0665f, -18.923f, 1.0f},
         {-8.0665f, -18.923f, 1.0f},
         {-8.0665f, -18.923f, 1.0f},
         {-8.0665f, -18.923f, 1.0f},
         {-7.0f, -20.5f, 1.0f},
         {-7.5378f, -15.362f, 0.8f},
         {-7.0f, -19.0f, 0.8f},
         {-7.0f, -15.0f, 1.0f},
         {-7.0f, -17.0f, 1.0f},
         {-7.6f, -18.7f, 0.8f},
         {-7.01f, -17.2f, 0.9f},
         {-7.21f, -7.8f, 0.5f},
         {-7.5378f, -18.5f, 0.8f},
         {-7.0f, -17.0f, 1.0f},
         {-7.0f, -17.0f, 1.0f},
         },
    };
    float turn[2][18][4] = {
        {
         {0.005f, 0.26413f, 0.0f, 0.0f},
         {0.05f, 0.249446f, 0.0f, 0.0f},
         {-0.033f, 0.2382f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         },
        {
         {-0.177612f, 0.23744f, 0.0f, 0.0f},
         {-0.177612f, 0.23744f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.070679f, 0.271298f, -0.02832f, 0.0f},
         {-0.050537f, 0.255213f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {-0.026733f, 0.251785f, 0.0f, 0.0f},
         {0.0f, 0.0f, 0.0f, 0.0f},
         },
    };
    float position[4];
    float scale[4];
    float rotation[4];

    position[0] = place[shop_kind][shop_no][0];
    position[1] = place[shop_kind][shop_no][1];
    position[2] = 1.0f;
    position[3] = 0.0f;

    for (int i = 0; i < 4; i++) {
        scale[i] = place[shop_kind][shop_no][2];
        rotation[i] = turn[shop_kind][shop_no][i];
    }

#ifdef PAL
    position[1] -= 0.5f;
#endif
    MenuCharaFrame.SetPosition(position);
    MenuCharaFrame.SetScale(scale);
    MenuCharaFrame.SetRotation(rotation);
}

/**
 * Records the line the shopkeeper is to say and how it is to be shown.
 *
 * @mangled SetItemShopTalkMode__Fii
 * @address 0x1E71D0
 * @size 0x50
 */
static void SetItemShopTalkMode(int talk_mode, int msg_mode) {
    if (talk_mode >= 0) {
        ShopMenu.talk_mode = talk_mode;
    }

    if (msg_mode >= 0) {
        ShopMenu.msg_mode = msg_mode;
    }

    if (ShopMenu.msg_mode == SHOP_MSG_TALKING) {
        CommonMenuMes3.mes_made = -1;
        ShopMenu.msg_no = -1;
    }
}

/**
 * Draws the icons of one shop board row.
 *
 * @mangled ShopIconDraw__FPsPUciiiiii
 * @address 0x1E7220
 * @size 0xFC
 */
static void ShopIconDraw(short *item_no, unsigned char *value, int count, int x, int y, int clip_top, int clip_bottom, int alpha) {
    int draw_x = x;
    int draw_y = y;
    int i;

    for (i = 0; i < count; i++) {
        DrawIconParts(item_no[i], draw_x, draw_y, clip_top, clip_bottom, alpha, value[i]);
        draw_x += 0x28;

        if (i % 5 == 4) {
            draw_x = x;
            draw_y += 0x28;
        }
    }
}

/**
 * Draws the left and right arrows of the recharge shop.
 *
 * @mangled ChargeShopLRDraw__Fi
 * @address 0x1E7320
 * @size 0x1AC
 */
static void ChargeShopLRDraw(int alpha) {
    int x = -1;
    int gap = x;

    switch (ShopMenu.side) {
        case SHOP_SIDE_STOCK:
            x = 0x38;
            gap = 0xD8;
            break;
        case SHOP_SIDE_PERSONAL:
            x = 0x146;
            gap = 0xE6;
            break;
    }

    if (0 < x) {
        CRect_i_ texel(0x62, 0x14, 0x1A, 0x18);
        int      y = 66.0f + 4.0f * sinf(PI * (CursorVibeCnt % 79 - 40) / 40.0f);

        DrawMenu2DSprite(PerBoardTex, CRect_i_(x, y, texel.width, texel.height), texel, alpha);
        texel.x += texel.width;
        DrawMenu2DSprite(PerBoardTex, CRect_i_(x + gap, y, texel.width, texel.height), texel, alpha);
    }
}

/**
 * Draws the shop cursor over the slot it stands on.
 *
 * @mangled ShopCurDraw__Fiiiiiii
 * @address 0x1E74D0
 * @size 0x2FC
 */
static void ShopCurDraw(int x, int y, int pos, int top, int mode, int select, int alpha) {
    float cur_x;
    float cur_y;
    int   frame_x;
    int   frame_y;
    int   vibe = select * 0x20 + 0x40;
    int   width;
    int   height;

    height = width = 0x26;

    switch (mode) {
        case 0: {
            cur_x = x + pos % 5 * 0x28;
            int row = pos / 5 - top;

            if (row < 0) {
                row = 0;
            }

            if (row > 3) {
                row = 3;
            }

            cur_y = y + row * 0x28;

            if (ShopMenu.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
#ifdef PAL
                cur_x = x + 0xE2;
#else
                cur_x = x + 0xD2;
#endif
                cur_y = y + 0x8C;
            }

            frame_x = (int) cur_x + 0x1E;
            frame_y = (int) cur_y - 0xA;
            break;
        }
        case 1:
            cur_x = x + 0x90;
            cur_y = y + 0x9E;
            frame_x = (int) cur_x + 0x1E;
            frame_y = (int) cur_y - 0xA;
            width = 0x64;
            break;
    }

    DrawMenuWaku(frame_x, frame_y, width, height, 0, StayTex, alpha);

    if (select) {
        cur_x += 34.0f;
        cur_y += 20.0f;
    }

    ShopMenu.cursor_x += (cur_x - ShopMenu.cursor_x) / 4.0f;
    ShopMenu.cursor_y += (cur_y - ShopMenu.cursor_y) / 4.0f;

    if (select == 2) {
        DrawMenuVibeItem(ShopMenu.cursor_x, ShopMenu.cursor_y, 0, -0xE, alpha);
    }

    DrawMenuObjectVibe(ShopMenu.cursor_x, ShopMenu.cursor_y, 1, vibe);
    CursorVibeCnt++;

    if (CursorVibeCnt >= 0x202FBF00) {
        CursorVibeCnt = 0;
    }
}

/**
 * Draws one shop icon at a position.
 *
 * @mangled DrawShopIcon__Fiiii
 * @address 0x1E77D0
 * @size 0x6C
 */
static void DrawShopIcon(int x, int y, int selected, int alpha) {
    int v = 0x50;
    int width = 0xA2;

    if (selected) {
        v += 0x28;
        width = 0xC8;
    }

    DrawMenu2DSprite(ShopBoard, CRect_i_(x, y, width, 0x28), CRect_i_(0x14, v, width, 0x28), alpha);
}

/**
 * Reports whether an item may be recharged.
 *
 * @mangled IsEnableCharge__Fi
 * @address 0x1E7840
 * @size 0x10C
 */
static int IsEnableCharge(int item_no) {
    int enable = false;

    if (item_no >= ITEM_WEAPON_START) {
        int          chara_no = WhoIsWeaponEquip(item_no);
        CUserStatus *status = ShopUserStatusPt;
        WEAPON_HAVE *weapons = status->chara_weapons[chara_no];
        int          default_no = GetDefaultWeaponNo(chara_no);

        for (int i = 0; i < 10; i++) {
            int weapon_no = weapons[i].item_no;

            if (weapon_no == default_no || weapon_no == default_no + 1) {
                enable = true;
                break;
            }
        }

        if (ShopUserStatusPt->party_size - 1 < chara_no) {
            enable = true;
        }

        if (ShopMenu.board.held_equipped_slot >= 0) {
            enable = false;
        }
    } else {
        enable = true;

        if (item_no == ITEM_MOON_ORB) {
            enable = false;
        }
    }

    return enable;
}

/**
 * Starts a shop menu up: its buffers, its textures, its board mode and the gamepad.
 *
 * @mangled ShopMenuInit__FPiii
 * @address 0x1E7950
 * @size 0x248
 */
static void ShopMenuInit(int *tex_block, int shop_no, int mode) {
    u_long128 *buffer;
    int        limit;

    buffer = BtlMenuBufferSet(mode);
    buffer = MenuCalcBufAlignment(buffer);
    limit = EdMenuBuffer.limit >> 1;
    ShopCashBuffer.base = (u_char *) buffer;
    ShopCashBuffer.limit = limit;
    ShopCashBuffer.used = 0;
    ShopUserStatusPt = (CUserStatus *) SaveData->GetDngStatus();
    ShopMenu.lang = GetMenuLangFlag();
    StartReadBG();
    ShopCashBuffer.Alloc((LoadFileBGMenuData("itemshop.pak", buffer) >> 4) + 0x102);
    ShopMenu.shop_no = shop_no;
    ShopMenu.mode = mode;
    ShopMenu.tex_block = tex_block[0];
    ShopMenu.ready = 0;
    ShopMenu.person_tex_block = tex_block[1];
    ShopMenu.person_state = 1;
    ShopMenu.alpha = 0;
    ShopMenu.side = SHOP_SIDE_PERSONAL;
    InitPersonalBoardMode(ShopUserStatusPt, &ShopMenu.board, PERSONAL_BOARD_ITEM, BOARD_PAGE_ITEM);
    ShopHaveItemPt = &ShopMenu.board.held_item;
    ShopHaveWepPt = &ShopMenu.board.weapon;
    ShopHaveAttachPt = &ShopMenu.board.held_attach;
    ShopMenu.stock_top_row = 0;
    ShopMenu.stock_row_offset = 0;
    ShopMenu.step_count = 0;
    ShopMenu.talk_mode = SHOP_TALK_FADE_IN;
    StayTex = TexManager.GetTexture("stayframe", -1);
    ItemVolumeStep.CheckItemVolume();
    ShopDataMove.spare = -1;
    ShopDataMove.slot_no = -1;
    ShopDataMove.icon_no = -1;
    ShopDataMove.item_no = -1;
    ShopDataMove.pos_y = 0.0f;
    ShopDataMove.pos_x = 0.0f;
    memset(&ShopDataMove.data, 0, sizeof(ShopDataMove.data));
    SetMenuTrushMark(ShopUserItemPack(ShopUserStatusPt));
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);
}

/**
 * Tears a shop menu down: its textures, its messages and the gamepad.
 *
 * @mangled ShopMenuExit__Fv
 * @address 0x1E7BA0
 * @size 0x12C
 */
static void ShopMenuExit() {
    ITEM_PACK *pack = ShopUserItemPack(ShopUserStatusPt);
    int        i;
    int        j;

    pack->item_count = 0;

    for (i = 0; i < 3; i++) {
        if (pack->quick_item_slot[i] >= ITEM_DUNGEON_START) {
            pack->item_count += (s8) pack->quick_item_qty[i];
        }
    }

    for (j = 0; j < pack->num; j++) {
        if (pack->item[j] >= ITEM_DUNGEON_START) {
            pack->item_count++;
        }
    }

    DeleteMenuTrushMark();
    CommonMenuMes3.auto_pos = MES_POS_NONE;
    AtoraNameMes.rows = 4;
    TexManager.DeleteTextureBlock(ShopMenu.tex_block);
    TexManager.DeleteTextureBlock(ShopMenu.person_tex_block);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

void ShopTextureLoadFix() {
    LOADTEXTURE_INFO2 info[3] = {
        {"#frame_imageshop#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {NULL,                                           0, 0},
        {NULL,                                           0, 0},
    };

    info[0].block_no = ShopMenu.tex_block;
    info[1].block_no = ShopMenu.tex_block;
    BG_READ_INFO *file = GetReadBGFile(0);
    info[1].name = (char *) GetPackFile((u_int *) file->buffer, "itemshop.img", NULL);
    TexManager.DeleteTextureBlock(ShopMenu.tex_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, info);
    ShopBoard = TexManager.GetTexture("shopbrd", ShopMenu.tex_block);
    PerBoardTex = TexManager.GetTexture("perbrd", ShopMenu.tex_block);
    WepIcon = TexManager.GetTexture("wepicon", ShopMenu.tex_block);
    ItemIcon = TexManager.GetTexture("itemicon", ShopMenu.tex_block);
    ShopMenu.ready = 1;
    s16 *shop_messages = (s16 *) GetPackFile((u_int *) file->buffer, "itemshop.bin", NULL);
    s16 *keeper_messages = (s16 *) GetPackFile((u_int *) file->buffer, "shopman.bin", NULL);
    InitMenuMesSet(MENU_MES_SET_SHOP, shop_messages);
    CommonMenuMes3.SetBuff(keeper_messages);
}

/**
 * Draws the fade that covers the shop while it opens and closes.
 *
 * @mangled ShopFadeoutDraw__Fv
 * @address 0x1E7EA0
 * @size 0xF8
 */
static void ShopFadeoutDraw() {
    int alpha = 0;

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_FADE_IN:
            alpha = 0x80 - ShopMenu.step_count * 3;
            break;
        case SHOP_TALK_FADE_OUT:
            alpha = ShopMenu.step_count * 3 + 0x40;
            break;
    }

    if (alpha < 0) {
        alpha = 0;
    }

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    FrameImageDraw(0x40, alpha);

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_NONE:
        case SHOP_TALK_GOODBYE:
            ShopMenu.step_count = 0;
            break;
        default:
            if (ShopMenu.ready != 0) {
                ShopMenu.step_count++;
            }

            break;
    }
}

/**
 * Starts reading a shopkeeper's model in the background.
 *
 * @mangled ShopPersonReadStart__Fii
 * @address 0x1E7FA0
 * @size 0x98
 */
static int ShopPersonReadStart(int shop_kind, int shop_no) {
    char       file_name[76];
    int        size;
    u_long128 *buffer;

    ItemShopGetPacFileName(shop_kind, shop_no, file_name);
    buffer = (u_long128 *) (ShopCashBuffer.base + ShopCashBuffer.used * 16);
    buffer = MenuCalcBufAlignment(buffer);
    StartReadBG();

    if (!LoadFileBG(file_name, buffer, &size)) {
#ifdef PAL
        printf("load is failed\n");
#endif
        return 0;
    }

    ReadBG();
    ShopMenu.person_state = 0;
    return 1;
}

/**
 * Sets the shopkeeper's model direction toward the menu camera.
 */
static void ShopMasterVectorSet(int mode);

/**
 * Stores in @p rest how many quadwords of a @p total quadword buffer are left once @p used are taken.
 */
static inline void GetRestSize(int &rest, int total, int used) {
    rest = total - used;
}

/**
 * Builds the shopkeeper's model using the current shop data.
 *
 * @mangled ShopPersonBuild__Fii
 * @address 0x1E8040
 * @size 0x268
 */
static int ShopPersonBuild(int shop_kind, int shop_no) {
    char name[64];
    int  size;

    if (ShopMenu.person_state == 0 && ReadBGSync() != 0) {
        return 0;
    }

    ItemShopGetImgFileName(shop_kind, shop_no, name);
    BG_READ_INFO     *file = GetReadBGFile(0);
    u_int            *pack = (u_int *) file->buffer;
    u_char           *model_area = (u_char *) pack + ((file->size >> 4) + 1) * 16;
    LOADTEXTURE_INFO2 texture[3] = {
        {"#frame_menushop_model#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {NULL,                                                0, 0},
        {NULL,                                                0, 0},
    };
    texture[0].block_no = ShopMenu.person_tex_block;
    texture[1].block_no = ShopMenu.person_tex_block;
    texture[1].name = (char *) GetPackFile(pack, name, &size);
    TexManager.DeleteTextureBlock(ShopMenu.person_tex_block);
    TexManager.LoadTextureBlockEX(-1, texture);
    GetRestSize(size, EdMenuBuffer.limit, ShopCashBuffer.limit);
    int limit = size;
    ShopCashBuffer2.base = model_area;
    ShopCashBuffer2.limit = limit;
    ShopCashBuffer2.used = 0;
    MenuCharaFrame.Initialize();
    MenuCharaFrame.LoadPackData(pack, "info.cfg", &ShopCashBuffer2, &ShopCashBuffer2, NULL);
    sceVu0FVECTOR position = {-7.0f, -17.0f, 0.0f, 1.0f};
    MenuCharaFrame.SetPosition(position);
    ShopPolySetInit(shop_kind, shop_no);

    if (MenuCharaFrame.frame == NULL) {
        printf("Frame is NULL\n");
        return 0;
    }

    ShopMenu.motion = 3;
    MenuCharaFrame.SetMotion(ShopMenu.motion, 0);
    SetItemShopTalkMode(SHOP_TALK_GREETING, SHOP_MSG_TALKING);
    ShopMenu.idle_count = 0;
    ShopMenu.msg_no = 0;
    ShopMasterVectorSet(0);
    ShopMenu.draw_delay = 0;
    return 2;
}

/**
 * Draws the shopkeeper and points the menu camera at them.
 *
 * @mangled LocalShopPersonDraw__Fv
 * @address 0x1E82B0
 * @size 0xB8
 */
static void LocalShopPersonDraw() {
    float ref[4] = {0.0f, 0.0f, -30.0f, 1.0f};
    float pos[4] = {0.0f, 0.0f, 30.0f, 1.0f};

    MenuCamera.SetRef(ref);
    MenuCamera.SetPos(pos);
    ShopMasterVectorSet(0);
    MenuCharaFrame.Step();

    if (ShopMenu.draw_delay < 4) {
        ShopMenu.draw_delay++;
    } else {
        MenuCharaFrame.Draw();
    }
}

/**
 * Draws the shopkeeper.
 *
 * @mangled ShopPersonDraw__Fi
 * @address 0x1E8370
 * @size 0x3C
 */
static void ShopPersonDraw(int shop_no) {
    MenuTextureReload(ShopMenu.person_tex_block);
    MenuPolygonDraw(0x80, LocalShopPersonDraw);
}

/**
 * Points the shopkeeper's model at the camera.
 *
 * @mangled ShopMasterVectorSet__Fi
 * @address 0x1E83B0
 * @size 0xD8
 * Turns the shopkeeper to face the menu camera.
 */
static void ShopMasterVectorSet(int mode) {
    float camera_pos[4];
    float pos[4];
    float rotation[4];
    float offset[4];
    float angle;

    switch (mode) {
        case 0:
            MenuCamera.GetPos(camera_pos);
            break;
        case 1:
            break;
    }

    sceVu0CopyVector(pos, MenuCharaFrame.pos);
    MenuCharaFrame.GetRotation(rotation);
    sceVu0SubVector(offset, pos, camera_pos);
    angle = atan2f(-offset[0], -offset[2]);
    rotation[1] = AngleInterpolate(rotation[1], angle, 0.1f, INTERPOLATE_STEP);
    MenuCharaFrame.SetRotation(rotation);
}

void InitChargeShop(int *tex_block, int shop_no, int mode) {
    int rows;

    if (shop_no >= 100) {
        shop_no -= 100;
    }

    ShopMenuInit(tex_block, shop_no, mode);
    ShopMenu.board.cursor = 0;
    ShopMenu.side = SHOP_SIDE_PERSONAL;
    ShopMenu.cursor_x = (ShopMenu.board.cursor % 5) * 0x28 + 0x154;
    ShopMenu.cursor_y = 120.0f;
    ShopStockPt = SaveData->GetStockItem();
    rows = ChargeShopMax[0] / 5;
    ShopMenu.stock_top_row = 0;
    ShopMenu.stock_y = 0x8A - ShopMenu.stock_top_row * 0x28;
    ShopMenu.stock_scroll = 142.0f + 114.0f * ShopMenu.stock_top_row / rows;
    ShopMenu.stock_row_offset = 0;
    GetMainMenuRightHelpWinLangOffset(ShopHelpWinPos[0], ShopHelpWinPos[1], ShopHelpWinW, ShopHelpWinH);
}

void ChargeShopLimmitCheck() {
    int max = ChargeShopMax[ShopMenu.board.page];
    int top = ShopMenu.stock_top_row;
    int rows = max / 5;
    int last_top = rows - 4;

    if (last_top < 0) {
        last_top = 0;
    }

    int cursor = ShopMenu.board.cursor;

    if (max - 1 < cursor && ShopMenu.side == SHOP_SIDE_STOCK) {
        while (ShopMenu.board.cursor >= max) {
            ShopMenu.board.cursor -= 5;
        }

        ShopMenu.stock_top_row = ShopMenu.board.cursor / 5 - 3;

        if (ShopMenu.stock_top_row < 0) {
            ShopMenu.stock_top_row = 0;
        }

        ShopMenu.stock_y = 0x8A - ShopMenu.stock_top_row * 0x28;

        if (rows <= 0) {
            rows = 4;
        }

        ShopMenu.stock_scroll = 142.0f + 114.0f * ShopMenu.stock_top_row / rows;
        return;
    }

    if (last_top < top) {
        while (last_top < ShopMenu.stock_top_row) {
            ShopMenu.stock_top_row--;
        }

        ShopMenu.stock_y = 0x8A - ShopMenu.stock_top_row * 0x28;
        ShopMenu.stock_scroll = 142.0f + 114.0f * ShopMenu.stock_top_row / rows;
    }
}

static void ExitChargeShop() {
    ShopMenuExit();
}

int ChargeShopLoop() {
    int done;

    ReadBG();
    done = ChargeShopKey();
    DrawChargeShop();

    if (done) {
        ExitChargeShop();
    }

    return done;
}

static inline void ShopDataMoveClear() {
    ShopDataMove.spare = -1;
    ShopDataMove.slot_no = -1;
    ShopDataMove.icon_no = -1;
    ShopDataMove.item_no = -1;
    ShopDataMove.pos_y = 0.0f;
    ShopDataMove.pos_x = 0.0f;
    memset(&ShopDataMove.data, 0, sizeof(WEAPON_HAVE));
}

static inline void ShopDataMoveFinish() {
    if (ShopDataMove.icon_no >= 0) {
        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 1);
        ShopDataMoveClear();
    }
}

static inline WEAPON_HAVE *ShopWeaponRow(CUserStatus *status, int chara) {
    return status->chara_weapons[chara];
}

int ChargeShopKey() {
    int           result = 0;
    MENU_ITEMDATA record;
    MENU_ITEMDATA stock;
    MENU_ITEMDATA weapon;
    ATTACH_LIST   attach;

    if (ShopMenu.ready == 0) {
        if (ReadBGSync() == 0) {
            ShopTextureLoadFix();
#ifdef PAL
            printf("Model Read Start\n");
#endif
            ShopPersonReadStart(SHOP_KIND_CHARGE, ShopMenu.shop_no);
        }
    } else {
        switch (ShopMenu.talk_mode) {
            case SHOP_TALK_FADE_IN:
                ShopMenu.alpha += 8;

                if (ShopMenu.alpha >= 0x80) {
                    ShopMenu.alpha = 0x80;
                }

                if (ShopMenu.step_count > 20 && ShopMenu.ready != 0 && ShopMenu.alpha >= 0x80) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                }

                break;
            case SHOP_TALK_FADE_OUT:
                ShopMenu.alpha -= 8;

                if (ShopMenu.alpha <= 0) {
                    ShopMenu.alpha = 0;
                }

                if (ShopMenu.step_count > 26 && ShopMenu.alpha <= 0) {
                    result = 1;
                }

                break;
            case SHOP_TALK_GREETING:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_DPAD) != 0 || GamePad.AllOn() != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.page_arrow = false;
                }

                break;
            case SHOP_TALK_STOCK_FULL:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_DPAD) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.page_arrow = false;
                }
            case SHOP_TALK_CANNOT_CHARGE:
            case SHOP_TALK_INVENTORY_OVERFLOW:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.page_arrow = false;
                }

                break;
            case SHOP_TALK_CHARGE_PUT_PROMPT:
                if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else if (GamePad.Down(PAD_TRIANGLE | PAD_CROSS) != 0) {
                    ShopDataMoveFinish();
                    int space = ShopStockPt->GetNowModeSpace(ShopMenu.board.page);
                    int icon = space - ShopMenu.stock_top_row * 5;
                    s16 cursor = ShopMenu.board.cursor;
                    int item_no;

                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM: {
                            ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                            item_no = pack->item[cursor];
                            record.volume = pack->item_vol[cursor];
                            printf("charge vol = %d\n", record.volume);
                            pack->item[cursor] = -1;
                            pack->item_vol[cursor] = 0;
                            break;
                        }
                        case BOARD_PAGE_ATTACH: {
                            ATTACH_LIST *list = &((ATTACH_LIST *) ShopUserStatusPt->consumable_items)[cursor];
                            item_no = list->item_no;
                            memcpy(&record, list, sizeof(ATTACH_LIST));
                            memset(list, 0, sizeof(ATTACH_LIST));
                            break;
                        }
                        case BOARD_PAGE_WEAPON: {
                            WEAPON_HAVE *have = &ShopWeaponRow(ShopUserStatusPt, cursor / 10)[cursor % 10];
                            item_no = have->item_no;
                            memcpy(&record, have, sizeof(WEAPON_HAVE));
                            memset(have, 0, sizeof(WEAPON_HAVE));
                            break;
                        }
                    }

                    int row = ShopMenu.board.cursor / 5 - ShopMenu.board.top_row;
                    int x = ShopMenu.board.cursor % 5 * 0x28 + 0x156;
                    int y = row * 0x28 + 0x94;
                    ShopDataMove.IconMoveTarSet(space, icon, item_no, &record, x, y, 1);

                    if (icon < 0 || icon >= 20) {
                        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 1);
                        ShopDataMoveClear();
                    }

                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                break;
            case SHOP_TALK_CHARGE_TAKE_PROMPT:
                if (GamePad.Down(PAD_TRIANGLE | PAD_CROSS) != 0) {
                    int page;
                    s16 stock_item;
                    s16 volume;
                    ShopDataMoveFinish();
                    stock_item = 0;
                    memset(&stock, 0, sizeof(stock));

                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM:
                            volume = 0;
                            ShopStockPt->SetItemToPos(ShopMenu.board.cursor, &stock_item, &volume);
                            stock.volume = volume;
                            break;
                        case BOARD_PAGE_ATTACH:
                            ShopStockPt->SetAttachToPos(ShopMenu.board.cursor, &stock.attach);
                            stock_item = stock.attach.item_no;
                            break;
                        case BOARD_PAGE_WEAPON:
                            ShopStockPt->SetWepToPos(ShopMenu.board.cursor, &stock.weapon);
                            stock_item = stock.weapon.item_no;
                            break;
                    }

                    int space = GetBoardSpace(stock_item, &page);
                    int icon = space - ShopMenu.board.top_row * 5;
                    int row = ShopMenu.board.cursor / 5 - ShopMenu.stock_top_row;
                    int x = ShopMenu.board.cursor % 5 * 0x28 + 0x34;
                    int y = row * 0x28 + 0x94;
                    ShopDataMove.IconMoveTarSet(space, icon, stock_item, &stock, x, y, 0);

                    if (icon < 0 || icon >= 20) {
                        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 1);
                        ShopDataMoveClear();
                    }

                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            case SHOP_TALK_GOODBYE:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_FADE_OUT;
                    ShopMenu.step_count = 0;
                    CommonMenuMes3.page_arrow = false;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                break;
            case SHOP_TALK_LEAVE_PROMPT:
                CommonMenuMes3.page_arrow = false;

                if (GamePad.Down(PAD_CROSS) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    CommonMenuMes3.page_arrow = false;
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_GOODBYE, SHOP_MSG_TALKING);
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.page_arrow = false;
                }

                break;
            case SHOP_TALK_NONE: {
                if (ShopMenu.person_state == 0) {
                    ShopMenu.person_state = ShopPersonBuild(SHOP_KIND_CHARGE, ShopMenu.shop_no);
                }

                int page = ShopMenu.board.page;
                int cursor = ShopMenu.board.cursor;
                int board_exit = 0;

                switch (ShopMenu.side) {
                    case SHOP_SIDE_STOCK:
                        board_exit = ChargeSelectKey();
                        break;
                    case SHOP_SIDE_PERSONAL:
                        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
                            BoardModeChangeKey();
                            PersonalBoardLimmitCheck();
                            ChargeShopLimmitCheck();
                        }

                        board_exit = PersonalBoardKeySub();

                        if (board_exit == 0 && GamePad.Down(PAD_CROSS) != 0) {
                            s16             held = ShopHaveItemPt->item_no;
                            PERSONAL_BOARD *board = &ShopMenu.board;

                            switch (board->cursor_area) {
                                case PERSONAL_BOARD_AREA_TRASH: {
                                    if (held < ITEM_ATTACH_START) {
                                        ComMenuSePlay(MENU_SOUND_REFUSE);
                                    } else {
                                        if (IsEnableTrushThrow(held) != 0) {
                                            board->trash_anim = 1;
                                            board->trash_frame = 0;
                                            InitAllHaveData();
                                        }

                                        ComMenuSePlay(MENU_SOUND_REFUSE);
                                    }

                                    break;
                                }
                                case PERSONAL_BOARD_AREA_CELLS:
                                    if (PersonalBoardItemGetorSwap(ShopMenu.board.cursor) != 0) {
                                        if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                                            ShopHaveItemPt->slot_state = SHOP_SLOT_SHOP_GOOD;
                                        } else {
                                            InitAllHaveData();
                                            ShopMenu.board.held_equipped_slot = -1;
                                        }

                                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    } else {
                                        ComMenuSePlay(MENU_SOUND_REFUSE);
                                    }

                                    break;
                            }
                        } else if (board_exit == 0 && GamePad.Down(PAD_SQUARE) != 0) {
                            switch (ShopMenu.board.page) {
                                case BOARD_PAGE_ITEM:
                                    SeitonItemBoard(&ShopUserStatusPt->item_pack);
                                    SetMenuTrushMark(&ShopUserStatusPt->item_pack);
                                    break;
                                case BOARD_PAGE_ATTACH:
                                    SeitonAttachBoard((ATTACH_LIST *) ShopUserStatusPt->consumable_items);
                                    break;
                            }

                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                        } else if (board_exit == 0 && GamePad.Down(PAD_TRIANGLE) != 0) {
                            int enable = true;

                            if (ShopMenu.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                s16 target = SearchBoardNowPosItemExist(ShopMenu.board.page, ShopMenu.board.cursor);

                                switch (ShopMenu.board.page) {
                                    case BOARD_PAGE_ITEM:
                                    case BOARD_PAGE_ATTACH:
                                        break;
                                    case BOARD_PAGE_WEAPON: {
                                        int owner = WhoIsWeaponEquip(target);

                                        if (target < ITEM_WEAPON_START) {
                                            ComMenuSePlay(MENU_SOUND_REFUSE);
                                        } else {
                                            if (owner == IsDefaultWeapon(target)) {
                                                SetItemShopTalkMode(SHOP_TALK_CANNOT_CHARGE, SHOP_MSG_TALKING);
                                                enable = false;
                                            }

                                            s8 slot = ShopUserStatusPt->equipped_weapon_slot[owner];

                                            if (ShopMenu.board.cursor % 10 == slot) {
                                                SetItemShopTalkMode(SHOP_TALK_CANNOT_CHARGE, SHOP_MSG_TALKING);
                                                enable = false;
                                            }
                                        }

                                        break;
                                    }
                                }

                                if (enable != 0) {
                                    enable = IsEnableCharge(target);

                                    if (enable == 0) {
                                        SetItemShopTalkMode(SHOP_TALK_CANNOT_CHARGE, SHOP_MSG_TALKING);
                                    }
                                }

                                if (ShopStockPt->GetNowModeSpace(ShopMenu.board.page) < 0) {
                                    enable = false;
                                    SetItemShopTalkMode(SHOP_TALK_STOCK_FULL, SHOP_MSG_TALKING);
                                }

                                if (target < ITEM_ATTACH_START) {
                                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                                    enable = false;
                                }

                                if (enable != 0) {
                                    ShopMenu.talk_mode = SHOP_TALK_CHARGE_PUT_PROMPT;
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            }
                        }

                        break;
                }

                if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);

                    if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
                        SetItemShopTalkMode(SHOP_TALK_LEAVE_PROMPT, SHOP_MSG_TALKING);
                    } else {
                        switch (ShopHaveItemPt->slot_state) {
                            case SHOP_SLOT_SHOP_GOOD:
                                PersonalBoardItemCancel();
                                break;
                            case SHOP_SLOT_EMPTY:
                                switch (ShopHaveItemPt->from_page) {
                                    case BOARD_PAGE_ITEM:
                                        ShopStockPt->SetItemToPos(ShopHaveItemPt->from_slot, &ShopHaveItemPt->item_no, &ShopHaveItemPt->volume);
                                        break;
                                    case BOARD_PAGE_WEAPON: {
                                        ShopStockPt->GetWeaponInfo(ShopHaveItemPt->from_slot, &weapon.weapon);
                                        s16 stock_item = weapon.weapon.item_no;
                                        ShopStockPt->SetWepToPos(ShopHaveItemPt->from_slot, ShopHaveWepPt);
                                        ShopHaveItemPt->item_no = stock_item;
                                        break;
                                    }
                                    case BOARD_PAGE_ATTACH: {
                                        ShopStockPt->GetAttachInfo(ShopHaveItemPt->from_slot, &attach);
                                        s16 stock_item = attach.item_no;
                                        ShopStockPt->SetAttachToPos(ShopHaveItemPt->from_slot, ShopHaveAttachPt);
                                        ShopHaveItemPt->item_no = stock_item;
                                        break;
                                    }
                                }

                                if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
                                    InitAllHaveData();
                                }

                                break;
                        }
                    }
                }

#ifdef PAL
                // Debug shortcut: raises or lowers the party's Gilda.
                if (DebugMode) {
                    int step;
                    int money = ShopUserStatusPt->money;

                    step = 0;

                    if (GamePad.On2(PAD_UP) && money < 9999) {
                        step++;
                    }

                    if (GamePad.On2(PAD_DOWN) && 0 < money) {
                        step--;
                    }

                    CUserStatus *status = ShopUserStatusPt;
                    int          total = status->money + step;

                    if (total >= 0xFFFF) {
                        status->money = 0xFFFF;
                    } else {
                        status->money = total;
                    }
                }

#endif
                switch (board_exit) {
                    case 0:
                        break;
                    case 1:
                        ShopMenu.side = SHOP_SIDE_STOCK;
                        ShopMenu.board.cursor = (ShopMenu.stock_top_row + (ShopMenu.board.cursor / 5 - ShopMenu.board.top_row)) * 5 + 4;
                        break;
                }

                if (cursor != ShopMenu.board.cursor || page != ShopMenu.board.page) {
                    int max = ChargeShopMax[ShopMenu.board.page];
                    int top = ShopMenu.stock_top_row;
                    int rows = max / 5;
                    int last_top = rows - 4;

                    if (last_top < 0) {
                        last_top = 0;
                    }

                    if (last_top < top) {
                        ShopMenu.stock_top_row = ShopMenu.board.cursor / 5 - 3;

                        if (ShopMenu.stock_top_row < 0) {
                            ShopMenu.stock_top_row = 0;
                        }

                        ShopMenu.stock_y = 0x8A - ShopMenu.stock_top_row * 0x28;

                        if (rows <= 0) {
                            rows = 4;
                        }

                        ShopMenu.stock_scroll = 142.0f + 114.0f * ShopMenu.stock_top_row / rows;
                    }

                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }

                break;
            }
        }

        int mes_no = 0;
        int item = 0;
        int value = 0;
        int name_mes = -1;
        int cursor = ShopMenu.board.cursor;

        if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
            item = ShopHaveItemPt->item_no;
            COM_ITEM_INFO *info = GetCommonItemInfo(item);
            mes_no = info != NULL ? info->msg + 500 : mes_no;
            value = GetAttachVolumeForMsg(ShopHaveAttachPt);

            if (ShopHaveItemPt->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                name_mes = ShopHaveAttachPt->sphere_weapon_no + 100;
            }
        }

        switch (ShopMenu.side) {
            case SHOP_SIDE_STOCK:
                switch (ShopMenu.board.page) {
                    case BOARD_PAGE_ITEM:
                        item = ShopStockPt->dungeon_items[cursor];
                        break;
                    case BOARD_PAGE_WEAPON: {
                        COM_ITEM_INFO *info = GetCommonItemInfo(ShopStockPt->weapons[cursor].item_no);

                        if (info != NULL) {
                            item = info->msg;
                        } else {
                            item = -1;
                        }

                        if (item >= ITEM_WEAPON_START) {
                            value = ShopStockPt->weapons[cursor].level;
                        }

                        break;
                    }
                    case BOARD_PAGE_ATTACH:
                        item = ShopStockPt->attachments[cursor].item_no;

                        if (item >= ITEM_ATTACH_START) {
                            mes_no = item + 500;
                            value = GetAttachVolumeForMsg(&ShopStockPt->attachments[cursor]);

                            if (item == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                                name_mes = ShopStockPt->attachments[cursor].sphere_weapon_no + 100;
                            }
                        }

                        break;
                }

                break;
            case SHOP_SIDE_PERSONAL:
                switch (ShopMenu.board.page) {
                    case BOARD_PAGE_ITEM: {
                        ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                        item = pack->item[cursor];

                        if (item >= ITEM_DUNGEON_START) {
                            mes_no = item + 500;
                        }

                        break;
                    }
                    case BOARD_PAGE_WEAPON: {
                        CUserStatus   *status = ShopUserStatusPt;
                        WEAPON_HAVE   *row = status->chara_weapons[cursor / 10];
                        WEAPON_HAVE   *have = &row[cursor % 10];
                        COM_ITEM_INFO *info = GetCommonItemInfo(have->item_no);

                        if (info != NULL) {
                            item = info->msg;
                        } else {
                            item = -1;
                        }

                        if (item >= 0) {
                            mes_no = item + 500;
                            value = have->level;
                        }

                        break;
                    }
                    case BOARD_PAGE_ATTACH: {
                        ATTACH_LIST *list = &((ATTACH_LIST *) ShopUserStatusPt->consumable_items)[cursor];
                        item = list->item_no;

                        if (item >= ITEM_ATTACH_START) {
                            mes_no = item + 500;
                            value = GetAttachVolumeForMsg(list);

                            if (item == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                                name_mes = list->sphere_weapon_no + 100;
                            }
                        }

                        break;
                    }
                }

                if (ShopMenu.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                    mes_no = 1000;
                }

                break;
        }

        if (item > 0) {
            mes_no = item + 500;
        }

        if (CommonMenuMes2.mes_made != mes_no || (name_mes > 0 && CommonMenuMes2.mes_no[0] != name_mes) || CommonMenuMes2.value != value) {
            CommonMenuMes2.value_signed = true;
            CommonMenuMes2.value = value;

            if (name_mes > 0) {
                CommonMenuMes2.mes_no[0] = name_mes;
            }

            if (mes_no <= 500) {
                mes_no = 0;
            }

            printf("msgno = %d\n", mes_no);
            CommonMenuMes2.mes_made = -1;
            CommonMenuMes2.MakeMesWin(mes_no);
        }
    }

    return result;
}

/**
 * Moves the cursor across the recharge shop's list.
 *
 * @mangled ChargeSelectKey__Fv
 * @address 0x1E9EE0
 * @size 0x8C0
 */
static int ChargeSelectKey() {
    if (ShopHaveItemPt->item_no < ITEM_ATTACH_START && BoardModeChangeKey()) {
        PersonalBoardLimmitCheck();
        ChargeShopLimmitCheck();
    }

    int count = ChargeShopMax[ShopMenu.board.page];

    if (GamePad.Down(PAD_UP)) {
        ShopMenu.board.cursor -= 5;

        if (ShopMenu.board.cursor < 0) {
            ShopMenu.board.cursor += 5;
        }

        if (ShopMenu.board.cursor / 5 < ShopMenu.stock_top_row) {
            ShopMenu.stock_top_row--;
        }
    }

    if (GamePad.Down(PAD_DOWN)) {
        if (ShopMenu.board.cursor < count - 5) {
            ShopMenu.board.cursor += 5;
        }

        if (ShopMenu.stock_top_row + 3 < ShopMenu.board.cursor / 5) {
            ShopMenu.stock_top_row++;
        }
    }

    if (GamePad.Down(PAD_LEFT) && ShopMenu.board.cursor % 5 != 0) {
        ShopMenu.board.cursor--;
    }

    if (GamePad.Down(PAD_RIGHT)) {
        if (ShopMenu.board.cursor % 5 == 4) {
            ShopMenu.side = SHOP_SIDE_PERSONAL;
            ShopMenu.board.cursor = (ShopMenu.board.top_row + (ShopMenu.board.cursor / 5 - ShopMenu.stock_top_row)) * 5;
        } else {
            ShopMenu.board.cursor++;
        }
    }

    switch (ShopMenu.side) {
        case SHOP_SIDE_STOCK:
            int       se = 2;
            IHAVEITEM saved;
            memcpy(&saved, ShopHaveItemPt, sizeof(IHAVEITEM));

            if (GamePad.Down(PAD_CROSS)) {
                // Put the held item into the stock slot under the cursor.
                if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START && !IsEnableCharge(ShopHaveItemPt->item_no)) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    SetItemShopTalkMode(SHOP_TALK_CANNOT_CHARGE, SHOP_MSG_TALKING);
                    return 0;
                }

                int mode = ShopMenu.board.page;
                int kind = WhatIsKindofItem(ShopHaveItemPt->item_no);

                if (mode != kind && kind != BOARD_PAGE_NONE) {
                    se = 2;
                } else {
                    switch (mode) {
                        case BOARD_PAGE_ITEM:
                            ShopStockPt->SetItemToPos(ShopMenu.board.cursor, &ShopHaveItemPt->item_no, &ShopHaveItemPt->volume);
                            break;
                        case BOARD_PAGE_WEAPON:
                            ShopStockPt->SetWepToPos(ShopMenu.board.cursor, ShopHaveWepPt);

                            if (ShopHaveWepPt->item_no < ITEM_ATTACH_START) {
                                ShopHaveItemPt->item_no = -1;
                                InitHaveWep(ShopHaveWepPt);
                                ShopMenu.board.held_equipped_slot = -1;
                            } else {
                                ShopHaveItemPt->item_no = ShopHaveWepPt->item_no;
                            }

                            break;
                        case BOARD_PAGE_ATTACH:
                            ShopStockPt->SetAttachToPos(ShopMenu.board.cursor, ShopHaveAttachPt);

                            if (ShopHaveAttachPt->item_no < ITEM_ATTACH_START) {
                                ShopHaveItemPt->item_no = -1;
                                InitHaveAttach(ShopHaveAttachPt);
                                ShopMenu.board.held_equipped_slot = -1;
                            } else {
                                ShopHaveItemPt->item_no = ShopHaveAttachPt->item_no;
                            }

                            break;
                    }

                    ShopHaveItemPt->slot_state = SHOP_SLOT_EMPTY;
                }

                if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
                    InitAllHaveData();
                } else {
                    ShopHaveItemPt->slot_state = SHOP_SLOT_EMPTY;
                    ShopHaveItemPt->from_page = ShopMenu.board.page;
                    ShopHaveItemPt->from_slot = ShopMenu.board.cursor;
                    se = 1;
                }

                if (memcmp(&saved, ShopHaveItemPt, sizeof(IHAVEITEM)) != 0 || se != 2) {
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_TRIANGLE)) {
                // Check whether the good under the cursor can be taken for charging.
                if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return 0;
                }

                int         ok = 1;
                ATTACH_LIST attach;
                WEAPON_HAVE weapon;
                int         kind;
                int         full;
                s16         item;
                s16         volume;

                switch (ShopMenu.board.page) {
                    case BOARD_PAGE_ITEM:
                        ShopStockPt->GetItemInfo(ShopMenu.board.cursor, &item, &volume);
                        break;
                    case BOARD_PAGE_ATTACH:
                        ShopStockPt->GetAttachInfo(ShopMenu.board.cursor, &attach);
                        item = attach.item_no;
                        break;
                    case BOARD_PAGE_WEAPON:
                        ShopStockPt->GetWeaponInfo(ShopMenu.board.cursor, &weapon);
                        item = weapon.item_no;
                        break;
                }

                GetBoardSpace(item, &kind);
                full = 0;
                int used = 0;
                int max = 0;
                GetNowModeMaxNum(ShopMenu.board.page, &full);

                switch (kind) {
                    case BOARD_PAGE_ITEM: {
                        ITEM_PACK *pack = ShopUserItemPack(ShopUserStatusPt);

                        max = pack->num;

                        for (int i = 0; i < 3; i++) {
                            if (pack->quick_item_slot[i] >= ITEM_DUNGEON_START) {
                                used += pack->quick_item_qty[i];
                            }
                        }

                        for (int i = 0; i < max; i++) {
                            if (pack->item[i] >= ITEM_DUNGEON_START) {
                                used++;
                            }
                        }

                        break;
                    }
                    case BOARD_PAGE_WEAPON: {
                        int i;
                        max = 10;
                        int          chara_no = WhoIsWeaponEquip(item);
                        CUserStatus *status = ShopUserStatusPt;
                        WEAPON_HAVE *weapons = status->chara_weapons[chara_no];

                        for (i = 0; i < 10; i++) {
                            if (weapons[i].item_no >= ITEM_WEAPON_START) {
                                used++;
                            }
                        }

                        break;
                    }
                    case BOARD_PAGE_ATTACH: {
                        int i;
                        max = 40;
                        DNG_CONSUMABLE *attachments = ShopUserStatusPt->consumable_items;

                        for (i = 0; i < 40; i++) {
                            if (attachments[i].id >= ITEM_ATTACH_START) {
                                used++;
                            }
                        }

                        break;
                    }
                }

                if (ShopDataMove.item_no > 0) {
                    used++;
                }

                if (used >= max) {
                    full = 1;
                }

                if (full && item >= ITEM_ATTACH_START) {
                    ok = 0;
                    SetItemShopTalkMode(SHOP_TALK_INVENTORY_OVERFLOW, SHOP_MSG_TALKING);
                }

                if (item < ITEM_ATTACH_START) {
                    ok = 0;
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                }

                if (ok) {
                    ShopMenu.talk_mode = SHOP_TALK_CHARGE_TAKE_PROMPT;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_SQUARE)) {
                switch (ShopMenu.board.page) {
                    case BOARD_PAGE_ITEM:
                        ShopStockPt->SeitonItem();
                        break;
                    case BOARD_PAGE_WEAPON:
                        break;
                    case BOARD_PAGE_ATTACH:
                        ShopStockPt->SeitonAttach();
                        break;
                }

                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }

            break;
    }

    return 0;
}

/**
 * Runs the shopkeeper's speech and the model's reaction to it.
 */
static void ShopModelMsgFunc(int shop_kind);

void DrawChargeShop() {
    int text_x;
    int text_y;

    setbilinear(0);
    MenuWorldTrans(&MenuCamera);
    int bright = 0x40;

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_FADE_IN:
            bright = 0x80 - ShopMenu.step_count * 3;
            break;
        case SHOP_TALK_FADE_OUT:
            bright = ShopMenu.step_count * 3 + 0x40;
            break;
    }

    if (bright < 0x40) {
        bright = 0x40;
    }

    if (bright > 0x80) {
        bright = 0x80;
    }

    FrameImageDraw(bright, 0x80);

    if (ShopMenu.ready != 0) {
        int cur_x;
        int top_row;
        int state;
        MenuTextureReload(ShopMenu.tex_block);
        DrawPersonalBoard(0x154, 0x84, ShopMenu.board.page, 0x80, 0);
        CommonTrushDraw(0x232, 0x118, 0x80);
        CommonMoneyBoardDraw(0x163, 0x12C, ShopUserStatusPt->money, 0x80);
        ChargeShopBoardDraw(0x32, 0x84, 0x80);

        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
            ChargeShopLRDraw(0x80);
        }

        switch (ShopMenu.talk_mode) {
            case SHOP_TALK_GOODBYE:
            case SHOP_TALK_LEAVE_PROMPT:
                break;
            default:
                switch (ShopMenu.side) {
                    case SHOP_SIDE_STOCK:
                        cur_x = 0x22;
                        top_row = ShopMenu.stock_top_row;
                        break;
                    case SHOP_SIDE_PERSONAL:
                        cur_x = 0x144;
                        top_row = ShopMenu.board.top_row;
                        break;
                }

                if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                    state = 2;
                } else {
                    switch (ShopMenu.side) {
                        case SHOP_SIDE_PERSONAL:
                            state = SearchBoardNowPosItemExist(ShopMenu.board.page, ShopMenu.board.cursor);

                            if (state <= 0) {
                                state = 0;
                            } else {
                                state = 1;
                            }

                            break;
                        case SHOP_SIDE_STOCK:
                            state = ShopStockPt->SearchSpace(ShopMenu.board.cursor, ShopMenu.board.page);

                            if (state > 0) {
                                state = 1;
                            }

                            break;
                    }
                }

                ShopCurDraw(cur_x, 0x90, ShopMenu.board.cursor, top_row, 0, state, 0x80);
                break;
        }

        DrawShopIcon(0x4C, 0x2A, 1, 0x80);
        ShopDataMove.IconAutoMoveDraw();
        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 0);

        if (ShopMenu.talk_mode == SHOP_TALK_CHARGE_PUT_PROMPT || ShopMenu.talk_mode == SHOP_TALK_CHARGE_TAKE_PROMPT) {
            int prompt[2] = {0x516, 0x517};

            if (CommonMenuMes1.mes_made != prompt[ShopMenu.talk_mode - SHOP_TALK_CHARGE_PUT_PROMPT]) {
                CommonMenuMes1.MakeMesWin(prompt[ShopMenu.talk_mode - SHOP_TALK_CHARGE_PUT_PROMPT]);
            }

            CommonMenuMes1.text_x = 0x14A;
            CommonMenuMes1.text_y = 0xBE;

            if (ShopMenu.talk_mode == SHOP_TALK_CHARGE_PUT_PROMPT) {
                CommonMenuMes1.text_x = 0x96;
            }

            CommonMenuMes1.stay_frame = true;
            CommonMenuMes1.Step();
            CommonMenuMes1.DrawMesWin();
        } else {
            CommonMenuMes1.stay_frame = false;
        }

        MenuHelpWinDraw((int) ShopHelpWinPos[0], (int) ShopHelpWinPos[1], ShopHelpWinW, ShopHelpWinH, 0x80);
        GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
        CommonMenuMes2.text_x = (int) (ShopHelpWinPos[0] + text_x);
        CommonMenuMes2.text_y = (int) (ShopHelpWinPos[1] + text_y);
    }

    if (ShopMenu.person_state == 2) {
        ShopPersonDraw(ShopMenu.shop_no);
    }

    if (ShopMenu.ready != 0) {
        MenuTextureReload(CommonMenuMes2.tex_block);
        CommonMenuMes2.Step();
        CommonMenuMes2.DrawMesWin();

        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
            int mes_no = 0x519;
#ifdef PAL
            u8 plate_x[7][2] = {
                {0xA2, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4},
                {0x78, 0x78},
                {0xAA, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4}
            };
#else
            s16 plate_x[7][2] = {
                {0xA2, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4},
                {0xB4, 0xB4}
            };
#endif
            AtoraNameMes.text_x = plate_x[ShopMenu.lang][0];

            if (ShopMenu.side == SHOP_SIDE_PERSONAL) {
                mes_no = 0x518;
                AtoraNameMes.text_x = plate_x[ShopMenu.lang][1];
            }

            if (AtoraNameMes.mes_made != mes_no) {
                AtoraNameMes.MakeMesWin(mes_no);
            }

            AtoraNameMes.stay_frame = true;
            AtoraNameMes.text_y = SCREEN_HEIGHT - 0x5E;
            AtoraNameMes.Step();
            AtoraNameMes.DrawMesWin();
        }
    }

    ShopModelMsgFunc(SHOP_KIND_CHARGE);
    ShopFadeoutDraw();
    setbilinear(1);
}

/**
 * Draws the charge shop's capacity plate: how many slots the board has and how many hold a good.
 *
 * @mangled ChargeShopMaxDraw__Fiiii
 * @address 0x1EAD50
 * @size 0x1EC
 */
static void ChargeShopMaxDraw(int max, int x, int y, int alpha) {
    int left = x + 0xD2;
    int top = y - 0x28;

    DrawMenu2DSprite(ShopBoard, CRect_i_(left, top + 1, 0x30, 0x2F), CRect_i_(0x100, 0x90, 0x30, 0x30), alpha);
    RECT digits = {0x90, 0xDC, 12, 13};
    left = x + 0xF7;
    top = y - 0xE;
    DrawMenuNumber(max, left, top, ShopBoard, digits, 1, alpha);
    left = x + 0xF2;
    top = y - 0x22;
    int count = 0;
    int i;

    switch (ShopMenu.board.page) {
        case BOARD_PAGE_ITEM:
            for (i = 0; i < 60; i++) {
                if (ShopStockPt->dungeon_items[i] >= ITEM_DUNGEON_START) {
                    count++;
                }
            }

            break;
        case BOARD_PAGE_WEAPON:
            for (i = 0; i < 30; i++) {
                if (ShopStockPt->weapons[i].item_no >= ITEM_WEAPON_START) {
                    count++;
                }
            }

            break;
        case BOARD_PAGE_ATTACH:
            for (i = 0; i < 30; i++) {
                if (ShopStockPt->attachments[i].item_no >= ITEM_ATTACH_START) {
                    count++;
                }
            }

            break;
    }

    DrawMenuNumber(count, left, top, ShopBoard, digits, 1, alpha);
}

void ChargeShopBoardDraw(int x, int y, int alpha) {
    s16 items[120];
    u8  values[120];
    int top = y + 9;
    int bottom = y + 0xA9;
    int board_y;
    int left = x + 0x14;

    board_y = y + 6 - ShopMenu.stock_top_row * 0x28;
    ShopMenu.stock_y += ((float) board_y - ShopMenu.stock_y) / 4.0f;
    board_y = ShopMenu.stock_y;
    DrawPerBoardDraw(0, 0x64, left, board_y, top, bottom, ShopBoard, alpha);

    int board_mode = ShopMenu.board.page;
    int i;

    switch (board_mode) {
        case BOARD_PAGE_WEAPON:
            for (i = 0; i < 30; i++) {
                items[i] = ShopStockPt->weapons[i].item_no;
                values[i] = 0;
            }

            break;
        case BOARD_PAGE_ITEM:
            for (i = 0; i < 60; i++) {
                items[i] = ShopStockPt->dungeon_items[i];
                values[i] = 0;
            }

            break;
        case BOARD_PAGE_ATTACH:
            for (i = 0; i < 30; i++) {
                items[i] = ShopStockPt->attachments[i].item_no;

                if (items[i] >= ITEM_ATTACH_STAT_START && items[i] < ITEM_ATTACH_GEM_START) {
                    values[i] = (&ShopStockPt->attachments[i].item_no)[items[i] - ITEM_ATTACH_STAT_START + 4];
                } else if (items[i] == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                    values[i] = ShopStockPt->attachments[i].sphere_weapon_no;
                } else {
                    values[i] = 0;
                }
            }

            break;
    }

    for (; i < 60; i++) {
        items[i] = -1;
        values[i] = 0;
    }

    ShopIconDraw(items, values, 0x80, left + 4, board_y + 6, top, bottom, 0x80);

    int tag = 0;

    switch (board_mode) {
        case BOARD_PAGE_ITEM:
            tag += 0xC;
            break;
        case BOARD_PAGE_WEAPON:
            tag += 0xA;
            break;
        case BOARD_PAGE_ATTACH:
            tag = 1;
            break;
    }

    PersonalBoardTagDraw(board_mode, x, y, ShopBoard, tag, alpha);
    PersonalBoardDrawWaku(x, y, ShopBoard, alpha);
    PersonalBoardScrlBarDraw(ChargeShopMax[board_mode], x, y, ShopMenu.stock_scroll, ShopMenu.stock_top_row, ShopBoard, alpha);
    ChargeShopMaxDraw(ChargeShopMax[board_mode], x, y, alpha);
}

/**
 * Returns the shop price of one item, which is zero for the item numbers below ITEM_ATTACH_START.
 *
 * @mangled CalItemMoney__Fii
 * @address 0x1EB2D0
 * @size 0x40
 */
static int CalItemMoney(int item_no, int sell) {
    if (item_no < ITEM_ATTACH_START) {
        return 0;
    }

    return GetItemMoney(item_no, sell);
}

/**
 * Totals the shop price of the attachments fitted to a weapon.
 *
 * @mangled WeaponCalMoney__FP11WEAPON_HAVEi
 * @address 0x1EB310
 * @size 0x84
 */
static int WeaponCalMoney(WEAPON_HAVE *weapon, int sell) {
    int total;
    int i;

    if (weapon == NULL) {
        return 0;
    }

    total = 0;

    for (i = 0; i < 6; i++) {
        ATTACH_LIST *attach = &weapon->attach[i];

        if (attach != NULL && attach->item_no >= ITEM_ATTACH_START) {
            total += CalItemMoney(attach->item_no, sell);
        }
    }

    return total;
}

/**
 * Totals the prices of goods currently marked for purchase.
 *
 * @mangled BuyMoneyCheck2__Fv
 * @address 0x1EB3A0
 * @size 0x1A0
 */
static int BuyMoneyCheck2() {
    int        total = 0;
    int        board_max[3] = {100, 60, 40};
    int        i;
    ITEM_PACK *pack = &ShopUserStatusPt->item_pack;

    for (i = 0; i < 100; i++) {
        if (ItemBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
            total += CalItemMoney(pack->item[i], 0);
        }
    }

    for (int chara = 0; chara < 6; chara++) {
        for (i = 0; i < 10; i++) {
            if (WeaponBoardInfo[chara][i] == SHOP_SLOT_SHOP_GOOD) {
                WEAPON_HAVE *weapon = &((WEAPON_HAVE *) ((CUserStatus *) ShopUserStatusPt)->chara_weapons[chara])[i];
                total += CalItemMoney(weapon->item_no, 0);
                total += WeaponCalMoney(weapon, 0);
            }
        }
    }

    DNG_CONSUMABLE *attach = ShopUserStatusPt->consumable_items;

    for (i = 0; i < 40; i++, attach++) {
        if (AttachBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
            total += CalItemMoney(attach->id, 0);
        }
    }

    return total;
}

/**
 * Totals what the goods currently marked for sale fetch.
 *
 * @mangled SellMoneyCheck2__Fv
 * @address 0x1EB540
 * @size 0xB4
 */
static int SellMoneyCheck2() {
    int total = 0;
    int i;

    for (i = 0; i < 30; i++) {
        if (ShopBoardInfo[i] == SHOP_SLOT_PLAYER_ITEM) {
            int item_no = ShopListPt[i].item_no;

            if (item_no >= ITEM_ATTACH_START) {
                int money = CalItemMoney(item_no, 1);

                if (item_no >= ITEM_WEAPON_START) {
                    money += WeaponCalMoney(&ShopListPt[i].data.weapon, 1);
                }

                total += money;
            }
        }
    }

    return total;
}

/**
 * Settles which goods are marked for purchase against what may be afforded.
 *
 * @mangled IncludeBuyItem2__Fv
 * @address 0x1EB600
 * @size 0x1A4
 */
static void IncludeBuyItem2() {
    int          buy = BuyMoneyCheck2();
    int          balance = SellMoneyCheck2() - buy;
    CUserStatus *status;
    int          money;
    int          i;
    int          j;

    for (i = 0; i < 30; i++) {
        if (ShopBoardInfo[i] == SHOP_SLOT_PLAYER_ITEM) {
            ShopBoardInfo[i] = SHOP_SLOT_EMPTY;
            memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
        }
    }

    for (j = 0; j < 100; j++) {
        if (ItemBoardInfo[j] == SHOP_SLOT_SHOP_GOOD) {
            ItemBoardInfo[j] = SHOP_SLOT_PLAYER_ITEM;
        }
    }

    // All six characters' weapon rows, walked as one run of sixty slots.
    for (i = 0; i < 60; i++) {
        if (WeaponBoardInfo[0][i] == SHOP_SLOT_SHOP_GOOD) {
            WeaponBoardInfo[0][i] = SHOP_SLOT_PLAYER_ITEM;
        }
    }

    for (i = 0; i < 40; i++) {
        if (AttachBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
            AttachBoardInfo[i] = SHOP_SLOT_PLAYER_ITEM;
        }
    }

    status = ShopUserStatusPt;
    money = status->money + balance;

    if (money >= 0xFFFF) {
        status->money = 0xFFFF;
    } else {
        status->money = money;
    }

    ItemShopGoodInitialize(ShopMenu.shop_no);
    ItemPosInfoInit();
}

/**
 * Reports the bit flags that prevent the marked item purchase.
 *
 * @mangled CheckBuyItemFunc2__Fv
 * @address 0x1EB7B0
 * @size 0x1C4
 */
static int CheckBuyItemFunc2() {
    int flags = 1;
    int shop_slot;

    for (shop_slot = 0; shop_slot < 30; shop_slot++) {
        if (ShopBoardInfo[shop_slot] == SHOP_SLOT_PLAYER_ITEM) {
            flags |= 2;
            break;
        }
    }

    int        carried = 0;
    ITEM_PACK *pack = ShopUserItemPack(ShopUserStatusPt);
    int        i;

    for (i = 0; i < 3; i++) {
        if (pack->quick_item_slot[i] >= ITEM_DUNGEON_START) {
            carried += pack->quick_item_qty[i];
        }
    }

    for (i = 0; i < pack->num; i++) {
        if (pack->item[i] >= ITEM_DUNGEON_START) {
            carried++;
        }
    }

    int  board_max[3] = {100, 60, 40};
    s32 *board_info[3] = {ItemBoardInfo, WeaponBoardInfo[0], AttachBoardInfo};
    int  board;
    int  slot;

    for (board = 0; board < 3; board++) {
        for (slot = 0; slot < board_max[board]; slot++) {
            if (board_info[board][slot] == SHOP_SLOT_SHOP_GOOD) {
                flags |= 4;
                break;
            }
        }

        if (flags & 4) {
            break;
        }
    }

    if ((flags & 4) && carried > pack->num) {
        flags = 8;
    }

    return flags;
}

/**
 * Leaves the item shop, returning the goods not bought and recording the shop game flag.
 *
 * @mangled ExitItemShop2__Fv
 * @address 0x1EB980
 * @size 0x9C
 */
static void ExitItemShop2() {
    CDngStatusData *dng_status;

    ShopCancelGoodReturn2();
    ShopMenuExit();

    if (SaveData != NULL && ShopMenu.shop_no == 1 && !SaveData->GetGameFlag(0xC8)) {
        dng_status = SaveData->GetDngStatus();

        if (dng_status != NULL && dng_status->SearchItemIndexNo(5) >= 0) {
            SaveData->SetGameFlag(0xC8, 1);
        }
    }
}

/**
 * Drops the shop special good from the stock once the player already carries one.
 *
 * @mangled ShopSpecialFunc__Fv
 * @address 0x1EBA20
 * @size 0x128
 */
static void ShopSpecialFunc() {
    int          found;
    int          i;
    WEAPON_HAVE *weapons;

    if (ShopMenu.shop_no == 1) {
        found = 0;
        weapons = ShopUserStatusPt->chara_weapons[0];

        for (i = 0; i < 10; i++) {
            if (weapons[i].item_no == 5) {
                found = 1;
                break;
            }
        }

        if (ShopDataMove.item_no == 5) {
            found = 1;
        }

        if (found) {
            for (i = 0; i < 30; i++) {
                if (ShopListPt[i].item_no == 5 && ShopBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
                    memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
                    ShopBoardInfo[i] = SHOP_SLOT_EMPTY;
                    return;
                }
            }
        }
    }
}

s32 sort_table[9] = {9, 0, 1, 2, 3, 4, 5, 6, 7};
s32 asort_table[5] = {5, 1, 2, 3, 4};
s32 sort_top_type = ITEM_SORT_FOOD;
s32 asort_top_type = 1;

int CompItem1(int first_item_no, int second_item_no) {
    ITEM_DATA *first = GetItemData(first_item_no);
    ITEM_DATA *second = GetItemData(second_item_no);
    int        first_priority = 0;
    int        second_priority = 0;

    if (first != NULL) {
        first_priority = sort_table[first->sort_key];
    }

    if (second != NULL) {
        second_priority = sort_table[second->sort_key];
    }

    if (first_item_no < ITEM_DUNGEON_START) {
        first_priority = 9;
    }

    if (second_item_no < ITEM_DUNGEON_START) {
        second_priority = 9;
    }

    if (second_priority < first_priority) {
        return 1;
    }

    if (first_priority < second_priority) {
        return -1;
    }

    if (second_item_no < first_item_no) {
        return 1;
    }

    if (first_item_no < second_item_no) {
        return -1;
    }

    return 0;
}

int SeitonShopItemBoardSub(ITEM_PACK *pack) {
    int sort_type = sort_top_type;
    int first_slot;
    int second_slot;
    int moved;

    for (first_slot = 0; first_slot < ITEM_SORT_COUNT; first_slot++) {
        sort_table[sort_type] = first_slot;
        sort_type++;

        if (sort_type >= ITEM_SORT_COUNT) {
            sort_type = ITEM_SORT_NONE;
        }
    }

    sort_table[ITEM_SORT_NONE] = 9;
    moved = false;

    for (first_slot = 0; first_slot < pack->num - 1; first_slot++) {
        for (second_slot = first_slot + 1; second_slot < pack->num; second_slot++) {
            if (CompItem1(pack->item[first_slot], pack->item[second_slot]) > 0) {
                SwapItem(pack, first_slot, second_slot);
                MenuDataSwap(&ItemBoardInfo[first_slot], &ItemBoardInfo[second_slot]);
                moved = true;
            }
        }
    }

    return moved;
}

/**
 * Sorts the item board, trying each ordering until one changes it.
 *
 * @mangled SeitonShopItemBoard__FP9ITEM_PACK
 * @address 0x1EBDB0
 * @size 0x88
 */
static void SeitonShopItemBoard(ITEM_PACK *pack) {
    int i;

    if (pack == NULL) {
        return;
    }

    for (i = 0; i < ITEM_SORT_COUNT; i++) {
        if (SeitonShopItemBoardSub(pack)) {
            return;
        }

        sort_top_type++;

        if (sort_top_type >= ITEM_SORT_COUNT) {
            sort_top_type = ITEM_SORT_FOOD;
        }
    }
}

int CompAttach1(ATTACH_LIST *first, ATTACH_LIST *second) {
    int first_priority = asort_table[GetAttachKind(first->item_no)];
    int second_priority = asort_table[GetAttachKind(second->item_no)];
    int second_item_no;
    int first_item_no;

    first_item_no = first->item_no;

    if (ITEM_ATTACH_START > first_item_no) {
        first_priority = 5;
    }

    second_item_no = second->item_no;

    if (ITEM_ATTACH_START > second_item_no) {
        second_priority = 5;
    }

    if (second_priority < first_priority) {
        return 1;
    }

    if (first_priority < second_priority) {
        return -1;
    }

    if (first_item_no > second_item_no) {
        return 1;
    }

    if (first_item_no < second_item_no) {
        return -1;
    }

    return 0;
}

int SeitonShopAttachBoardSub(ATTACH_LIST *attachments) {
    int sort_type = asort_top_type;
    int first_slot;
    int second_slot;
    int moved;

    for (first_slot = 0; first_slot < 5; first_slot++) {
        asort_table[sort_type] = first_slot;
        sort_type++;

        if (sort_type >= 5) {
            sort_type = 0;
        }
    }

    asort_table[0] = 5;
    moved = false;

    for (first_slot = 0; first_slot < 39; first_slot++) {
        for (second_slot = first_slot + 1; second_slot < 40; second_slot++) {
            if (CompAttach1(&attachments[first_slot], &attachments[second_slot]) > 0) {
                MenuDataSwap(&attachments[first_slot], &attachments[second_slot]);
                MenuDataSwap(&AttachBoardInfo[first_slot], &AttachBoardInfo[second_slot]);
                moved = true;
            }
        }
    }

    return moved;
}

/**
 * Sorts the attachment board, trying each ordering until one changes it.
 *
 * @mangled SeitonShopAttachBoard__FP11ATTACH_LIST
 * @address 0x1EC080
 * @size 0x94
 */
static int SeitonShopAttachBoard(ATTACH_LIST *list) {
    int i;

    if (list == NULL) {
        return 0;
    }

    for (i = 0; i < 5; i++) {
        if (SeitonShopAttachBoardSub(list)) {
            break;
        }

        asort_top_type++;

        if (asort_top_type >= 5) {
            asort_top_type = 0;
        }
    }

    return 1;
}

int ItemShopLoop2() {
    int done;

    ReadBG();
    done = ItemShopKey2();
    ItemShopDraw2();

    if (done) {
        ExitItemShop2();
    }

    return done;
}

int CheckSideKey2() {
    int se;

    if (GamePad.Down(PAD_UP | PAD_LEFT) != 0) {
        ShopMenu.side = SHOP_SIDE_STOCK;
        ShopMenu.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
        ShopMenu.board.cursor = (ShopMenu.stock_top_row + 3) * 5 + 3;
    }

    if (GamePad.Down(PAD_CROSS) != 0) {
        ShopMenu.key_used = 1;

        if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
            se = 2;
        } else {
            int flags = CheckBuyItemFunc2();

            if (flags & 1) {
                se = 2;
                SetItemShopTalkMode(SHOP_TALK_NOTHING_MARKED, SHOP_MSG_TALKING);
            }

            if ((flags & 2) || (flags & 4)) {
                int balance;
                int buy;
                int money = ShopUserStatusPt->money;
                buy = BuyMoneyCheck2();
                int sell = SellMoneyCheck2();
                balance = money + (sell - buy);

                if (balance < 0) {
                    SetItemShopTalkMode(SHOP_TALK_NOT_ENOUGH_MONEY, SHOP_MSG_TALKING);
                    se = 2;
                } else if (balance > 0xFFFF) {
                    SetItemShopTalkMode(SHOP_TALK_TOO_MUCH_MONEY, SHOP_MSG_TALKING);
                    se = 2;
                } else {
                    SetItemShopTalkMode(SHOP_TALK_CONFIRM_DEAL, SHOP_MSG_TALKING);
                    se = 1;
                }
            }

            if (flags & 8) {
                SetItemShopTalkMode(SHOP_TALK_INVENTORY_OVERFLOW, SHOP_MSG_TALKING);
            }
        }

        ComMenuSePlay(se);
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        ShopMenu.side = SHOP_SIDE_PERSONAL;
        ShopMenu.board.cursor = (ShopMenu.board.top_row + 3) * 5;
    }

    return 0;
}

/**
 * Draws the item shop's board with its goods and their prices.
 *
 * @mangled DrawItemShopBoard2__Fiii
 * @address 0x1EC370
 * @size 0x2C4
 */
static void DrawItemShopBoard2(int x, int y, int alpha) {
    int top = y + 9;
    int bottom = y + 0xA9;
    int board_y;
    int left = x + 0x14;

    board_y = y + 6 - ShopMenu.stock_top_row * 0x28;
    ShopMenu.stock_y += ((float) board_y - ShopMenu.stock_y) / 4.0f;
    board_y = ShopMenu.stock_y;
    DrawPerBoardDraw(0, 0x1E, left, board_y, top, bottom, ShopBoard, 0x80);

    int icon_x = left + 2;
    int icon_y = board_y + 6;

    for (int i = 0; i < 0x1E; i++) {
        int item = ShopListPt[i].item_no;
        int num = 0;

        if (item >= ITEM_ATTACH_STAT_START && item < ITEM_ATTACH_GEM_START) {
            num = ShopListPt[i].data.param[item - ITEM_ATTACH_STAT_START + 4];
        }

        if (item == ITEM_ATTACH_SYNTHESIS_SPHERE) {
            num = ShopListPt[i].data.param[1];
        }

        DrawIconParts(item, icon_x, icon_y, top, bottom, alpha, num);
        icon_x += 0x28;

        if (i % 5 == 4) {
            icon_x = left + 2;
            icon_y += 0x28;
        }
    }

    DrawSellTicket_2(left + 2, board_y + 6, top, bottom, 0x80);

    CRect_i_ screen(x, y - 0x13, 0x80, 0x14);
    CRect_i_ texel(0x100, 0x7C, 0x80, 0x14);
    DrawMenu2DSprite(ShopBoard, screen, texel, alpha);
    screen.x += 0x80;
    texel.x += 0x80;
    texel.y = 0x1C;
    DrawMenu2DSprite(ShopBoard, screen, texel, alpha);

    PersonalBoardDrawWaku(x, y, ShopBoard, 0x80);
    PersonalBoardScrlBarDraw(0x1E, x, y, ShopMenu.stock_scroll, ShopMenu.stock_top_row, ShopBoard, 0x80);
    DrawCheckButton(x + 0xA8, y + 0xAA, 0x80);
}

/**
 * Draws the running total the purchase will come to.
 *
 * @mangled DrawMoneyCheckBoard2__Fiii
 * @address 0x1EC640
 * @size 0x18C
 */
static void DrawMoneyCheckBoard2(int x, int y, int alpha) {
    DrawMenu2DSprite(ShopBoard, CRect_i_(x, y + 1, 0x60, 0x1B), CRect_i_(0xD0, 0xC0, 0x60, 0x1C), alpha);
    int  buy = BuyMoneyCheck2();
    int  balance = SellMoneyCheck2() - buy;
    RECT digits = {0, 0xDC, 0xC, 0xC};
    int  u;
    int  v;

    if (balance < 0) {
        digits.y += 0xC;
        u = 0x84;
        v = 0xE8;
    }

    if (balance > 0) {
        digits.y += 0x18;
        u = 0x78;
        v = 0xF4;
    }

    x = (int) (x + 0x54);
    y = (int) (y + 7);
    x = DrawMenuNumber(abs(balance), x, y, ShopBoard, digits, 1, alpha);

    if (balance != 0) {
        DrawMenu2DSprite(ShopBoard, CRect_i_(x - 0xC, y, 0xC, 0xC), CRect_i_(u, v, 0xC, 0xC), alpha);
    }
}

/**
 * Draws the shop's confirmation button.
 *
 * @mangled DrawCheckButton__Fiii
 * @address 0x1EC7D0
 * @size 0x9C
 */
static void DrawCheckButton(int x, int y, int alpha) {
    int u = 0x130;

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_BUTTON_FLASH:
            u = 0x190;

            if (ShopMenu.step_count > 8) {
                ShopMenu.talk_mode = SHOP_TALK_NONE;
            }

            break;
    }

    DrawMenu2DSprite(ShopBoard, CRect_i_(x, y, 0x60, 0x20), CRect_i_(u, 0x90, 0x60, 0x20), alpha);
}

/**
 * Draws the small price ticket, clipped to the digits it has to show.
 *
 * @mangled DrawSmallSellTicket__Fiiiiii
 * @address 0x1EC870
 * @size 0xE0
 */
static void DrawSmallSellTicket(int selected, int x, int y, int clip_top, int clip_bottom, int alpha) {
    int draw_x = x + 0x16;
    int draw_y = y + 0x18;
    int v = 0xB0;
    int height = 0xC;

    if (selected) {
        v = 0xBC;
    }

    if (!(clip_top < draw_y + 0xC && clip_bottom > draw_y)) {
        return;
    }

    MenuTextureClip(draw_y, v, height, clip_top, clip_bottom);
    DrawMenu2DSprite(ShopBoard, CRect_i_(draw_x, draw_y, 0x10, height), CRect_i_(0x130, v, 0x10, height), alpha);
}

/**
 * Draws the large price ticket of the shop slot under the cursor.
 *
 * @mangled DrawBigSellTicket__Fiiiii
 * @address 0x1EC950
 * @size 0xE0
 */
static void DrawBigSellTicket(int selected, int money, int x, int y, int alpha) {
    int  draw_x = x + 0x12;
    int  draw_y = y + 0x14;
    int  u = 0x1A0;
    RECT clip = {0x140, 0xB0, 8, 0xC};

    if (selected) {
        u = 0x1CC;
        clip.y += 0xC;
    }

    DrawMenu2DSprite(ShopBoard, CRect_i_(draw_x, draw_y, 0x2C, 0x20), CRect_i_(u, 0xB0, 0x2C, 0x20), alpha);
    DrawMenuNumber(money, draw_x + 0x24, draw_y + 0xE, ShopBoard, clip, 0, alpha);
}

/**
 * Draws the selling price ticket of one shop slot.
 *
 * @mangled DrawSellTicket_2__Fiiiii
 * @address 0x1ECA30
 * @size 0x184
 */
static void DrawSellTicket_2(int x, int y, int clip_top, int clip_bottom, int alpha) {
    int draw_x = x;
    int draw_y = y;
    int visible;
    int selected;
    int state;
    int i;

    for (i = 0; i < 30; i++) {
        state = ShopBoardInfo[i];

        if (state == SHOP_SLOT_EMPTY) {
            draw_x += 0x28;

            if (i % 5 == 4) {
                draw_x = x;
                draw_y += 0x28;
            }
        } else {
            visible = true;

            if (ShopListPt[i].item_no < ITEM_ATTACH_START) {
                visible = false;
            }

            if (ShopMenu.side == SHOP_SIDE_STOCK && i == ShopMenu.board.cursor) {
                visible = false;
            }

            selected = 0;

            if (state == SHOP_SLOT_PLAYER_ITEM) {
                selected = 1;
            }

            if (visible) {
                DrawSmallSellTicket(selected, draw_x, draw_y, clip_top, clip_bottom, alpha);
            }

            draw_x += 0x28;

            if (i % 5 == 4) {
                draw_x = x;
                draw_y += 0x28;
            }
        }
    }
}

/**
 * Draws the price ticket of one shop slot, small or large according to the price.
 *
 * @mangled DrawLocalTicket__Fiiiiiii
 * @address 0x1ECBC0
 * @size 0x14C
 */
static void DrawLocalTicket(int x, int y, int clip_top, int clip_bottom, int slot, int item_no, int alpha) {
    int money;
    int ticket_x;
    int ticket_y;

    if (slot == ShopMenu.board.cursor && ShopMenu.side == SHOP_SIDE_PERSONAL) {
        money = CalItemMoney(item_no, 0);

        if (money < 0) {
            money = 1;
        }

        ticket_x = x + (slot % 5) * 0x28;
        ticket_y = y + (slot / 5) * 0x28;

        if (ticket_y >= clip_top || ticket_y < clip_bottom) {
            DrawBigSellTicket(0, money, ticket_x, ticket_y, 0x80);
        }
    } else {
        ticket_x = x + (slot % 5) * 0x28;
        ticket_y = y + (slot / 5) * 0x28;
        DrawSmallSellTicket(0, ticket_x, ticket_y, clip_top, clip_bottom, alpha);
    }
}

void DrawSellTicket22(int x, int y, int clip_top, int clip_bottom, int alpha) {
    int             page = ShopMenu.board.page;
    int             board_max[3] = {100, 60, 40};
    int             i;
    int             j;
    int             item_no;
    ITEM_PACK      *pack = &ShopUserStatusPt->item_pack;
    DNG_CONSUMABLE *attach = ShopUserStatusPt->consumable_items;
    WEAPON_HAVE    *weapons;

    switch (page) {
        case BOARD_PAGE_ITEM:
            for (i = 0; i < 100; i++) {
                if (ItemBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
                    item_no = pack->item[i];

                    if (item_no >= ITEM_DUNGEON_START) {
                        DrawLocalTicket(x, y, clip_top, clip_bottom, i, item_no, alpha);
                    }
                }
            }

            break;
        case BOARD_PAGE_WEAPON:
            for (i = 0; i < 6; i++) {
                weapons = (WEAPON_HAVE *) ((char *) ShopUserStatusPt + i * sizeof(ShopUserStatusPt->chara_weapons[0]) + 0x450C);

                for (j = 0; j < 10; j++) {
                    if (WeaponBoardInfo[i][j] == SHOP_SLOT_SHOP_GOOD) {
                        item_no = weapons[j].item_no;

                        if (item_no >= ITEM_WEAPON_START) {
                            DrawLocalTicket(x, y, clip_top, clip_bottom, j + i * 10, item_no, 0x80);
                        }
                    }
                }
            }

            break;
        case BOARD_PAGE_ATTACH:
            for (i = 0; i < 40; i++) {
                if (AttachBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
                    item_no = attach[i].id;

                    if (item_no >= ITEM_ATTACH_START) {
                        DrawLocalTicket(x, y, clip_top, clip_bottom, i, item_no, 0x80);
                    }
                }
            }

            break;
    }
}

/**
 * Puts every marked good back where it came from.
 *
 * @mangled ShopCancelGoodReturn2__Fv
 * @address 0x1ECF90
 * @size 0x4E0
 */
static void ShopCancelGoodReturn2() {
    int          count = -1;
    int          page = BOARD_PAGE_NONE;
    s32         *info;
    ATTACH_LIST *attach = (ATTACH_LIST *) ShopUserStatusPt->consumable_items;
    ITEM_PACK   *pack = ShopUserItemPack(ShopUserStatusPt);
    WEAPON_HAVE *weapon;
    int          space;
    int          chara_no;
    int          slot_no;

    // Gather every marked item, weapon and attachment into the work buffer.
    for (int i = 0; i < 100; i++) {
        if (ItemBoardInfo[i] == SHOP_SLOT_SHOP_GOOD) {
            count++;
            ShopWorkBuf[count].item_no = pack->item[i];
            ShopWorkBuf[count].data.volume = pack->item_vol[i];
            pack->item[i] = 0;
            pack->item_vol[i] = 0;
            ItemBoardInfo[i] = SHOP_SLOT_EMPTY;
        }
    }

    for (int i = 0; i < 60; i++) {
        if (WeaponBoardInfo[0][i] == SHOP_SLOT_SHOP_GOOD) {
            chara_no = i / 10;
            slot_no = i % 10;
            CUserStatus *status = ShopUserStatusPt;
            WEAPON_HAVE *row = status->chara_weapons[chara_no];
            weapon = &row[slot_no];

            if (weapon->item_no >= ITEM_WEAPON_START) {
                count++;
                ShopWorkBuf[count].item_no = weapon->item_no;
                memcpy(&ShopWorkBuf[count].data, weapon, sizeof(WEAPON_HAVE));
                InitHaveWep(weapon);
                WeaponBoardInfo[0][i] = SHOP_SLOT_EMPTY;
            }
        }
    }

    for (int i = 0; i < 40; i++) {
        if (AttachBoardInfo[i] == SHOP_SLOT_SHOP_GOOD && attach != NULL) {
            count++;
            ATTACH_LIST *list = &attach[i];
            ShopWorkBuf[count].item_no = list->item_no;
            memcpy(&ShopWorkBuf[count].data, list, sizeof(ATTACH_LIST));
            InitHaveAttach(list);
            AttachBoardInfo[i] = SHOP_SLOT_EMPTY;
        }
    }

    // Put the goods on the shop board back on the board they came from.
    for (int i = 0; i < 30; i++) {
        if (ShopBoardInfo[i] == SHOP_SLOT_PLAYER_ITEM) {
            int item_no = ShopListPt[i].item_no;

            if (item_no >= ITEM_ATTACH_START) {
                space = GetBoardSpace(item_no, &page);

                if (space >= 0) {
                    switch (page) {
                        case BOARD_PAGE_ITEM:
                            pack->item[space] = item_no;
                            pack->item_vol[space] = ShopListPt[i].data.param[0];
                            info = ItemBoardInfo;
                            break;
                        case BOARD_PAGE_WEAPON: {
                            chara_no = space / 10;
                            slot_no = space % 10;
                            WEAPON_HAVE *row = ((CUserStatus *) ShopUserStatusPt)->chara_weapons[chara_no];
                            weapon = &row[slot_no];
                            memcpy(weapon, &ShopListPt[i].data, sizeof(WEAPON_HAVE));
                            weapon->item_no = item_no;
                            info = WeaponBoardInfo[0];
                            break;
                        }
                        case BOARD_PAGE_ATTACH: {
                            ATTACH_LIST *list = &attach[space];
                            memcpy(list, &ShopListPt[i].data, sizeof(ATTACH_LIST));
                            list->item_no = item_no;
                            info = AttachBoardInfo;
                            break;
                        }
                    }

                    info[space] = SHOP_SLOT_PLAYER_ITEM;
                    memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
                    ShopBoardInfo[i] = SHOP_SLOT_EMPTY;
                }
            }
        }
    }

    // Refill the emptied shop slots from the work buffer.
    int next = 0;

    for (int i = 0; i < 30; i++) {
        if (ShopListPt[i].item_no < ITEM_ATTACH_START) {
            if (next > count) {
                break;
            }

            ShopBoardInfo[i] = SHOP_SLOT_SHOP_GOOD;
            memcpy(&ShopListPt[i], &ShopWorkBuf[next], sizeof(SHOP_ITEMLIST));
            ShopListPt[i].item_no = ShopWorkBuf[next].item_no;
            next++;
        }
    }
}

/**
 * Chooses the line the shopkeeper says for the shop's current state.
 *
 * @mangled GetNowMasterMsgNo2__Fii
 * @address 0x1ED470
 * @size 0x268
 */
static int GetNowMasterMsgNo2(int shop_kind, int shop_no) {
    int mes_no;
    int base = shop_kind * 2000;

    mes_no = base + 2000 + shop_no * 100;

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_GREETING:
            break;
        case SHOP_TALK_CANNOT_CHARGE:
            mes_no += 0x32;
            break;
        case SHOP_TALK_NOT_ENOUGH_MONEY:
            mes_no += 0x34;
            break;
        case SHOP_TALK_TOO_MUCH_MONEY:
            mes_no += 0x38;
            break;
        case SHOP_TALK_NOTHING_MARKED:
            mes_no += 0x33;
            break;
        case SHOP_TALK_CANNOT_SELL:
            mes_no += 0x36;
            break;
        case SHOP_TALK_WEAPON_EQUIPPED:
            mes_no += 0x35;
            break;
        case SHOP_TALK_STOCK_FULL:
            mes_no += 0x3A;
            break;
        case SHOP_TALK_NO_SPACE:
        case SHOP_TALK_INVENTORY_OVERFLOW:
            mes_no += 0x37;
            break;
        case SHOP_TALK_DEAL_PENDING:
            mes_no += 0x41;
            break;
        case SHOP_TALK_CONFIRM_DEAL: {
            int flags = CheckBuyItemFunc2();

            if (flags & 2) {
                mes_no = base + 2070 + shop_no * 100;
            }

            if (flags & 4) {
                mes_no = base + 2071 + shop_no * 100;
            }

            if (flags & 6) {
                mes_no = base + 2072 + shop_no * 100;
            }

            break;
        }
        case SHOP_TALK_DEAL_DONE:
            mes_no += 0x4B;
            break;
        case SHOP_TALK_LEAVE_PROMPT:
            mes_no += 0x5A;
            break;
        case SHOP_TALK_GOODBYE:
        case SHOP_TALK_FADE_OUT:
            mes_no += 0x5B;
            break;
        case SHOP_TALK_NONE:
            switch (ShopMenu.side) {
                case SHOP_SIDE_STOCK:
                    mes_no += 0x14;
                    break;
                case SHOP_SIDE_PERSONAL:
                    if (ShopMenu.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                        mes_no += 0x28;
                    } else {
                        mes_no += 0x1E;
                    }

                    break;
                case SHOP_SIDE_CHECK_BUTTON:
                    mes_no += 0x3C;
                    break;
            }

            break;
    }

    return mes_no;
}

/**
 * Runs the shopkeeper's speech and the model's reaction to it.
 *
 * @mangled ShopModelMsgFunc__Fi
 * @address 0x1ED6E0
 * @size 0x2D8
 */
static void ShopModelMsgFunc(int shop_kind) {
    if (ShopMenu.person_state == 2) {
        int motion = ShopMenu.motion;

        switch (ShopMenu.msg_mode) {
            case SHOP_MSG_IDLE:
                motion = 0;

                if (GamePad.AllOn()) {
                    ShopMenu.idle_count = 0;
                } else {
                    ShopMenu.idle_count++;

                    if (ShopMenu.idle_count >= 0x140) {
                        ShopMenu.msg_mode = SHOP_MSG_TALKING;
                        motion = 3;
                    }
                }

                break;
            case SHOP_MSG_TALKING:
                switch (ShopMenu.talk_mode) {
                    case SHOP_TALK_NONE:
                        if (GamePad.AllOn()) {
                            ShopMenu.msg_mode = SHOP_MSG_IDLE;
                            ShopMenu.idle_count = 0;
                            motion = 0;
                        }

                        break;
                    default:
                        motion = 3;
                        break;
                }

                break;
        }

        if (ShopMenu.msg_mode == SHOP_MSG_TALKING) {
            int msg_no = GetNowMasterMsgNo2(shop_kind, ShopMenu.shop_no);

            if (ShopMenu.msg_no != msg_no || GamePad.Down(PAD_CIRCLE | PAD_CROSS)) {
                ShopMenu.msg_no = msg_no;
                CommonMenuMes3.mes_made = -1;
                CommonMenuMes3.MakeMesWin(ShopMenu.msg_no);
                motion = 3;
            }

            switch (ShopMenu.talk_mode) {
                case SHOP_TALK_FADE_OUT:
                case SHOP_TALK_FADE_IN:
                    break;
                default:
                    if (ShopMenu.person_state) {
                        SetShopTalkMsgPos();
                        CommonMenuMes3.tail_on = true;
                        CommonMenuMes3.auto_pos = MES_POS_BOTTOM;
                        CommonMenuMes3.Step();

                        if (ShopMenu.draw_delay >= 4) {
                            CommonMenuMes3.DrawMesWin();
                        }

                        setbilinear(0);
                    }

                    break;
            }
        }

        switch (CommonMenuMes3.State()) {
            case CLSMES_CLOSED:
            case CLSMES_FADE_OUT:
                motion = 0;
                break;
            case CLSMES_FADE_IN:
            case CLSMES_REVEALING:
                motion = 3;
                break;
        }

        if (ShopMenu.motion != motion) {
            ShopMenu.motion = motion;
            MenuCharaFrame.SetMotion(ShopMenu.motion, 0);
        }
    }
}

/**
 * Places the shopkeeper's speech bubble above their model.
 *
 * @mangled SetShopTalkMsgPos__Fv
 * @address 0x1ED9C0
 * @size 0xB4
 * Places the shopkeeper's message window over the shopkeeper.
 */
static void SetShopTalkMsgPos() {
    float pos[4];
    int   screen[6];
    int   msg_pos[2];
    float scale[3];

    sceVu0CopyVector(pos, MenuCharaFrame.pos);
    MenuCharaFrame.GetScale(scale);
    pos[0] -= 2.0f;
    pos[1] += 0.85f * MenuCharaFrame.body_height;
    pos[3] = 1.0f;
    MGRotTransPers2D(screen, pos, 0);
    msg_pos[0] = screen[0];
    msg_pos[1] = screen[1];
    CommonMenuMes3.AutoSet(msg_pos);
}

void ItemShopGetPacFileName(int shop_kind, int shop_no, char *file_name) {
    char *names[2][18] = {
        {"p13", "p31", "p39", "p54", "p74", "c03", NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL, NULL },
        {"p02", "p32", "p35", "p36", "p36", "p36", "p36", "p37", "p49", "p55", "p75", "c03", "p41", "p02", "p32", "p38",
         "c03",                                                                                                                "c03"}
    };
    char file[32];

    strcpy(file, names[shop_kind][shop_no]);
    strcat(file, "a.pac");
    {
        char path[64] = "commenu/shopchara/";

        strcat(path, file);
        strcpy(file_name, path);
    }
}

void ItemShopGetImgFileName(int shop_kind, int shop_no, char *file_name) {
    char *names[2][18] = {
        {"p13a", "p31a", "p39a", "p54a", "p74a", "c03c", "",     NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL, NULL,   NULL,
         NULL                                                                                                                                        },
        {"p02a", "p32a", "p35a", "p36a", "p36a", "p36a", "p36a", "p37a", "p49a", "p55a", "p75a", "c03c", "p41a", "p02a",
         "p32a",                                                                                                               "p38a", "c03c", "c03c"}
    };

    strcpy(file_name, names[shop_kind][shop_no]);
    strcat(file_name, "01.img");
}

void ItemShopMemoryAlloc() {
    ShopBoardInfo = (s32 *) ShopCashBuffer.Alloc(0x78);
    ItemBoardInfo = (s32 *) ShopCashBuffer.Alloc(0x190);
    WeaponBoardInfo = (s32(*)[10]) ShopCashBuffer.Alloc(0xF0);
    AttachBoardInfo = (s32 *) ShopCashBuffer.Alloc(0xA0);
    memset(ShopBoardInfo, 0, 0x78);
    memset(ItemBoardInfo, 0, 0x190);
    memset(WeaponBoardInfo, 0, 0xF0);
    memset(AttachBoardInfo, 0, 0xA0);
    ShopListPt = (SHOP_ITEMLIST *) ShopCashBuffer.Alloc(0x1D88);
    memset(ShopListPt, 0, 0x1D88);
    ShopWorkBuf = (SHOP_ITEMLIST *) ShopCashBuffer.Alloc(0x1D88);
}

void ItemPosInfoInit() {
    int        i;
    int        j;
    ITEM_PACK *pack = ShopUserItemPack(ShopUserStatusPt);

    for (i = 0; i < 100; i++) {
        if (pack->item[i] >= ITEM_DUNGEON_START) {
            ItemBoardInfo[i] = SHOP_SLOT_PLAYER_ITEM;

#ifdef PAL
            if (DebugMode) {
                printf("%d,  item exist is %d\n", i, pack->item[i]);
            }
#endif
        } else {
            ItemBoardInfo[i] = SHOP_SLOT_EMPTY;
        }
    }

    for (i = 0; i < 6; i++) {
        CUserStatus *status = ShopUserStatusPt;
        WEAPON_HAVE *weapons = status->chara_weapons[i];

        for (j = 0; j < 10; j++) {
#ifdef PAL
            s16 item_no = weapons[j].item_no;

            if (item_no >= ITEM_WEAPON_START) {
#else
            if (weapons[j].item_no >= ITEM_WEAPON_START) {
#endif
                WeaponBoardInfo[i][j] = SHOP_SLOT_PLAYER_ITEM;

#ifdef PAL
                if (DebugMode) {
                    printf("%d,  wep exist is %d\n", i * 10 + j, item_no);
                }
#endif
            } else {
                WeaponBoardInfo[i][j] = SHOP_SLOT_EMPTY;
            }
        }
    }

    DNG_CONSUMABLE *attach = ShopUserStatusPt->consumable_items;

    for (i = 0; i < 40; i++) {
        if (attach[i].id >= ITEM_ATTACH_START) {
            AttachBoardInfo[i] = SHOP_SLOT_PLAYER_ITEM;

#ifdef PAL
            if (DebugMode) {
                printf("%d,  attach exist is %d\n", i, attach[i].id);
            }
#endif
        } else {
            AttachBoardInfo[i] = SHOP_SLOT_EMPTY;
        }
    }
}

/**
 * Fills the goods board from one shop's list.
 *
 * @mangled ItemShopGoodInitialize__Fi
 * @address 0x1EDEA0
 * @size 0x24C
 * Fills the shop stock from the goods list of the shop number it is given.
 */
static void ItemShopGoodInitialize(int shop_no) {
    s16 *list = GetItemShopList(shop_no);

    for (int i = 0; i < 20; i++) {
        s16 item_no = list[i];

        if (item_no >= ITEM_ATTACH_START) {
            switch (WhatIsKindofItem(item_no)) {
                case BOARD_PAGE_WEAPON:
                    WepDataListToHaveCopy(item_no, &ShopListPt[i].data.weapon);
                    break;
                case BOARD_PAGE_ATTACH:
                    AttachDataListToHaveCopy(item_no, &ShopListPt[i].data.attach);

                    if (item_no >= ITEM_ATTACH_STAT_START && item_no < ITEM_ATTACH_GEM_START) {
                        ShopListPt[i].data.param[item_no - ITEM_ATTACH_STAT_START + 4] = 1;
                    }

                    break;
                case BOARD_PAGE_ITEM:
                    ShopListPt[i].data.volume = ItemDataToHaveCopy(item_no);
                    break;
                default:
                    memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
                    break;
            }

            ShopListPt[i].item_no = item_no;
            ShopBoardInfo[i] = SHOP_SLOT_SHOP_GOOD;
        } else {
            memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
            ShopBoardInfo[i] = SHOP_SLOT_EMPTY;
        }
    }

    for (int i = 20; i < 30; i++) {
        ShopBoardInfo[i] = SHOP_SLOT_EMPTY;
        memset(&ShopListPt[i], 0, sizeof(SHOP_ITEMLIST));
    }

    ShopSpecialFunc();
    InitAllHaveData();
}

void InitItemShop2(int *tex_block, int shop_no, int mode) {
    ShopMenuInit(tex_block, shop_no, mode);
    ItemShopMemoryAlloc();
    ItemShopGoodInitialize(shop_no);
    ItemPosInfoInit();
    InitAllHaveData();
    ShopHaveItemPt->slot_state = SHOP_SLOT_EMPTY;
    ShopMenu.board.cursor = 0;
    ShopMenu.side = SHOP_SIDE_STOCK;
    ShopMenu.stock_top_row = 0;
    ShopMenu.stock_scroll = 142.0f + 114.0f * ShopMenu.stock_top_row / 6.0f;
    ShopMenu.stock_y = 0x7E - ShopMenu.stock_top_row * 0x28;
    ShopMenu.cursor_x = 86.0f;
    ShopMenu.cursor_y = 152.0f;
    ShopMenu.cursor_x = (ShopMenu.board.cursor % 5) * 0x28 + 0x154;
    ShopMenu.cursor_y = 120.0f;
    GetMainMenuRightHelpWinLangOffset(ShopHelpWinPos[0], ShopHelpWinPos[1], ShopHelpWinW, ShopHelpWinH);
}

/**
 * Moves the cursor across the item shop's board and swaps or buys the good under it.
 *
 * @mangled ItemShopSelectKey2__Fv
 * @address 0x1EE280
 * @size 0xAF0
 */
static void ItemShopSelectKey2() {
    int           i;
    MENU_ITEMDATA swap_data[2];
    int           swap_item[2];

    if (GamePad.Down(PAD_UP) != 0) {
        ShopMenu.board.cursor -= 5;

        if (ShopMenu.board.cursor < 0) {
            ShopMenu.board.cursor += 5;
        }

        if (ShopMenu.board.cursor / 5 < ShopMenu.stock_top_row) {
            ShopMenu.stock_top_row--;
        }
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        if (ShopMenu.board.cursor >= 25) {
            ShopMenu.side = SHOP_SIDE_CHECK_BUTTON;
        } else {
            ShopMenu.board.cursor += 5;
        }

        if (ShopMenu.stock_top_row + 3 < ShopMenu.board.cursor / 5) {
            ShopMenu.stock_top_row++;
        }
    }

    if (GamePad.Down(PAD_LEFT) != 0 && ShopMenu.board.cursor % 5 != 0) {
        ShopMenu.board.cursor--;
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        if (ShopMenu.board.cursor % 5 == 4) {
            ShopMenu.side = SHOP_SIDE_PERSONAL;
            ShopMenu.board.cursor = (ShopMenu.board.top_row + (ShopMenu.board.cursor / 5 - ShopMenu.stock_top_row)) * 5;
        } else {
            ShopMenu.board.cursor++;
        }
    }

    if (ShopMenu.side == SHOP_SIDE_STOCK && ShopMenu.board.cursor > 30) {
        while (ShopMenu.board.cursor > 30) {
            ShopMenu.board.cursor -= 5;
        }

        ShopMenu.stock_top_row = ShopMenu.board.cursor / 5 - 2;
        ShopMenu.side = SHOP_SIDE_CHECK_BUTTON;
        return;
    }

    if (GamePad.Down(PAD_CROSS) != 0) {
        ShopMenu.key_used = 1;
        int cursor = ShopMenu.board.cursor;
        int shop_info = ShopBoardInfo[cursor];

        if (ShopListPt[cursor].item_no < ITEM_ATTACH_START && ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            return;
        }

        int enable = IsEnableCharge(ShopHaveItemPt->item_no);

        if (enable != 0) {
            int item_no = ShopHaveItemPt->item_no;

            if (item_no >= ITEM_DUNGEON_START) {
                ITEM_DATA *data = GetItemData(item_no);

                if (data != NULL) {
                    if (data->kind_flags & ITEMKINDF_THROWABLE) {
                        enable = false;
                    }

                    if (ShopHaveItemPt->item_no == ITEM_FISHING_ROD) {
                        enable = false;
                    }
                }
            }

            if (ShopHaveItemPt->item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
                enable = false;
            }
        }

        if (ShopMenu.board.held_equipped_slot >= 0) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            SetItemShopTalkMode(SHOP_TALK_WEAPON_EQUIPPED, SHOP_MSG_TALKING);
            return;
        }

        if (enable == 0) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            SetItemShopTalkMode(SHOP_TALK_CANNOT_SELL, SHOP_MSG_TALKING);
            return;
        }

        shop_info = ShopBoardInfo[cursor];
        int have_info = ShopHaveItemPt->slot_state;
        memset(swap_data, 0, sizeof(swap_data));
        SHOP_ITEMLIST *good = &ShopListPt[cursor];
        int            held_kind;
        int            good_kind = WhatIsKindofItem(good->item_no);
        swap_item[0] = good->item_no;

        switch (good_kind) {
            case BOARD_PAGE_ITEM:
                swap_data[0].volume = good->data.volume;
                break;
            case BOARD_PAGE_WEAPON:
                memcpy(&swap_data[0], &good->data, sizeof(WEAPON_HAVE));
                break;
            case BOARD_PAGE_ATTACH:
                memcpy(&swap_data[0], &good->data, sizeof(ATTACH_LIST));
                break;
            default:
                InitShopItemListData(good);
                break;
        }

        held_kind = WhatIsKindofItem(ShopHaveItemPt->item_no);
        swap_item[1] = ShopHaveItemPt->item_no;

        switch (held_kind) {
            case BOARD_PAGE_ITEM:
                swap_data[1].volume = ShopHaveItemPt->volume;
                break;
            case BOARD_PAGE_WEAPON:
                memcpy(&swap_data[1], ShopHaveWepPt, sizeof(WEAPON_HAVE));
                break;
            case BOARD_PAGE_ATTACH:
                memcpy(&swap_data[1], ShopHaveAttachPt, sizeof(ATTACH_LIST));
                break;
        }

        switch (good_kind) {
            case BOARD_PAGE_ITEM:
                ShopHaveItemPt->volume = swap_data[0].volume;
                break;
            case BOARD_PAGE_WEAPON:
                memcpy(ShopHaveWepPt, &swap_data[0], sizeof(WEAPON_HAVE));
                break;
            case BOARD_PAGE_ATTACH:
                memcpy(ShopHaveAttachPt, &swap_data[0], sizeof(ATTACH_LIST));
                break;
        }

        ShopHaveItemPt->item_no = swap_item[0];

        switch (held_kind) {
            case BOARD_PAGE_ITEM:
                good->data.volume = swap_data[1].volume;
                break;
            case BOARD_PAGE_WEAPON:
                memcpy(&good->data, &swap_data[1], sizeof(WEAPON_HAVE));
                break;
            case BOARD_PAGE_ATTACH:
                memcpy(&good->data, &swap_data[1], sizeof(ATTACH_LIST));
                break;
        }

        good->item_no = swap_item[1];
        ShopHaveItemPt->slot_state = shop_info;
        ShopBoardInfo[cursor] = have_info;

        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
            InitAllHaveData();
            ShopMenu.board.held_equipped_slot = -1;
        } else {
            ShopHaveItemPt->last_slot = cursor;
            ShopMenu.board.page = good_kind;
            PersonalBoardLimmitCheck();
        }

        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if (GamePad.Down(PAD_TRIANGLE) != 0) {
        ShopMenu.key_used = 1;

        if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            return;
        }

        if (ShopBoardInfo[ShopMenu.board.cursor] == SHOP_SLOT_SHOP_GOOD) {
            int money = ShopUserStatusPt->money;
            int item_no = ShopListPt[ShopMenu.board.cursor].item_no;
            int price = CalItemMoney(item_no, 0);
            int enable = true;

            if (item_no < ITEM_ATTACH_START) {
                enable = false;
            }

            if (money < price) {
                enable = false;
                SetItemShopTalkMode(SHOP_TALK_NOT_ENOUGH_MONEY, SHOP_MSG_TALKING);
            }

            int page = WhatIsKindofItem(item_no);

            if (GetBoardSpace(item_no, &page) < 0) {
                enable = false;
                SetItemShopTalkMode(SHOP_TALK_NO_SPACE, SHOP_MSG_TALKING);
            }

            int full = 0;
            int max = 0;
            int used = 0;
            int owner = WhoIsWeaponEquip(item_no);

            switch (page) {
                case BOARD_PAGE_ITEM: {
                    int        j;
                    ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                    max = pack->num;

                    for (j = 0; j < 3; j++) {
                        if (pack->quick_item_slot[j] >= ITEM_DUNGEON_START) {
                            used += pack->quick_item_qty[j];
                        }
                    }

                    for (int k = 0; k < max; k++) {
                        if (pack->item[k] >= ITEM_DUNGEON_START) {
                            used++;
                        }
                    }

                    break;
                }
                case BOARD_PAGE_WEAPON: {
                    int j;
                    max = 10;
                    CUserStatus *status = ShopUserStatusPt;
                    WEAPON_HAVE *weapons = status->chara_weapons[owner];

                    for (j = 0; j < 10; j++) {
                        if (weapons[j].item_no >= ITEM_WEAPON_START) {
                            used++;
                        }
                    }

                    break;
                }
                case BOARD_PAGE_ATTACH: {
                    int j;
                    max = 40;
                    DNG_CONSUMABLE *attach = ShopUserStatusPt->consumable_items;

                    for (j = 0; j < 40; j++) {
                        if (attach[j].id >= ITEM_ATTACH_START) {
                            used++;
                        }
                    }

                    break;
                }
            }

            if (ShopDataMove.item_no > 0) {
                used++;
            }

            for (int k = 0; k < 30; k++) {
                if (page >= 0 && page == WhatIsKindofItem(ShopListPt[k].item_no) && ShopBoardInfo[k] == SHOP_SLOT_PLAYER_ITEM) {
                    if (page == BOARD_PAGE_WEAPON) {
                        if (owner == WhoIsWeaponEquip(ShopListPt[k].item_no)) {
                            used++;
                        }
                    } else {
                        used++;
                    }
                }
            }

            if (used >= max) {
                full = 1;
            }

            if (full != 0) {
                enable = false;
                SetItemShopTalkMode(SHOP_TALK_NO_SPACE, SHOP_MSG_TALKING);
            }

            if (enable != 0) {
                ShopMenu.talk_mode = SHOP_TALK_BUY_PROMPT;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            } else {
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }
        } else {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }
    }

    if (BoardModeChangeKey() != 0) {
        PersonalBoardLimmitCheck();
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

#ifdef PAL
    // Debug shortcut: dumps the held item.
    if (DebugMode && GamePad.Down2(PAD_UP)) {
        printf("------nowHaveData---------\n");
        printf("flag   = %d\n", ShopHaveItemPt->slot_state);
        printf("mode = %d\n", ShopHaveItemPt->from_page);
        printf("listno = %d\n", ShopHaveItemPt->item_no);
        printf("select = %d\n", ShopHaveItemPt->from_slot);
        printf("pos = %d\n", ShopHaveItemPt->last_slot);
        printf("listno = %d\n", ShopHaveItemPt->item_no);

        if (0 <= ShopHaveItemPt->item_no && ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
            printf("wepIndex = %d\n", ShopHaveWepPt->item_no);
        }
    }
#endif
}

static inline void ShopSwapHeldGood(SHOP_ITEMLIST *good) {
    MENU_ITEMDATA swap_data[2];

    memset(swap_data, 0, sizeof(swap_data));
    int held_kind;
    int good_kind = WhatIsKindofItem(good->item_no);

    switch (good_kind) {
        case BOARD_PAGE_ITEM:
            swap_data[0].volume = good->data.volume;
            break;
        case BOARD_PAGE_WEAPON:
            memcpy(&swap_data[0], &good->data, sizeof(WEAPON_HAVE));
            break;
        case BOARD_PAGE_ATTACH:
            memcpy(&swap_data[0], &good->data, sizeof(ATTACH_LIST));
            break;
    }

    held_kind = WhatIsKindofItem(ShopHaveItemPt->item_no);

    switch (held_kind) {
        case BOARD_PAGE_ITEM:
            swap_data[1].volume = ShopHaveItemPt->volume;
            break;
        case BOARD_PAGE_WEAPON:
            memcpy(&swap_data[1], ShopHaveWepPt, sizeof(WEAPON_HAVE));
            break;
        case BOARD_PAGE_ATTACH:
            memcpy(&swap_data[1], ShopHaveAttachPt, sizeof(ATTACH_LIST));
            break;
    }

    switch (good_kind) {
        case BOARD_PAGE_ITEM:
            ShopHaveItemPt->volume = swap_data[0].volume;
            break;
        case BOARD_PAGE_WEAPON:
            memcpy(ShopHaveWepPt, &swap_data[0], sizeof(WEAPON_HAVE));
            break;
        case BOARD_PAGE_ATTACH:
            memcpy(ShopHaveAttachPt, &swap_data[0], sizeof(ATTACH_LIST));
            break;
    }

    switch (held_kind) {
        case BOARD_PAGE_ITEM:
            good->data.volume = swap_data[1].volume;
            break;
        case BOARD_PAGE_WEAPON:
            memcpy(&good->data, &swap_data[1], sizeof(WEAPON_HAVE));
            break;
        case BOARD_PAGE_ATTACH:
            memcpy(&good->data, &swap_data[1], sizeof(ATTACH_LIST));
            break;
    }
}

static inline int ShopHeldInfo() {
    return ShopHaveItemPt->slot_state;
}

int ItemShopKey2() {
#ifdef PAL
    // Declared ahead of result so the personal-board cancel path keeps retail's callee-saved registers.
    int  held_info;
    s32 *slot;
#endif
    int           result = 0;
    MENU_ITEMDATA record;

    if (ShopMenu.ready == 0) {
        if (ReadBGSync() == 0) {
            ShopTextureLoadFix();
            ShopPersonReadStart(SHOP_KIND_ITEM, ShopMenu.shop_no);
        }
    } else {
        switch (ShopMenu.talk_mode) {
            case SHOP_TALK_FADE_IN:
                ShopMenu.alpha += 8;

                if (ShopMenu.alpha >= 0x80) {
                    ShopMenu.alpha = 0x80;
                }

                if (ShopMenu.step_count > 20 && ShopMenu.ready != 0 && ShopMenu.alpha >= 0x80) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                }

                break;
            case SHOP_TALK_FADE_OUT:
                ShopMenu.alpha -= 8;

                if (ShopMenu.alpha <= 0) {
                    ShopMenu.alpha = 0;
                }

                if (ShopMenu.step_count > 26 && ShopMenu.alpha <= 0) {
                    result = 1;
                }

                break;
            case SHOP_TALK_SELL_PROMPT:
                if (GamePad.Down(PAD_TRIANGLE | PAD_CROSS) != 0) {
                    s16          cursor = ShopMenu.board.cursor;
                    int          item_no;
                    int          money;
                    s32         *info;
                    WEAPON_HAVE *weapon;

                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM: {
                            ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                            item_no = pack->item[cursor];
                            pack->item[cursor] = -1;
                            pack->item_vol[cursor] = 0;
                            info = ItemBoardInfo;
                            break;
                        }
                        case BOARD_PAGE_ATTACH: {
                            ATTACH_LIST *list = &((ATTACH_LIST *) ShopUserStatusPt->consumable_items)[cursor];
                            item_no = list->item_no;
                            memset(list, 0, sizeof(ATTACH_LIST));
                            info = AttachBoardInfo;
                            break;
                        }
                        case BOARD_PAGE_WEAPON: {
                            CUserStatus *status = ShopUserStatusPt;
                            WEAPON_HAVE *row;
                            weapon = &(row = status->chara_weapons[cursor / 10])[cursor % 10];
                            item_no = weapon->item_no;
                            info = WeaponBoardInfo[0];
                            break;
                        }
                    }

                    money = CalItemMoney(item_no, 1);

                    if (item_no >= ITEM_WEAPON_START) {
                        money += WeaponCalMoney(weapon, 1);
                        memset(weapon, 0, sizeof(WEAPON_HAVE));
                    }

                    CUserStatus *status = ShopUserStatusPt;
                    int          total = status->money + money;

                    if (total >= 0xFFFF) {
                        status->money = 0xFFFF;
                    } else {
                        status->money = total;
                    }

                    info[cursor] = SHOP_SLOT_EMPTY;
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(SE_PURCHASE);
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            case SHOP_TALK_BUY_PROMPT:
                if (CommonMenuMes1.mes_made != 0x4B4) {
                    CommonMenuMes1.MakeMesWin(0x4B4);
                }

                if (GamePad.Down(PAD_TRIANGLE | PAD_CROSS) != 0) {
                    int item_no = ShopListPt[ShopMenu.board.cursor].item_no;
                    ShopDataMoveFinish();
                    int  price = CalItemMoney(item_no, 0);
                    int  page = WhatIsKindofItem(item_no);
                    int  space = GetBoardSpace(item_no, &page);
                    s32 *info;
                    int  icon = space - ShopMenu.board.top_row * 5;

                    switch (page) {
                        case BOARD_PAGE_ITEM:
                            record.volume = ShopListPt[ShopMenu.board.cursor].data.volume;
                            info = ItemBoardInfo;
                            break;
                        case BOARD_PAGE_WEAPON:
                            memcpy(&record, &ShopListPt[ShopMenu.board.cursor].data, sizeof(WEAPON_HAVE));
                            info = WeaponBoardInfo[0];
                            break;
                        case BOARD_PAGE_ATTACH:
                            memcpy(&record, &ShopListPt[ShopMenu.board.cursor].data, sizeof(ATTACH_LIST));
                            info = AttachBoardInfo;
                            break;
                    }

                    ShopMenu.board.page = page;
                    int row = ShopMenu.board.cursor / 5 - ShopMenu.stock_top_row;
                    int x = ShopMenu.board.cursor % 5 * 0x28 + 0x34;
                    int y = row * 0x28 + 0x94;
                    ShopDataMove.IconMoveTarSet(space, icon, item_no, &record, x, y, 0);

                    if (icon < 0 || icon >= 20) {
                        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 1);
                        ShopDataMoveClear();
                    }

                    info[space] = SHOP_SLOT_PLAYER_ITEM;
                    PersonalBoardLimmitCheck();
                    ShopSpecialFunc();
                    CUserStatus *status = ShopUserStatusPt;
                    int          money = status->money;

                    if (money - price >= 0xFFFF) {
                        status->money = 0xFFFF;
                    } else {
                        status->money += -price;
                    }

                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(SE_PURCHASE);
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            case SHOP_TALK_DEAL_PENDING:
            case SHOP_TALK_INVENTORY_OVERFLOW:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CROSS) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    CommonMenuMes3.page_arrow = false;
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    CommonMenuMes3.page_arrow = false;
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ShopCancelGoodReturn2();
                }

                break;
            case SHOP_TALK_CANNOT_SELL:
            case SHOP_TALK_WEAPON_EQUIPPED:
                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    int item_no = ShopHaveItemPt->item_no;
                    int page;
                    int space = GetBoardSpace(item_no, &page);

                    if (space < 0) {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                    } else {
                        CUserStatus *status = ShopUserStatusPt;
                        ITEM_PACK   *pack = &status->item_pack;
                        ATTACH_LIST *attach = (ATTACH_LIST *) status->consumable_items;

                        switch (page) {
                            case BOARD_PAGE_ITEM:
                                pack->item[space] = item_no;
                                pack->item_vol[space] = ShopHaveItemPt->volume;
                                break;
                            case BOARD_PAGE_WEAPON: {
                                int          chara = space / 10;
                                int          slot = space % 10;
                                WEAPON_HAVE *row = status->chara_weapons[chara];
                                WEAPON_HAVE *weapon = &row[slot];
                                memcpy(weapon, ShopHaveWepPt, sizeof(WEAPON_HAVE));
                                weapon->item_no = item_no;

                                if (ShopMenu.board.held_equipped_slot >= 0) {
                                    ShopUserStatusPt->equipped_weapon_slot[chara] = slot;
                                    ShopMenu.board.held_equipped_slot = -1;
                                }

                                break;
                            }
                            case BOARD_PAGE_ATTACH: {
                                ATTACH_LIST *list = &attach[space];
                                memcpy(list, ShopHaveAttachPt, sizeof(ATTACH_LIST));
                                list->item_no = item_no;
                                break;
                            }
                        }

                        InitAllHaveData();
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                    }
                }

                break;
            case SHOP_TALK_GREETING:
                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS | PAD_DPAD) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_NONE;
                    CommonMenuMes3.page_arrow = false;
                }

                break;
            case SHOP_TALK_UNK_9:
            case SHOP_TALK_NO_SPACE:
            case SHOP_TALK_UNK_10:
            case SHOP_TALK_DEAL_DONE:
            case SHOP_TALK_NOT_ENOUGH_MONEY:
            case SHOP_TALK_TOO_MUCH_MONEY:
            case SHOP_TALK_NOTHING_MARKED:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    CommonMenuMes3.page_arrow = false;
                }

                break;
            case SHOP_TALK_GOODBYE:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    ShopMenu.talk_mode = SHOP_TALK_FADE_OUT;
                    ShopMenu.step_count = 0;
                    CommonMenuMes3.page_arrow = false;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            case SHOP_TALK_LEAVE_PROMPT:
                CommonMenuMes3.page_arrow = true;

                if (GamePad.Down(PAD_CROSS) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    CommonMenuMes3.page_arrow = false;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_GOODBYE, SHOP_MSG_TALKING);
                    CommonMenuMes3.page_arrow = false;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                break;
            case SHOP_TALK_CONFIRM_DEAL:
                if (GamePad.Down(PAD_CROSS) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_DEAL_DONE, SHOP_MSG_TALKING);
                    IncludeBuyItem2();
                    ComMenuSePlay(SE_PURCHASE);
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    SetItemShopTalkMode(SHOP_TALK_NONE, SHOP_MSG_IDLE);
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            case SHOP_TALK_NONE: {
                if (ShopMenu.person_state == 0) {
                    ShopMenu.person_state = ShopPersonBuild(SHOP_KIND_ITEM, ShopMenu.shop_no);
                }

                int cursor = ShopMenu.board.cursor;
                int old_page = ShopMenu.board.page;

                switch (ShopMenu.side) {
                    case SHOP_SIDE_CHECK_BUTTON:
                        CheckSideKey2();
                        break;
                    case SHOP_SIDE_STOCK:
                        ItemShopSelectKey2();
                        break;
                    case SHOP_SIDE_PERSONAL: {
                        int board_exit = PersonalBoardKey();

                        if (board_exit == 0 && GamePad.Down(PAD_CROSS) != 0) {
                            int             pos;
                            int             slot_info;
                            PERSONAL_BOARD *board = &ShopMenu.board;
                            int             held_info = ShopHeldInfo();

                            switch (board->cursor_area) {
                                case PERSONAL_BOARD_AREA_TRASH: {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    s16 held = ShopHaveItemPt->item_no;

                                    if (held >= ITEM_ATTACH_START && held_info != SHOP_SLOT_SHOP_GOOD && IsEnableTrushThrow(held) != 0) {
                                        board->trash_anim = 1;
                                        board->trash_frame = 0;
                                        ShopHaveItemPt->item_no = 0;
                                    }

                                    break;
                                }
                                case PERSONAL_BOARD_AREA_CELLS: {
                                    s32 *info;
                                    pos = ShopMenu.board.cursor;

                                    switch (ShopMenu.board.page) {
                                        case BOARD_PAGE_ITEM:
                                            info = ItemBoardInfo;
                                            break;
                                        case BOARD_PAGE_WEAPON:
                                            info = WeaponBoardInfo[0];
                                            break;
                                        case BOARD_PAGE_ATTACH:
                                            info = AttachBoardInfo;
                                            break;
                                    }

                                    slot_info = info[pos];

                                    if (ShopMenu.board.page == BOARD_PAGE_WEAPON && held_info == SHOP_SLOT_SHOP_GOOD && (ShopHaveItemPt->item_no == ITEM_REPAIR_POWDER || ShopHaveItemPt->item_no == ITEM_POWERUP_POWDER)) {
                                        ComMenuSePlay(MENU_SOUND_REFUSE);
                                    } else if (PersonalBoardItemGetorSwap(pos) != 1) {
                                        ComMenuSePlay(MENU_SOUND_REFUSE);
                                    } else {
                                        ShopHaveItemPt->slot_state = slot_info;
                                        info[pos] = held_info;

                                        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
                                            InitAllHaveData();
                                            ShopMenu.board.held_equipped_slot = -1;
                                        } else {
                                            ShopHaveItemPt->last_slot = pos;
                                        }

                                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    }

                                    break;
                                }
                            }
                        } else if (board_exit == 0 && GamePad.Down(PAD_TRIANGLE) != 0) {
                            ShopMenu.key_used = 1;

                            if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                                return 0;
                            }

                            int  target = SearchBoardNowPosItemExist(ShopMenu.board.page, ShopMenu.board.cursor);
                            s32 *slot;

                            switch (ShopMenu.board.page) {
                                case BOARD_PAGE_ITEM:
                                    slot = &ItemBoardInfo[ShopMenu.board.cursor];
                                    break;
                                case BOARD_PAGE_ATTACH:
                                    slot = &AttachBoardInfo[ShopMenu.board.cursor];
                                    break;
                                case BOARD_PAGE_WEAPON:
                                    slot = &WeaponBoardInfo[0][ShopMenu.board.cursor];
                                    break;
                            }

                            if (*slot == SHOP_SLOT_SHOP_GOOD) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                int enable = IsEnableCharge(target);

                                if (enable != 0) {
                                    if (target <= ITEM_WEAPON_START && target >= ITEM_DUNGEON_START) {
                                        ITEM_DATA *data = GetItemData(target);

                                        if (data != NULL && ((data->kind_flags & 0x10) || ShopHaveItemPt->item_no == ITEM_FISHING_ROD)) {
                                            enable = false;
                                            SetItemShopTalkMode(SHOP_TALK_CANNOT_SELL, SHOP_MSG_TALKING);
                                        }
                                    }

                                    if (target >= ITEM_WEAPON_START) {
                                        int chara = ShopMenu.board.cursor / 10;

                                        if (chara == IsDefaultWeapon(target)) {
                                            SetItemShopTalkMode(SHOP_TALK_CANNOT_SELL, SHOP_MSG_TALKING);
                                            enable = false;
                                        }

                                        if (target == 0x10C && GetMenuHebikiriFlag() == 0) {
                                            SetItemShopTalkMode(SHOP_TALK_CANNOT_SELL, SHOP_MSG_TALKING);
                                            enable = false;
                                        }

                                        int          owner = ShopMenu.board.cursor / 10;
                                        CUserStatus *status = ShopUserStatusPt;
                                        s8           slot = status->equipped_weapon_slot[owner];

                                        if (ShopMenu.board.cursor % 10 == slot) {
                                            enable = false;
                                            SetItemShopTalkMode(SHOP_TALK_WEAPON_EQUIPPED, SHOP_MSG_TALKING);
                                        }
                                    }
                                } else {
                                    SetItemShopTalkMode(SHOP_TALK_CANNOT_SELL, SHOP_MSG_TALKING);
                                }

                                int money;
                                int wallet = ShopUserStatusPt->money;
                                money = CalItemMoney(target, 1);

                                if (target >= ITEM_WEAPON_START) {
                                    money += WeaponCalMoney(&ShopWeaponRow(ShopUserStatusPt, ShopMenu.board.cursor / 10)[ShopMenu.board.cursor % 10], 1);
                                }

                                if (money <= 0) {
                                    money = 1;
                                }

                                if (wallet + money > 0xFFFF) {
                                    SetItemShopTalkMode(SHOP_TALK_TOO_MUCH_MONEY, SHOP_MSG_TALKING);
                                    enable = false;
                                }

                                if (target < ITEM_ATTACH_START) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else if (enable != 0) {
                                    ShopMenu.talk_mode = SHOP_TALK_SELL_PROMPT;
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            }
                        }

                        if (board_exit != 0) {
                            ShopMenu.side = SHOP_SIDE_STOCK;
                            ShopMenu.board.cursor = (ShopMenu.stock_top_row + (ShopMenu.board.cursor / 5 - ShopMenu.board.top_row)) * 5 + 4;
                        }

                        break;
                    }
                }

                if (ShopMenu.key_used == 0 && GamePad.Down(PAD_CIRCLE) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    s16 held = ShopHaveItemPt->item_no;

                    if (held < ITEM_ATTACH_START) {
                        if (ShopMenu.side == SHOP_SIDE_CHECK_BUTTON) {
                            CommonMenuMes3.mes_made = -1;
                            ShopMenu.msg_mode = SHOP_MSG_TALKING;
                            int flags = CheckBuyItemFunc2();

                            if (flags & 1) {
                                ShopMenu.talk_mode = SHOP_TALK_LEAVE_PROMPT;
                            }

                            if (flags & 6) {
                                ShopMenu.talk_mode = SHOP_TALK_DEAL_PENDING;
                                ShopMenu.side = SHOP_SIDE_CHECK_BUTTON;
                            }

                            if (flags & 8) {
                                ShopMenu.talk_mode = SHOP_TALK_INVENTORY_OVERFLOW;
                            }
                        } else {
                            ShopMenu.side = SHOP_SIDE_CHECK_BUTTON;
                        }
                    } else {
#ifdef PAL
                        held_info = ShopHaveItemPt->slot_state;
#else
                        int held_info = ShopHaveItemPt->slot_state;
#endif
                        s32 *info;

                        switch (held_info) {
                            case SHOP_SLOT_SHOP_GOOD: {
                                int free = -1;

                                for (int i = 0; i < 30; i++) {
                                    if (ShopBoardInfo[i] == SHOP_SLOT_EMPTY) {
                                        free = i;
                                        break;
                                    }
                                }

                                if (free < 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    SHOP_ITEMLIST *good = &ShopListPt[free];
                                    ShopSwapHeldGood(good);
                                    MenuDataSwap(&ShopHaveItemPt->slot_state, &ShopBoardInfo[free]);
                                    MenuDataSwap(&ShopHaveItemPt->item_no, &good->item_no);
                                }

                                break;
                            }
                            case SHOP_SLOT_PLAYER_ITEM: {
                                int page = WhatIsKindofItem(held);

                                switch (page) {
                                    case BOARD_PAGE_ITEM:
                                        info = ItemBoardInfo;
                                        break;
                                    case BOARD_PAGE_WEAPON:
                                        info = WeaponBoardInfo[0];
                                        break;
                                    case BOARD_PAGE_ATTACH:
                                        info = AttachBoardInfo;
                                        break;
                                }

#ifndef PAL
                                s32 *slot;
#endif
                                int space = GetBoardSpace(held, &page);
                                int max = PersonalRetMax(page);

                                if (space < 0 || max <= space) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    slot = &info[space];
                                    int slot_info = *slot;
                                    ShopHaveItemPt->from_page = page;
                                    ShopHaveItemPt->from_slot = space;
                                    int equipped = ShopMenu.board.held_equipped_slot;
                                    int item_no = ShopHaveItemPt->item_no;
                                    PersonalBoardItemCancel();

                                    if (0 <= equipped) {
                                        int chara = WhoIsWeaponEquip(item_no);
                                        ShopUserStatusPt->equipped_weapon_slot[chara] = space % 10;
                                    }

                                    ShopHaveItemPt->slot_state = slot_info;
                                    *slot = held_info;
                                }

                                break;
                            }
                        }
                    }
                } else if (GamePad.Down(PAD_SQUARE) != 0) {
                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM: {
                            ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                            SeitonShopItemBoard(pack);
                            SetMenuTrushMark(pack);
                            break;
                        }
                        case BOARD_PAGE_ATTACH:
                            SeitonShopAttachBoard((ATTACH_LIST *) ShopUserStatusPt->consumable_items);
                            break;
                    }

                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

#ifdef PAL
                // Debug shortcuts: add or take Gilda, and dump the shop board.
                if (GamePad.On2(PAD_TRIANGLE)) {
                    CUserStatus *status = ShopUserStatusPt;
                    int          total = status->money + 1000;

                    if (total >= 0xFFFF) {
                        status->money = 0xFFFF;
                    } else {
                        status->money = total;
                    }
                }

                if (GamePad.On2(PAD_CROSS)) {
                    CUserStatus *status = ShopUserStatusPt;
                    int          money = status->money;

                    if (money - 1000 >= 0xFFFF) {
                        status->money = 0xFFFF;
                    } else {
                        status->money += -1000;
                    }
                }

                if (GamePad.Down2(PAD_R1)) {
                    for (int i = 0; i < 30; i++) {
                        printf(" %d \tis \t\t%d\n", i, ShopBoardInfo[i]);
                    }
                }

                if (GamePad.Down2(PAD_R2)) {
                    int capacity[3] = {100, 60, 40};
                    int max;

                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM:
                            max = capacity[0];
                            break;
                        case BOARD_PAGE_WEAPON:
                            max = capacity[1];
                            break;
                        case BOARD_PAGE_ATTACH:
                            max = capacity[2];
                            break;
                    }
                }

#endif
                if (cursor != ShopMenu.board.cursor || old_page != ShopMenu.board.page) {
                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }

                int mes_no = 0;
                int value = 0;
                int name_mes = -1;

                if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
                    COM_ITEM_INFO *item_info = GetCommonItemInfo(ShopHaveItemPt->item_no);
                    mes_no = item_info != NULL ? item_info->msg + 500 : mes_no;
                    value = GetAttachVolumeForMsg(ShopHaveAttachPt);

                    if (ShopHaveItemPt->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                        name_mes = GetWeaponMsgNo2(ShopHaveAttachPt->sphere_weapon_no);
                    }
                }

                int          item_no = -1;
                int          pos = ShopMenu.board.cursor;
                ATTACH_LIST *list;

                switch (ShopMenu.side) {
                    case SHOP_SIDE_CHECK_BUTTON:
                        mes_no = 0x4B3;
                        break;
                    case SHOP_SIDE_STOCK:
                        item_no = ShopListPt[pos].item_no;

                        if (item_no >= ITEM_WEAPON_START) {
                            value = ShopListPt[pos].data.weapon.level;
                        }

                        if (item_no >= ITEM_ATTACH_STAT_START && item_no < ITEM_ATTACH_GEM_START) {
                            list = &ShopListPt[pos].data.attach;
                            value = GetAttachVolumeForMsg(list);
                        }

                        if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                            name_mes = GetWeaponMsgNo2(ShopListPt[pos].data.attach.sphere_weapon_no);
                            value = ShopListPt[pos].data.attach.sphere_level;
                        }

                        break;
                    case SHOP_SIDE_PERSONAL:
                        switch (ShopMenu.board.page) {
                            case BOARD_PAGE_ITEM: {
                                ITEM_PACK *pack = &ShopUserStatusPt->item_pack;
                                item_no = pack->item[pos];
                                break;
                            }
                            case BOARD_PAGE_WEAPON: {
                                CUserStatus *status = ShopUserStatusPt;
                                WEAPON_HAVE *row;
                                WEAPON_HAVE *weapon = &(row = status->chara_weapons[pos / 10])[pos % 10];
                                item_no = weapon->item_no;
                                value = weapon->level;
                                break;
                            }
                            case BOARD_PAGE_ATTACH: {
                                list = &((ATTACH_LIST *) ShopUserStatusPt->consumable_items)[pos];

                                if (list != NULL) {
                                    item_no = list->item_no;

                                    if (item_no > 0) {
                                        value = GetAttachVolumeForMsg(list);

                                        if (list->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                                            name_mes = GetWeaponMsgNo2(list->sphere_weapon_no);
                                        }
                                    }
                                }

                                break;
                            }
                        }

                        break;
                }

                if (item_no >= ITEM_ATTACH_START) {
                    COM_ITEM_INFO *item_info = GetCommonItemInfo(item_no);

                    if (item_info != NULL) {
                        mes_no = item_info->msg + 500;
                    }
                }

                if (CommonMenuMes2.mes_made != mes_no || (name_mes > 0 && CommonMenuMes2.mes_no[0] != name_mes + 100) || CommonMenuMes2.value != value) {
                    CommonMenuMes2.value_signed = true;
                    CommonMenuMes2.value_show = false;
                    CommonMenuMes2.value = value;

                    if (name_mes > 0) {
                        CommonMenuMes2.mes_no[0] = name_mes + 100;
                    }

                    if (mes_no <= 500) {
                        mes_no = 0;
                    }

                    CommonMenuMes2.mes_made = -1;
                    CommonMenuMes2.MakeMesWin(mes_no);
                }

                break;
            }
        }
    }

    return result;
}

void ItemShopDraw2() {
    int cur_x;
    int pos_y;
    int count;
    int top_row;
    int state;
    int text_x;
    int text_y;

    setbilinear(0);
    MenuWorldTrans(&MenuCamera);
    int bright = 0x40;

    switch (ShopMenu.talk_mode) {
        case SHOP_TALK_FADE_IN:
            bright = 0x80 - ShopMenu.step_count * 3;
            break;
        case SHOP_TALK_FADE_OUT:
            bright = ShopMenu.step_count * 4 + 0x40;
            break;
    }

    if (bright < 0x40) {
        bright = 0x40;
    }

    if (bright > 0x80) {
        bright = 0x80;
    }

    FrameImageDraw(bright, 0x80);

    if (ShopMenu.ready != 0) {
        MenuTextureReload(ShopMenu.tex_block);
        count = PersonalRetMax(ShopMenu.board.page);
        pos_y = 0x7F - ShopMenu.board.top_row * 0x28;
        ShopMenu.board.y += ((float) pos_y - ShopMenu.board.y) / 4.0f;
        cur_x = 0x168;
        pos_y = (int) ShopMenu.board.y;
        int mark = 0;

        if (ShopMenu.board.page == BOARD_PAGE_WEAPON) {
            mark = 2;
        }

        DrawPerBoardDraw(mark, count, 0x168, pos_y, 0x81, 0x121, PerBoardTex, 0x80);
        CommonIconDraw(ShopMenu.board.page, count, 0x16A, pos_y + 6, 0x81, 0x121, 0x80);
        PersonalBoardOptionDraw(ShopMenu.board.page, count, 0x154, 0x78, PerBoardTex, 0x80);
        CommonTrushDraw(0x232, 0x10C, 0x80);
        CommonMoneyBoardDraw(0x163, 0x120, ShopUserStatusPt->money, 0x80);
        DrawMoneyCheckBoard2(0x1C8, 0x120, 0x80);
        DrawItemShopBoard2(0x32, 0x78, 0x80);
        ChargeShopLRDraw(0x80);
        ShopDataMove.IconAutoMoveDraw();
        ShopDataMove.IconAutoMove(ChargeOrShopFlag, 0);

        switch (ShopMenu.side) {
            case SHOP_SIDE_CHECK_BUTTON:
            case SHOP_SIDE_STOCK:
                cur_x = 0x22;
                top_row = ShopMenu.stock_top_row;
                break;
            case SHOP_SIDE_PERSONAL:
                cur_x = 0x144;
                top_row = ShopMenu.board.top_row;
                break;
        }

        state = 0;

        if (ShopHaveItemPt->item_no >= ITEM_ATTACH_START) {
            state = 2;
        } else {
            switch (ShopMenu.side) {
                case SHOP_SIDE_PERSONAL:
                    state = SearchBoardNowPosItemExist(ShopMenu.board.page, ShopMenu.board.cursor);

                    if (state < ITEM_ATTACH_START) {
                        state = 0;
                    } else {
                        state = 1;
                    }

                    break;
                case SHOP_SIDE_STOCK:
                    if (ShopListPt[ShopMenu.board.cursor].item_no > ITEM_ATTACH_START) {
                        state = 1;
                    } else {
                        state = 0;
                    }

                    break;
                case SHOP_SIDE_CHECK_BUTTON:
                    break;
            }
        }

        int on_button = 0;

        switch (ShopMenu.side) {
            case SHOP_SIDE_CHECK_BUTTON:
                on_button = 1;
                break;
        }

        ShopCurDraw(cur_x, 0x84, ShopMenu.board.cursor, top_row, on_button, state, 0x80);
        DrawShopIcon(0x4C, 0x2A, 0, 0x80);

        switch (ShopMenu.side) {
            case SHOP_SIDE_STOCK: {
                int cursor = ShopMenu.board.cursor;
                int item_no = ShopListPt[cursor].item_no;

                if (item_no >= ITEM_ATTACH_START) {
                    int selling = 0;

                    if (ShopBoardInfo[cursor] == SHOP_SLOT_PLAYER_ITEM) {
                        selling = 1;
                    }

                    int money = CalItemMoney(item_no, selling);

                    if (item_no >= ITEM_WEAPON_START) {
                        money += WeaponCalMoney((WEAPON_HAVE *) &ShopListPt[cursor].data, selling);
                    }

                    if (money < 0) {
                        money = 1;
                    }

                    cur_x = cursor % 5 * 0x28 + 0x4A;
                    pos_y = (int) (6.0f + ShopMenu.stock_y + (float) (cursor / 5 * 0x28));
                    DrawBigSellTicket(selling, money, cur_x, pos_y, 0x80);
                }

                break;
            }
            case SHOP_SIDE_PERSONAL:
                if (ShopMenu.talk_mode == SHOP_TALK_SELL_PROMPT) {
                    int item_no;

                    switch (ShopMenu.board.page) {
                        case BOARD_PAGE_ITEM: {
                            CUserStatus *status = ShopUserStatusPt;
                            ITEM_PACK   *pack = &status->item_pack;
                            item_no = pack->item[ShopMenu.board.cursor];
                            break;
                        }
                        case BOARD_PAGE_ATTACH: {
                            CUserStatus    *status = ShopUserStatusPt;
                            DNG_CONSUMABLE *attach = status->consumable_items;
                            item_no = attach[ShopMenu.board.cursor].id;
                            break;
                        }
                        case BOARD_PAGE_WEAPON: {
                            int          chara_no = ShopMenu.board.cursor / 10;
                            CUserStatus *status = ShopUserStatusPt;
                            WEAPON_HAVE *row = status->chara_weapons[chara_no];
                            item_no = row[ShopMenu.board.cursor % 10].item_no;
                            break;
                        }
                    }

                    int ticket_y = (int) (6.0f + ShopMenu.board.y);
                    int money = CalItemMoney(item_no, 1);

                    if (item_no >= ITEM_WEAPON_START) {
                        int          chara_no = ShopMenu.board.cursor / 10;
                        CUserStatus *status = ShopUserStatusPt;
                        WEAPON_HAVE *row = status->chara_weapons[chara_no];
                        money += WeaponCalMoney(&row[ShopMenu.board.cursor % 10], 1);
                    }

                    if (money <= 0) {
                        money = 1;
                    }

                    DrawBigSellTicket(1, money, ShopMenu.board.cursor % 5 * 0x28 + 0x16A, ticket_y + ShopMenu.board.cursor / 5 * 0x28, 0x80);
                }

                break;
        }

        DrawSellTicket22(0x16A, (int) (6.0f + ShopMenu.board.y), 0x81, 0x121, 0x80);
        MenuHelpWinDraw((int) ShopHelpWinPos[0], (int) ShopHelpWinPos[1], ShopHelpWinW, ShopHelpWinH, 0x80);
        GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
        CommonMenuMes2.text_x = (int) (ShopHelpWinPos[0] + text_x);
        CommonMenuMes2.text_y = (int) (ShopHelpWinPos[1] + text_y);
    }

    if (ShopMenu.person_state == 2) {
        ShopPersonDraw(ShopMenu.shop_no);
    }

    if (ShopMenu.ready != 0) {
        MenuTextureReload(CommonMenuMes2.tex_block);
        CommonMenuMes2.Step();
        CommonMenuMes2.DrawMesWin();

        if (ShopHaveItemPt->item_no < ITEM_ATTACH_START) {
#ifdef PAL
            s8  plate[3] = {6, 7, 8};
            int mes_no = plate[ShopMenu.side] + 0x4B0;

#else
            int plate[3] = {0x4B6, 0x4B7, 0x4B8};
            int mes_no = plate[ShopMenu.side];

#endif
            if (AtoraNameMes.mes_made != mes_no) {
                AtoraNameMes.MakeMesWin(mes_no);
            }

            AtoraNameMes.stay_frame = true;
            AtoraNameMes.text_x = 0xB4;
            AtoraNameMes.text_y = SCREEN_HEIGHT - 0x58;

#ifdef PAL
            // The third plate draws further left in language 6.
            if (GetMenuLangFlag() == LANG_SPANISH && mes_no == 0x4B8) {
                AtoraNameMes.text_x -= 0x12;
            }

#endif
            AtoraNameMes.Step();
            AtoraNameMes.DrawMesWin();
        }

        switch (ShopMenu.talk_mode) {
            case SHOP_TALK_BUY_PROMPT:
            case SHOP_TALK_SELL_PROMPT: {
                s16 prompt[2] = {0x4B4, 0x4B5};
                int mes_no = prompt[ShopMenu.talk_mode - SHOP_TALK_BUY_PROMPT];

                if (CommonMenuMes1.mes_made != mes_no) {
                    CommonMenuMes1.MakeMesWin(mes_no);
                }

                s8 offset[7][2] = {
                    {0x1E, 0x32},
                    {0x18, 0x32},
                    {0x18, 0x32},
                    {0x18, 0x32},
                    {0x18, 0x32},
                    {0x18, 0x32},
                    {0x18, 0x32}
                };
                CommonMenuMes1.text_x = offset[ShopMenu.lang][0] + 0x12C;
                CommonMenuMes1.text_y = 0xBE;

                if (ShopMenu.talk_mode == SHOP_TALK_SELL_PROMPT) {
                    CommonMenuMes1.text_x = offset[ShopMenu.lang][1] + 0x64;
                }

                CommonMenuMes1.stay_frame = true;
                CommonMenuMes1.Step();
                CommonMenuMes1.DrawMesWin();
                break;
            }
            default:
                CommonMenuMes1.stay_frame = false;
                break;
        }
    }

    ShopModelMsgFunc(SHOP_KIND_ITEM);
    ShopFadeoutDraw();
    setbilinear(1);
}

/**
 * Returns one prize the fishing exchange offers.
 *
 * @mangled GetExchangeItemList__Fi
 * @address 0x1F1250
 * @size 0x18
 */
FISH_EXCHANGE_ITEM exitemlst[35] = {
    {ITEM_ANTIDOTE_DRINK,        5   },
    {ITEM_HOLY_WATER,            5   },
    {ITEM_SOAP,                  5   },
    {ITEM_MIGHTY_HEALING,        15  },
    {ITEM_ATTACH_DINOSLAYER,     50  },
    {ITEM_ATTACH_UNDEAD_BUSTER,  50  },
    {ITEM_ATTACH_SEA_KILLER,     50  },
    {ITEM_ATTACH_STONE_BREAKER,  50  },
    {ITEM_ATTACH_PLANT_BUSTER,   50  },
    {ITEM_ATTACH_BEAST_BUSTER,   50  },
    {ITEM_ATTACH_SKY_HUNTER,     50  },
    {ITEM_ATTACH_METALBREAKER,   50  },
    {ITEM_ATTACH_MIMIC_BREAKER,  50  },
    {ITEM_ATTACH_MAGE_SLAYER,    50  },
    {ITEM_ATTACH_GARNET,         100 },
    {ITEM_ATTACH_AMETHYST,       100 },
    {ITEM_ATTACH_AQUAMARINE,     100 },
    {ITEM_ATTACH_DIAMOND,        100 },
    {ITEM_ATTACH_EMERALD,        100 },
    {ITEM_ATTACH_PEARL,          100 },
    {ITEM_ATTACH_RUBY,           100 },
    {ITEM_ATTACH_PERIDOT,        100 },
    {ITEM_ATTACH_SAPPHIRE,       100 },
    {ITEM_ATTACH_OPAL,           100 },
    {ITEM_ATTACH_TOPAZ,          100 },
    {ITEM_ATTACH_TURQUOISE,      100 },
    {ITEM_WEAPON_TSUKIKAGE,      1100},
    {ITEM_WEAPON_MATADOR,        1400},
    {ITEM_WEAPON_MAGICAL_HAMMER, 1800},
    {ITEM_WEAPON_FAIRY_S_RING,   1300},
    {ITEM_WEAPON_SCORPION,       1400},
    {ITEM_WEAPON_BLESSING_GUN,   1500},
    {ITEM_WEAPON_MARDAN_EINS,    2500},
    {ITEM_BOMB_NUTS,             2   },
    {ITEM_POISONOUS_APPLE,       2   },
};

static FISH_EXCHANGE_ITEM *GetExchangeItemList(int index) {
    return &exitemlst[index];
}

int GetMardanGareyanFlag() {
    return SaveData->GetMardanGareyanFlag();
}

void SetAlreadyGetMardanWeapon(int taken) {
    SaveData->SetGameFlag(0xCA, taken);
}

void SetFishMardanGarayanNum(int count) {
    int total = count + GetFishMardanGarayanNum();

    if (total < 0) {
        total = 0;
    }

    if (total > 10000) {
        total = 10000;
    }

    SaveData->SetGameIntFlag(0x14, total);
    printf("now mardan num(IntFlag):\t%d\n", GetFishMardanGarayanNum());
}

int GetFishMardanGarayanNum() {
    return SaveData->GetGameIntFlag(0x14);
}

void ClearFishMardanGarayanNum() {
    SaveData->SetGameIntFlag(0x14, 0);
}

/**
 * Reports whether the Mardan Garayan weapon has been handed over: zero once game flag 0xCA
 * marks it taken, one or two otherwise according to the Mardan Garayan flag.
 *
 * @mangled AlreadyGetMardanWeapon__Fv
 * @address 0x1F13A0
 * @size 0x70
 */
static int AlreadyGetMardanWeapon() {
    int flag = SaveData->GetGameFlag(0xCA);

    if (flag == 0) {
        if (GetMardanGareyanFlag() > 0) {
            return 1;
        }

        return 2;
    }

    if (flag == 1) {
        return 0;
    }

    return flag;
}

void InitFishingExchange(u_long128 *buffer, int *tex_block, int mode) {
    FishMenu.buffer = (u_long128 *) (EdMenuBuffer.base + EdMenuBuffer.used * 16);
    FishMenu.buffer = MenuCalcBufAlignment(FishMenu.buffer);
    FishMenu.tex_block = tex_block[0];
    FishMenu.tex_block2 = tex_block[1];
    FishMenu.mode = mode;
    StartReadBG();
    FishMenu.buffer = FishMenu.buffer + (LoadFileBGMenuData("fishmenu.pac", FishMenu.buffer) >> 4) + 1;
    FishMenu.buffer = MenuCalcBufAlignment(FishMenu.buffer);
    FishMenu.ready = 0;
    FishMenu.confirm = 1;
    FishMenu.cursor = 0;
    FishMenu.top = 0;
    FishMenu.fade_mode = FISH_EXCHANGE_FADE_IN;
    FishMenu.fade_count = 0;
    FishMenu.point = SaveData->GetFishingPoint();

    if (FishMenu.point < 0) {
        SaveData->SetFishingPoint(0);
        FishMenu.point = SaveData->GetFishingPoint();
    }

    if (FishMenu.point > 9999) {
        FishMenu.point = 9999;
    }

    StayTex = TexManager.GetTexture("stayframe", -1);
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);
}

char FishFrameImage[] __attribute__((section(".rodata"))) = "#frame_image#640#" SCREEN_HEIGHT_STR "#4";

/**
 * Enters the fishing menu's textures once they have been read.
 *
 * @mangled FishMenuTextureLoad__Fv
 * @address 0x1F15D0
 * @size 0x2AC
 */
static int FishMenuTextureLoad() {
    int done = 0;

    if (FishMenu.ready == 0) {
        if (ReadBGSync() == 0) {
            BG_READ_INFO     *file = GetReadBGFile(0);
            LOADTEXTURE_INFO2 texture[3] = {
                {FishFrameImage, 0, 0},
                {NULL,           0, 0},
                {NULL,           0, 0},
            };
            texture[0].block_no = FishMenu.tex_block;
            texture[1].block_no = FishMenu.tex_block;
            texture[1].name = (char *) GetPackFile((u_int *) file->buffer, "fishing.img", NULL);
            TexManager.DeleteTextureBlock(FishMenu.tex_block);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture);
            FishMenuTex = TexManager.GetTexture("fishbrd", -1);
            WepIcon = TexManager.GetTexture("wepicon", -1);
            ItemIcon = TexManager.GetTexture("itemicon", -1);
            InitMenuMesSet(MENU_MES_SET_ALLMENU, (short *) GetPackFile((u_int *) file->buffer, FishMessageFile, NULL));
            CommonMenuMes2.mes_made = -1;
            AtoraNameMes.Preset(MES_PRESET_SYSTEM);

            for (int i = 0; i < 5; i++) {
                AtoraNameMes.mes_no[i] = GetExchangeItemList(i)->item_no + 100;
            }

            AtoraNameMes.narrow_gaiji = true;
            AtoraNameMes.style = MES_EDGE_DOUBLE;
            AtoraNameMes.value_signed = false;
            AtoraNameMes.value_show = true;
            AtoraNameMes.mes_made = -1;
            AtoraNameMes.MakeMesWin(0xC8);
            AtoraNameMes.Step();
            FishMenu.ready = 1;
            CommonMenuMes3.value_signed = false;
            CommonMenuMes3.value_show = true;
            CommonMenuMes3.stay_frame = true;
            CommonMenuMes3.value = 0;
            int digits = GetNumberKeta(0);
            CommonMenuMes3.mes_made = -1;
            CommonMenuMes3.MakeMesWin(digits + 0xCD);
            FishMenu.cursor_y = 0x7E;
            done = 1;
        }
    } else {
        done = 1;
    }

    return done;
}

char FishMessageFile[] __attribute__((section(".rodata"))) = "fishmes.bin";

int FishingExchangeKey() {
    int result = 0;

    ReadBG();

#ifdef PAL
    // Debug shortcut: adds a Mardan Garayan catch.
    if (DebugMode && GamePad.Down2(PAD_TRIANGLE)) {
        s32 &caught = SaveData->mardan_garayan_caught;

        caught++;
        SetFishMardanGarayanNum(1);
    }

#endif
    int mardan = AlreadyGetMardanWeapon();
    int party = SaveData->GetDngStatus()->party_size;

    if (party <= 0) {
        party = 1;
    }

    int last_prize = party + 0x19;
    int last_top = party + 0x16;

    if (mardan == 1) {
        last_prize++;
        last_top++;
    }

    int count;

    switch (FishMenu.fade_mode) {
        case FISH_EXCHANGE_FADE_IN:
            FishMenuTextureLoad();
            FishMenu.fade_count++;

            if (FishMenu.ready != 0 && FishMenu.fade_count > 32) {
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            }

            break;
        case FISH_EXCHANGE_FADE_OUT:
            FishMenu.fade_count++;

            if (FishMenu.fade_count > 32) {
                result = 1;
            }

            break;
        case FISH_EXCHANGE_SELECT: {
            int cursor = FishMenu.cursor;
            int top = FishMenu.top;

            if (GamePad.Down(PAD_R2 | PAD_R1 | PAD_RIGHT) != 0) {
                FishMenu.top += 5;
                FishMenu.cursor += 5;
            }

            if (GamePad.Down(PAD_L2 | PAD_L1 | PAD_LEFT) != 0) {
                FishMenu.top -= 5;
                FishMenu.cursor -= 5;

                if (FishMenu.cursor < 0 || FishMenu.top < 0) {
                    FishMenu.cursor = 0;
                    FishMenu.top = 0;
                }
            }

            if (GamePad.Down(PAD_UP) != 0) {
                FishMenu.cursor--;

                if (FishMenu.cursor < 0) {
                    FishMenu.cursor = 0;
                }

                if (FishMenu.cursor < FishMenu.top) {
                    FishMenu.top--;
                }

                if (FishMenu.top < 0) {
                    FishMenu.top = 0;
                    FishMenu.cursor = 0;
                }
            }

            if (GamePad.Down(PAD_DOWN) != 0) {
                FishMenu.cursor++;

                if (last_prize < FishMenu.cursor) {
                    FishMenu.cursor = last_prize;
                }

                if (FishMenu.top < FishMenu.cursor - 4) {
                    FishMenu.top++;

                    if (last_top < FishMenu.top) {
                        FishMenu.top = last_top - 1;
                    }
                }
            }

#ifdef PAL
            // Debug shortcut: raises or lowers the points to spend.
            if (DebugMode) {
                if (GamePad.On2(PAD_CROSS) && FishMenu.point < 9999) {
                    FishMenu.point++;
                }

                if (GamePad.On2(PAD_CIRCLE) && FishMenu.point > 0) {
                    FishMenu.point--;
                }
            }

#endif
            if (last_prize < FishMenu.cursor) {
                FishMenu.cursor = last_prize;
            }

            if (last_top < FishMenu.top) {
                FishMenu.top = last_top - 1;
            }

            if (cursor != FishMenu.cursor) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            FISH_EXCHANGE_ITEM *prize = GetExchangeItemList(FishMenu.cursor);

            if (mardan == 1 && FishMenu.cursor == last_prize) {
                prize = GetExchangeItemList(0x20);
            }

            if (prize != NULL) {
                COM_ITEM_INFO *info = GetCommonItemInfo(prize->item_no);

                if (info != NULL) {
                    int mes_no = info->msg + 500;

                    if (CommonMenuMes2.mes_made != mes_no) {
                        CommonMenuMes2.MakeMesWin(mes_no);
                    }
                }
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                CUserStatus *status = (CUserStatus *) SaveData->GetDngStatus();
                int          full = 0;
                int          kind = WhatIsKindofItem(prize->item_no);
                count = 0;

                switch (kind) {
                    case BOARD_PAGE_ITEM: {
                        int        i;
                        int        k;
                        ITEM_PACK *pack = &status->item_pack;

                        for (i = 0; i < 3; i++) {
                            count += pack->quick_item_qty[i];
                        }

                        for (k = 0; k < pack->num; k++) {
                            if (pack->item[k] >= ITEM_DUNGEON_START) {
                                count++;
                            }
                        }

                        if (count >= pack->num) {
                            full = 1;
                        }

                        break;
                    }
                    case BOARD_PAGE_WEAPON: {
                        int          i;
                        int          owner = WhoIsWeaponEquip(prize->item_no);
                        int          held = 0;
                        WEAPON_HAVE *weapons = status->chara_weapons[owner];

                        for (i = 0; i < 10; i++) {
                            if (weapons[i].item_no >= ITEM_WEAPON_START) {
                                held++;
                            }
                        }

                        if (held >= 10) {
                            full = 1;
                        }

                        break;
                    }
                    case BOARD_PAGE_ATTACH: {
                        int             held;
                        DNG_CONSUMABLE *attach = status->consumable_items;
                        int             entry;
                        held = 0;

                        for (; count < 40; count++) {
                            entry = count * sizeof(DNG_CONSUMABLE);
                            entry = (int) attach + entry;

                            if (((DNG_CONSUMABLE *) entry)->id >= ITEM_ATTACH_START) {
                                held++;
                            }
                        }

                        if (held >= 40) {
                            full = 1;
                        }

                        break;
                    }
                }

                if (prize->price > FishMenu.point) {
                    FishMenu.fade_mode = FISH_EXCHANGE_REFUSE;
                    FishMenu.warning = 14;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else if (full != 0) {
                    FishMenu.warning = kind;
                    FishMenu.fade_mode = FISH_EXCHANGE_REFUSE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else {
                    FishMenu.confirm = 1;
                    FishMenu.cursor_y = FishMenu.confirm * 0x24 + 0x102;
                    FishMenu.fade_mode = FISH_EXCHANGE_CONFIRM;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                FishMenu.fade_mode = FISH_EXCHANGE_FADE_OUT;
                FishMenu.fade_count = 0;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (top != FishMenu.top) {
                FISH_EXCHANGE_ITEM *entry = GetExchangeItemList(FishMenu.top);

                for (int i = 0; i < 5; i++) {
                    if (mardan == 1 && FishMenu.cursor == FishMenu.top + i && FishMenu.cursor == last_prize) {
                        entry = GetExchangeItemList(0x20);
                    }

                    if (entry == NULL) {
                        printf("getinfo is NULL\n");
                    } else {
                        COM_ITEM_INFO *info = GetCommonItemInfo(entry->item_no);

                        if (info != NULL) {
                            AtoraNameMes.mes_no[i] = info->msg + 100;
                            entry++;
                        }
                    }
                }

                AtoraNameMes.mes_made = -1;
                AtoraNameMes.MakeMesWin(0xC8);
            }

            break;
        }
        case FISH_EXCHANGE_CONFIRM: {
            if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
                if (FishMenu.confirm != 0) {
                    FishMenu.confirm = 0;
                } else {
                    FishMenu.confirm = 1;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            FISH_EXCHANGE_ITEM *prize;

            if (GamePad.Down(PAD_CROSS) != 0) {
                int enough = 1;
                prize = GetExchangeItemList(FishMenu.cursor);

                if (mardan == 1 && FishMenu.cursor == last_prize) {
                    prize = GetExchangeItemList(0x20);
                }

                if (prize == NULL) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    break;
                }

                int price = prize->price;

                if (FishMenu.point < price) {
                    enough = 0;
                }

                if (FishMenu.confirm == 0) {
                    if (enough != 0) {
                        ComMenuSePlay(SE_PURCHASE);
                        ((CDngStatusData *) SaveData->GetDngStatus())->GetItem(prize->item_no, 0);
                        FishMenu.point -= price;

                        if (prize->item_no == ITEM_WEAPON_MARDAN_EINS) {
                            SetAlreadyGetMardanWeapon(1);
                            ClearFishMardanGarayanNum();
                            FishMenu.top--;
                            FishMenu.cursor--;
                        }

                        if (FishMenu.point < 0) {
                            FishMenu.point = 0;
                        }
                    } else {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                    }
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                FishMenu.cursor_y = (FishMenu.cursor - FishMenu.top) * 0x22 + 0x7E;
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
            }

            prize = GetExchangeItemList(FishMenu.cursor);

            if (mardan == 1 && FishMenu.cursor == last_prize) {
                prize = GetExchangeItemList(0x20);
            }

            if (prize != NULL) {
                COM_ITEM_INFO *info = GetCommonItemInfo(prize->item_no);

                if (info != NULL) {
                    int msg = info->msg;

                    if (CommonMenuMes1.mes_made != 0xCA || CommonMenuMes1.mes_no[0] != msg + 100 || CommonMenuMes1.values[0] != prize->price) {
                        CommonMenuMes1.mes_made = -1;
                        CommonMenuMes1.mes_no[0] = msg + 100;
                        CommonMenuMes1.values[0] = prize->price;
                        CommonMenuMes1.MakeMesWin(0xCA);
                    }
                }
            }

            break;
        }
        case FISH_EXCHANGE_REFUSE: {
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                FishMenu.fade_mode = FISH_EXCHANGE_SELECT;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (CommonMenuMes1.mes_made != FishMenu.warning + 0xCB) {
                CommonMenuMes1.MakeMesWin(FishMenu.warning + 0xCB);
            }

            break;
        }
    }

    CursorVibeCnt++;

    if (CursorVibeCnt >= 0x066FF300) {
        CursorVibeCnt = 0;
    }

    return result;
}

/**
 * Draws one fish icon.
 *
 * @mangled FishImageIconDraw__Fiiii
 * @address 0x1F2420
 * @size 0x54
 */
static void FishImageIconDraw(int x, int y, int width, int alpha) {
    DrawMenu2DSprite(FishMenuTex, CRect_i_(x, y, width, 0x28), CRect_i_(0, 0xD0, width, 0x28), alpha);
}

/**
 * Returns @p base moved by @p offset.
 */
static inline float AddOffset(float base, float offset) {
    return base + offset;
}

/**
 * Draws the fishing exchange's goods, their prices and its help window.
 *
 * @mangled FishExchangeItemDraw__Fiii
 * @address 0x1F2480
 * @size 0x788
 */
static void FishExchangeItemDraw(int x, int y, int alpha) {
    int   last_prize;
    int   pos_y;
    float win_x;
    float win_y;
    float win_h;
    float win_w;
    int   text_x;
    int   text_y;
    int   i;
    int   row;

    MenuTextureReload(FishMenu.tex_block);
    int mardan = AlreadyGetMardanWeapon();
    int party = SaveData->GetDngStatus()->party_size;

    if (party <= 0) {
        party = 1;
    }

    last_prize = party + 0x19;

    if (mardan == 1) {
        last_prize++;
    }

    DrawMenu2DSprite(FishMenuTex, CRect_i_(x, y, 0x160, 0xD0), CRect_i_(0, 0, 0x160, 0xD0), alpha);
    float rows = last_prize;
    int   bar_x = x + 0x144;
    float step = 126.0f / rows;
    pos_y = (int) AddOffset((float) (y + 0x28), FishMenu.top * step);
    float    length = 630.0f / rows - 8.0f;
    CRect_i_ source(0xF0, 0xD0, 8, 4);
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, pos_y, 8, 4), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, pos_y + 4, 8, (int) length), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, (int) ((float) (pos_y + 4) + length), 8, 4), source, alpha);
    x = (int) (x + 0x16);
    pos_y = y + 0x14;

    for (i = 0; i < 5; i++, pos_y += 0x22) {
        FISH_EXCHANGE_ITEM *prize = GetExchangeItemList(FishMenu.top + i);

        if (mardan == 1 && i == 4 && FishMenu.top + 4 == last_prize) {
            prize = GetExchangeItemList(0x20);
        }

        DrawIconParts(prize->item_no, x, pos_y, y, y + 0xCE, alpha, 0);
        DrawMenu2DSprite(FishMenuTex, CRect_i_(x + 0x104, pos_y + 8, 0x20, 0x14), CRect_i_(0x1E0, 0xEC, 0x20, 0x14), alpha);
#ifdef PAL
#ifdef PAL
        RECT digits = {0x140, 0xEA, 0x10, 0x16};
#else
        RECT digits = {0x160, 0xEA, 0x10, 0x16};
#endif
#else
        RECT digits = {0x140, 0xEA, 0x10, 0x16};
#endif
        DrawMenuNumber(prize->price, x + 0x106, pos_y + 6, FishMenuTex, digits, 1, alpha);

        if (i >= 0 && i < 10) {
            AtoraNameMes.line_pos[i].x = x + 0x24;
            AtoraNameMes.line_pos[i].y = pos_y + 4;
        }
    }

    row = FishMenu.cursor - FishMenu.top;
    int target_y = y + 0x14 + row * 0x22;
    int cursor_x = x - 0x1C;

    if (FishMenu.fade_mode == FISH_EXCHANGE_CONFIRM) {
        cursor_x = 0xF2;
        target_y = (FishMenu.confirm << 5) + 0x102;
    }

    FishMenu.cursor_y += (int) ((float) (target_y - FishMenu.cursor_y) / 4.0f);

    if (abs(FishMenu.cursor_y - target_y) < 2) {
        FishMenu.cursor_y = target_y;
    }

    int cursor_y = FishMenu.cursor_y;
    MenuTextureReload(CommonMenuMes2.tex_block);
    GetMainMenuRightHelpWinLangOffset(win_x, win_y, win_w, win_h);
    MenuHelpWinDraw((int) win_x, (int) win_y, win_w, win_h, 0x80);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
    CommonMenuMes2.edge_alpha = alpha;
    CommonMenuMes2.text_x = (int) (win_x + text_x);
    CommonMenuMes2.text_y = (int) (win_y + text_y);
    CommonMenuMes2.Step();
    CommonMenuMes2.DrawMesWin();
    AtoraNameMes.edge_alpha = alpha;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();
    int point = FishMenu.point;

    if (CommonMenuMes3.value != point) {
        CommonMenuMes3.value = point;
        int digits = GetNumberKeta(point);
        CommonMenuMes3.mes_made = -1;
        CommonMenuMes3.MakeMesWin(digits + 0xCD);
    }

    CommonMenuMes3.edge_alpha = alpha;
    CommonMenuMes3.text_x = 0x46;
#ifdef PAL
    CommonMenuMes3.text_y = 0x160;
#else
    CommonMenuMes3.text_y = 0x140;
#endif
    CommonMenuMes3.Step();
    CommonMenuMes3.DrawMesWin();

    if (FishMenu.fade_mode == FISH_EXCHANGE_CONFIRM || FishMenu.fade_mode == FISH_EXCHANGE_REFUSE) {
        AllFadeForMenu(0x40);

        if (FishMenu.fade_mode == FISH_EXCHANGE_CONFIRM) {
            CRect_i_ dest(0x110, 0x102, 0x60, 0x1F);
            CRect_i_ window(0x1A0, 0, 0x60, 0x20);
            MenuTextureReload(FishMenu.tex_block);
            DrawMenu2DSprite(FishMenuTex, dest, window, 0x80);
            dest.y += 0x20;
            window.y = 0x20;
            DrawMenu2DSprite(FishMenuTex, dest, window, 0x80);
            CommonMenuMes1.auto_pos = MES_POS_CENTRE;
        } else {
            CommonMenuMes1.auto_pos = MES_POS_CENTRE;
        }

        MenuTextureReload(CommonMenuMes1.tex_block);
        CommonMenuMes1.stay_frame = true;
        CommonMenuMes1.text_x = 0xDC;
        CommonMenuMes1.text_y = 0x8C;
        CommonMenuMes1.Step();
        CommonMenuMes1.DrawMesWin();
    }

    if (FishMenu.fade_mode != FISH_EXCHANGE_REFUSE) {
        DrawMenuObjectVibe(cursor_x, cursor_y, 1, 0x40);
    }
}

void FishingExchangeDraw() {
    int frame_alpha;
    int alpha;

    setbilinear(0);
    frame_alpha = 0x40;
    alpha = 0x80;

    switch (FishMenu.fade_mode) {
        case FISH_EXCHANGE_FADE_IN:
            frame_alpha = alpha - FishMenu.fade_count * 2;
            alpha = FishMenu.fade_count * 4;
            break;
        case FISH_EXCHANGE_FADE_OUT:
            frame_alpha = FishMenu.fade_count * 2 + 0x40;
            alpha = alpha - FishMenu.fade_count * 4;
            break;
    }

    if (frame_alpha > 0x80) {
        frame_alpha = 0x80;
    }

    if (frame_alpha < 0x40) {
        frame_alpha = 0x40;
    }

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    if (alpha < 0) {
        alpha = 0;
    }

    FrameImageDraw(frame_alpha, 0x80);

    if (FishMenu.ready != 0) {
        FishImageIconDraw(0x50, 0x32, 0xF0, alpha);
        FishExchangeItemDraw(0x9C, 0x6A, alpha);
        setbilinear(1);
    }
}

void ExitFishingExchange() {
    CommonMenuMes1.auto_pos = MES_POS_NONE;
    SaveData->SetFishingPoint(FishMenu.point);
    TexManager.DeleteTextureBlock(FishMenu.tex_block);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

int FishingExchangeLoop() {
    int done;

    done = FishingExchangeKey();
    FishingExchangeDraw();

    if (done) {
        ExitFishingExchange();
        return 1;
    }

    return 0;
}

s8 FishMsg[18] = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 0, 1, 2, 3, 4, 5, 6, 7};

int GetFishMsgNo(int fish_no) {
    if (fish_no < 0 || fish_no >= 18) {
        return 0;
    }

    return FishMsg[fish_no] + 30;
}

SV_FISH_DATA *GetFishingRankData(int rank_index) {
    return SaveData->GetFishingRank(rank_index);
}

void InitFishRecordView(u_long128 *buffer, int *tex_block, int mode) {
    u_long128 *load_buffer = buffer;

    if (buffer == NULL) {
        load_buffer = (u_long128 *) (EdMenuBuffer.base + EdMenuBuffer.used * 16);
    }

    load_buffer = MenuCalcBufAlignment(load_buffer);
    FishRecordMenu.tex_block = tex_block[0];
    FishRecordMenu.tex_block2 = tex_block[1];
    FishRecordMenu.mode = mode;
    StartReadBG();
    LoadFileBGMenuData("fishrec.pak", load_buffer);
    FishRecordMenu.ready = 0;
    FishRecordMenu.cursor = 0;
    FishRecordMenu.top = 0;
    FishRecordMenu.fade_count = 0;
    FishRecordMenu.fade_mode = FISH_RECORD_FADE_IN;
    StayTex = TexManager.GetTexture("stayframe", -1);
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);
}

/**
 * Leaves the fishing record view and releases its texture block.
 *
 * @mangled ExitFishRecord__Fv
 * @address 0x1F2FA0
 * @size 0x60
 */
static void ExitFishRecord() {
    TexManager.DeleteTextureBlock(FishRecordMenu.tex_block);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

void FishRecordTextureEnter() {
    if (ReadBGSync() != 0) {
        return;
    }

    BG_READ_INFO     *file = GetReadBGFile(0);
    LOADTEXTURE_INFO2 texture[3] = {
        {FishFrameImage, 0, 0},
        {NULL,           0, 0},
        {NULL,           0, 0},
    };
    texture[0].block_no = FishRecordMenu.tex_block;
    texture[1].block_no = FishRecordMenu.tex_block;
    texture[1].name = (char *) GetPackFile((u_int *) file->buffer, "fishrec.img", NULL);
    TexManager.DeleteTextureBlock(FishRecordMenu.tex_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, texture);
    FishMenuTex = TexManager.GetTexture("fprecbrd", -1);
    InitMenuMesSet(MENU_MES_SET_ALLMENU, (short *) GetPackFile((u_int *) file->buffer, FishMessageFile, NULL));
    CommonMenuMes2.mes_made = -1;
    AtoraNameMes.mes_made = -1;

    for (int i = 0; i < 5; i++) {
        SV_FISH_DATA *rank = GetFishingRankData(FishRecordMenu.top + i);

        if (rank != NULL) {
            AtoraNameMes.mes_no[i] = GetFishMsgNo(*(int *) rank);
        } else {
            AtoraNameMes.mes_no[i] = 0;
        }
    }

    AtoraNameMes.MakeMesWin(0xC8);
    FishRecordMenu.cursor_y = 0x7E;
    FishRecordMenu.ready = 1;
}

/**
 * Handles one frame of fishing record input and returns the mode it leaves the view in.
 *
 * @mangled FishRecordViewKey__Fv
 * @address 0x1F31E0
 * @size 0x410
 */
static int FishRecordViewKey() {
    ReadBG();
    int ret = 0;

    switch (FishRecordMenu.fade_mode) {
        case FISH_RECORD_FADE_IN:
            FishRecordMenu.fade_count++;

            if (!FishRecordMenu.ready) {
                FishRecordTextureEnter();
            } else if (FishRecordMenu.fade_count > 0x20) {
                FishRecordMenu.fade_mode = FISH_RECORD_VIEW;
                FishRecordMenu.fade_count = 0;
            }

            break;
        case FISH_RECORD_FADE_OUT:
            FishRecordMenu.fade_count++;

            if (FishRecordMenu.fade_count > 0x20) {
                ExitFishRecord();
                ret = 1;
            }

            break;
        case FISH_RECORD_VIEW: {
            int old_cursor = FishRecordMenu.cursor;

            if (GamePad.Down(PAD_R2 | PAD_R1 | PAD_RIGHT)) {
                FishRecordMenu.top += 5;
                FishRecordMenu.cursor += 5;

                if (FishRecordMenu.top > 0x10) {
                    FishRecordMenu.top = 0xF;
                }

                if (FishRecordMenu.cursor >= 0x13) {
                    FishRecordMenu.cursor = 0x13;
                }
            }

            if (GamePad.Down(PAD_L2 | PAD_L1 | PAD_LEFT)) {
                FishRecordMenu.top -= 5;
                FishRecordMenu.cursor -= 5;

                if (FishRecordMenu.cursor < 0 || FishRecordMenu.top < 0) {
                    FishRecordMenu.cursor = 0;
                    FishRecordMenu.top = 0;
                }
            }

            if (GamePad.Down(PAD_UP)) {
                FishRecordMenu.cursor--;

                if (FishRecordMenu.cursor < 0) {
                    FishRecordMenu.cursor = 0;
                }

                if (FishRecordMenu.cursor < FishRecordMenu.top) {
                    FishRecordMenu.top--;
                }

                if (FishRecordMenu.top < 0) {
                    FishRecordMenu.top = 0;
                    FishRecordMenu.cursor = 0;
                }
            }

            if (GamePad.Down(PAD_DOWN)) {
                FishRecordMenu.cursor++;

                if (FishRecordMenu.cursor >= 0x13) {
                    FishRecordMenu.cursor = 0x13;
                }

                if (FishRecordMenu.top < FishRecordMenu.cursor - 4) {
                    FishRecordMenu.top++;

                    if (FishRecordMenu.top > 0x10) {
                        FishRecordMenu.top = 0x10;
                    }
                }
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                FishRecordMenu.fade_mode = FISH_RECORD_FADE_OUT;
                FishRecordMenu.fade_count = 0;
            }

#ifdef PAL
            // Debug shortcut: records a random catch.
            if (DebugMode && GamePad.Down2(PAD_CROSS)) {
                int fish_id = rand() % 18;
                SaveData->SetFishingRank(fish_id, rand() % 20);
            }

#endif
            if (old_cursor != FishRecordMenu.cursor) {
                ComMenuSePlay(MENU_SOUND_CURSOR);

                for (int i = 0; i < 5; i++) {
                    SV_FISH_DATA *fish = GetFishingRankData(FishRecordMenu.top + i);

                    if (fish != NULL) {
                        AtoraNameMes.mes_no[i] = GetFishMsgNo(fish->fish_id);
                    } else {
                        AtoraNameMes.mes_no[i] = 0;
                    }
                }

                AtoraNameMes.mes_made = -1;
                AtoraNameMes.MakeMesWin(0xC8);
            }

            break;
        }
    }

    return ret;
}

/**
 * Draws the fishing record board: each rank, its fish and its size.
 *
 * @mangled FishRecordViewBoard__Fiii
 * @address 0x1F35F0
 * @size 0x648
 */
static void FishRecordViewBoard(int x, int y, int alpha) {
    int pos_x;
    int pos_y;

    MenuTextureReload(FishMenu.tex_block);
    DrawMenu2DSprite(FishMenuTex, CRect_i_(x, y, 0x160, 0xD0), CRect_i_(0, 0, 0x160, 0xD0), alpha);
    int head_y = y - 0xE;
    pos_x = x + 0x10;
    CRect_i_ head(0x160, 0x1A, 0x44, 0x1C);
    int      widths[3] = {0x54, 0x5A, 0};

    for (int i = 0; i <= 2; i++, head.y += head.height) {
        if (i == 2) {
            head.width = 0x74;
        }

        DrawMenu2DSprite(FishMenuTex, CRect_i_(pos_x, head_y, head.width, head.height - 1), head, alpha);
        pos_x += widths[i];
    }

    int      bar_x = x + 0x144;
    float    length = 23.5f;
    int      bar_y = (int) ((float) (y + 0x2A) + 6.3f * FishRecordMenu.top);
    CRect_i_ source(0x110, 0xD0, 8, 4);
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, bar_y, 8, 4), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, bar_y + 4, 8, (int) length), source, alpha);
    source.y += 4;
    DrawMenu2DSprite(FishMenuTex, CRect_i_(bar_x, (int) ((float) (bar_y + 4) + length), 8, 4), source, alpha);
    pos_x = x + 0x12;
    pos_y = y + 6;

    for (int i = 0; i < 5; i++, pos_y += 0x22) {
        int           rank_no = FishRecordMenu.top + i;
        SV_FISH_DATA *record = GetFishingRankData(rank_no);
        RECT          digits = {0x158, 0xDC, 0xE, 0x12};

        if (rank_no == FishRecordMenu.cursor) {
            digits.y += digits.height;
        }

        CRect_i_ medal(0x160, 0, 0x20, 0x1B);
        int      medal_x = pos_x + 0x10;

        if (rank_no != 0) {
            medal.x += 0x20;

            if (rank_no == FishRecordMenu.cursor) {
                medal.x += 0x20;
            }
        }

        DrawMenu2DSprite(FishMenuTex, CRect_i_(medal_x, pos_y + 0x14, medal.width, medal.height - 1), medal, alpha);
        int number_x = medal_x + 0x14;

        if (GetNumberKeta(rank_no + 1) >= 2) {
            number_x += digits.width >> 1;
        }

        DrawMenuNumber(rank_no + 1, number_x, pos_y + 0x18, FishMenuTex, digits, 1, alpha);
        int size = 0;

        if (record != NULL) {
            size = (int) record->rank;
        }

        if (size > 0) {
            DrawMenuNumber(size, pos_x + 0xFE, pos_y + 0x18, FishMenuTex, digits, 1, alpha);
            DrawMenu2DSprite(FishMenuTex, CRect_i_(pos_x + 0xFE, pos_y + 0x16, digits.width * 2, digits.height), CRect_i_(digits.x + digits.width * 10, digits.y, digits.width * 2, digits.height), alpha);

            if (i >= 0 && i < 10) {
                AtoraNameMes.line_pos[i].x = pos_x + 0x3E;
                AtoraNameMes.line_pos[i].y = pos_y + 0x14;
            }
        }
    }

    if (GetMardanGareyanFlag() != 0) {
        pos_x = x + 0x134;
        pos_y = y + 0xB6;
        DrawMenu2DSprite(FishMenuTex, CRect_i_(pos_x, pos_y, 0x28, 0x28), CRect_i_(0x1D8, 0xB4, 0x28, 0x28), alpha);
    }

    int row = FishRecordMenu.cursor - FishRecordMenu.top;
    pos_y = y + 0x14 + row * 0x22;
    pos_x = x + 0x16;
    pos_x = (int) (pos_x - 0x1C);
    FishRecordMenu.cursor_y += (int) ((float) (pos_y - FishRecordMenu.cursor_y) / 4.0f);

    if (abs(FishRecordMenu.cursor_y - pos_y) < 2) {
        FishRecordMenu.cursor_y = pos_y;
    }

    pos_y = FishRecordMenu.cursor_y;
    MenuTextureReload(CommonMenuMes2.tex_block);
    AtoraNameMes.edge_alpha = alpha;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();
    DrawMenuObjectVibe(pos_x, pos_y, 1, 0x40);
}

/**
 * Draws one frame of the fishing record view.
 *
 * @mangled FishRecordViewDraw__Fv
 * @address 0x1F3C40
 * @size 0x11C
 */
static void FishRecordViewDraw() {
    int frame_alpha;
    int alpha;

    setbilinear(0);
    frame_alpha = 0x40;
    alpha = 0x80;

    switch (FishRecordMenu.fade_mode) {
        case FISH_RECORD_FADE_IN:
            frame_alpha = alpha - FishRecordMenu.fade_count * 2;
            alpha = FishRecordMenu.fade_count * 4;
            break;
        case FISH_RECORD_FADE_OUT:
            frame_alpha = FishRecordMenu.fade_count * 2 + 0x40;
            alpha = alpha - FishRecordMenu.fade_count * 4;
            break;
    }

    if (frame_alpha > 0x80) {
        frame_alpha = 0x80;
    }

    if (frame_alpha < 0x40) {
        frame_alpha = 0x40;
    }

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    if (alpha < 0) {
        alpha = 0;
    }

    FrameImageDraw(frame_alpha, 0x80);

    if (FishRecordMenu.ready != 0) {
        FishImageIconDraw(0x50, 0x32, 0x10C, alpha);
        FishRecordViewBoard(0x9C, 0x6A, alpha);
        setbilinear(1);
    }
}

int FishRecordViewLoop() {
    int done;

    done = FishRecordViewKey();
    FishRecordViewDraw();
    return done;
}
