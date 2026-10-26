#pragma once

#include "common.h"

#include "itemdata.hpp"
#include "stockitem.hpp"

/**
 * What the shopkeeper is saying or prompting, as ShopMenuWork::talk_mode holds it.
 */
// clang-format off
enum ShopTalkMode {
    SHOP_TALK_NONE               = 0,  /**< Shopping. */
    SHOP_TALK_FADE_IN            = 1,  /**< Fading in. */
    SHOP_TALK_FADE_OUT           = 2,  /**< Fading out. */
    SHOP_TALK_GREETING           = 3,  /**< Greeting. */
    SHOP_TALK_CANNOT_SELL        = 4,  /**< The item cannot be sold. */
    SHOP_TALK_WEAPON_EQUIPPED    = 5,  /**< The weapon is equipped. */
    SHOP_TALK_DEAL_PENDING       = 6,  /**< Marked goods wait to be dealt. */
    SHOP_TALK_CANNOT_CHARGE      = 7,  /**< The item cannot be stored. */
    SHOP_TALK_STOCK_FULL         = 8,  /**< The storage is full. */
    SHOP_TALK_UNK_9              = 9,  /**< Dismissed like a message; never set. */
    SHOP_TALK_INVENTORY_OVERFLOW = 10, /**< The inventory would overflow. */
    SHOP_TALK_NO_SPACE           = 11, /**< No room on the board. */
    SHOP_TALK_CONFIRM_DEAL       = 12, /**< Confirming the deal. */
    SHOP_TALK_BUY_PROMPT         = 13, /**< Asking whether to buy. */
    SHOP_TALK_SELL_PROMPT        = 14, /**< Asking whether to sell. */
    SHOP_TALK_UNK_10             = 16, /**< Dismissed like a message; never set. */
    SHOP_TALK_CHARGE_PUT_PROMPT  = 17, /**< Asking whether to store an item. */
    SHOP_TALK_CHARGE_TAKE_PROMPT = 18, /**< Asking whether to take out an item. */
    SHOP_TALK_DEAL_DONE          = 20, /**< The deal is done. */
    SHOP_TALK_NOT_ENOUGH_MONEY   = 21, /**< Too little money. */
    SHOP_TALK_TOO_MUCH_MONEY     = 22, /**< The wallet would overflow. */
    SHOP_TALK_NOTHING_MARKED     = 23, /**< Nothing is marked. */
    SHOP_TALK_LEAVE_PROMPT       = 24, /**< Asking whether to leave. */
    SHOP_TALK_GOODBYE            = 25, /**< Saying goodbye. */
    SHOP_TALK_BUTTON_FLASH       = 26, /**< Check button flashing. */
};

// clang-format on

/**
 * Whether the shopkeeper is talking, as ShopMenuWork::msg_mode holds it.
 */
// clang-format off
enum ShopMsgMode {
    SHOP_MSG_IDLE    = 0, /**< Waiting. */
    SHOP_MSG_TALKING = 1, /**< Talking. */
};

// clang-format on

/**
 * Which board the shop cursor is on, as ShopMenuWork::side holds it.
 */
// clang-format off
enum ShopSide {
    SHOP_SIDE_STOCK        = 0, /**< The shop's goods or the storage. */
    SHOP_SIDE_PERSONAL     = 1, /**< The player's board. */
    SHOP_SIDE_CHECK_BUTTON = 2, /**< The item shop's check button. */
};

// clang-format on

/**
 * Who a shop board slot's item belongs to.
 */
// clang-format off
enum ShopSlotState {
    SHOP_SLOT_EMPTY       = 0, /**< Empty. */
    SHOP_SLOT_SHOP_GOOD   = 1, /**< One of the shop's goods. */
    SHOP_SLOT_PLAYER_ITEM = 2, /**< One of the player's items. */
};

// clang-format on

/**
 * Which shop is open, as ChargeOrShopFlag holds it.
 */
// clang-format off
enum ShopKind {
    SHOP_KIND_CHARGE = 0, /**< The storage shop. */
    SHOP_KIND_ITEM   = 1, /**< An item shop. */
};

// clang-format on

/**
 * Steps of the fish point exchange, as FishMenuWork::fade_mode holds them.
 */
// clang-format off
enum FishExchangeMode {
    FISH_EXCHANGE_FADE_IN  = 0, /**< Opening. */
    FISH_EXCHANGE_FADE_OUT = 1, /**< Closing. */
    FISH_EXCHANGE_SELECT   = 3, /**< Choosing a prize. */
    FISH_EXCHANGE_CONFIRM  = 4, /**< Confirming. */
    FISH_EXCHANGE_REFUSE   = 5, /**< Refusing. */
};

// clang-format on

/**
 * Steps of the fishing record view, as FishRecordMenuWork::fade_mode holds them.
 */
// clang-format off
enum FishRecordMode {
    FISH_RECORD_FADE_IN  = 0, /**< Opening. */
    FISH_RECORD_FADE_OUT = 1, /**< Closing. */
    FISH_RECORD_VIEW     = 2, /**< Viewing. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
struct ITEM_PACK;
struct SV_FISH_DATA;

/**
 * The record behind one shop slot, read according to the kind of item in it.
 */
union MENU_ITEMDATA {
    s32         volume;      /**< A dungeon item's volume. */
    WEAPON_HAVE weapon;      /**< A weapon's record. */
    ATTACH_LIST attach;      /**< An attachment's record. */
    s16         param[0x7C]; /**< The record read as halfwords; the stat attachments ITEM_ATTACH_ATTACK to ITEM_ATTACH_MAGICAL_POWER each set one of an attachment's stats. */
};

STATIC_ASSERT(sizeof(MENU_ITEMDATA) == 0xF8);

/**
 * Stores one shop's item-list state.
 */
struct SHOP_ITEMLIST {
    s16           item_no; /**< The item, weapon or attachment on offer. */
    u8            unk_02[2];
    MENU_ITEMDATA data; /**< The item's record. */
};

STATIC_ASSERT(sizeof(SHOP_ITEMLIST) == 0xFC);

/**
 * Stores one item held temporarily by a menu.
 */
struct IHAVEITEM {
    s32 slot_state; /**< State of the board slot the held item was taken from, handed back to the slot it is put down in. */
    s32 from_page;  /**< Board the held item was taken from: a PersonalBoardPage, or a WepMenuMode attachment screen in the weapon menu. */
    s32 last_slot;  /**< Slot the item shop last picked the held item up from. */
    s32 from_slot;  /**< Slot on that board the held item was taken from, where a cancel returns it. */
    s16 item_no;    /**< The item held. */
    s16 volume;     /**< The held item's volume. */
};

STATIC_ASSERT(sizeof(IHAVEITEM) == 0x14);

/**
 * Points to the item record held by the shop menu.
 */
extern IHAVEITEM *ShopHaveItemPt;

/**
 * Points to the weapon record held by the shop menu.
 */
extern WEAPON_HAVE *ShopHaveWepPt;

/**
 * Points to the attachment record held by the shop menu.
 */
extern ATTACH_LIST *ShopHaveAttachPt;

/**
 * Gives the goods list one item shop sells from.
 *
 * @mangled GetItemShopList__Fi
 * @address 0x1E68D0
 * @size 0x20
 */
s16 *GetItemShopList(int shop_no);

/**
 * Clears an item-list record when the supplied pointer is valid.
 *
 * @mangled InitShopItemListData__FP13SHOP_ITEMLIST
 * @address 0x1E68F0
 * @size 0x38
 */
void InitShopItemListData(SHOP_ITEMLIST *item_list);

/**
 * Tracks one shop icon flying from its shelf to the slot it was bought or sold into.
 */
class ShopIconMove {
public:
    s16           to_stock; /**< Nonzero routes the icon into the shop's stock; zero writes it straight into the player's status. */
    s16           spare;    /**< Reset to -1 whenever the flight is cleared. */
    s16           slot_no;  /**< Indexes the board slot, and the matching CStockItem/CUserStatus entry, the icon is bound for. */
    s16           icon_no;  /**< Indexes the icon's position on the board; divided and taken modulo five gives its row and column. */
    float         pos_x;    /**< Current horizontal screen position of the flying icon. */
    float         pos_y;    /**< Current vertical screen position of the flying icon. */
    s16           item_no;  /**< The item, weapon or attachment identifier the icon is carrying. */
    u8            unk_12[2];
    MENU_ITEMDATA data; /**< A copy of the item, weapon or attachment record. */

    /**
     * Aims a shop icon at the slot it is to fly to.
     *
     * @mangled IconMoveTarSet__12ShopIconMoveFiiiP13MENU_ITEMDATAffi
     * @address 0x1E6930
     * @size 0x68
     */
    void IconMoveTarSet(int slot_no, int icon_no, int item_no, MENU_ITEMDATA *item_data, float start_x, float start_y, int to_stock);

    /**
     * Advances a flying shop icon and reports when it has arrived.
     *
     * @mangled IconAutoMove__12ShopIconMoveFii
     * @address 0x1E69A0
     * @size 0x460
     */
    int IconAutoMove(int item_shop, int force_arrive);

    /**
     * Draws a shop icon part-way through its flight.
     *
     * @mangled IconAutoMoveDraw__12ShopIconMoveFv
     * @address 0x1E6E00
     * @size 0xE8
     */
    void IconAutoMoveDraw();
};

STATIC_ASSERT(sizeof(ShopIconMove) == 0x10C);

/**
 * Turns a shop number into the shop and the master it stands for.
 *
 * @mangled ShopNoInput__FPiii
 * @address 0x1E6F40
 * @size 0x88
 */
int ShopNoInput(int *tex_block, int shop_no, int mode);

/**
 * Clears the shop's held item, weapon and attachment lists.
 *
 * @mangled InitAllHaveData__Fv
 * @address 0x1E6FD0
 * @size 0x44
 */
void InitAllHaveData();

/**
 * Runs one frame of whichever shop is open.
 *
 * @mangled CommonShopLoop__Fv
 * @address 0x1E7020
 * @size 0x5C
 */
int CommonShopLoop();

/**
 * Enters the shop's fixed textures into the texture manager.
 *
 * @mangled ShopTextureLoadFix__Fv
 * @address 0x1E7CD0
 * @size 0x1D0
 */
void ShopTextureLoadFix();

/**
 * Starts the charge shop up on the player's stock.
 *
 * @mangled InitChargeShop__FPiii
 * @address 0x1E8490
 * @size 0x188
 */
void InitChargeShop(int *tex_block, int shop_no, int mode);

/**
 * Keeps the charge shop's scrolled view on the row the cursor is on.
 *
 * @mangled ChargeShopLimmitCheck__Fv
 * @address 0x1E8620
 * @size 0x274
 */
void ChargeShopLimmitCheck();

/**
 * Runs one frame of the charge shop and returns the mode its input handler left.
 *
 * @mangled ChargeShopLoop__Fv
 * @address 0x1E88C0
 * @size 0x50
 */
int ChargeShopLoop();

/**
 * Handles one frame of charge shop input and returns the mode it leaves the shop in.
 *
 * @mangled ChargeShopKey__Fv
 * @address 0x1E8910
 * @size 0x15CC
 */
int ChargeShopKey();

/**
 * Draws one frame of the charge shop.
 *
 * @mangled DrawChargeShop__Fv
 * @address 0x1EA7A0
 * @size 0x5A4
 */
void DrawChargeShop();

/**
 * Draws the charge shop's personal board: its frame, scroll bar, tags and icons.
 *
 * @mangled ChargeShopBoardDraw__Fiii
 * @address 0x1EAF40
 * @size 0x384
 */
void ChargeShopBoardDraw(int x, int y, int alpha);

/**
 * Totals the prices of goods currently marked for purchase.
 *
 * @mangled BuyMoneyCheck2__Fv
 * @address 0x1EB3A0
 * @size 0x1A0
 */
static int BuyMoneyCheck2();

/**
 * Orders two items for the shop's sort.
 *
 * @mangled CompItem1__Fii
 * @address 0x1EBB50
 * @size 0x11C
 */
int CompItem1(int first_item_no, int second_item_no);

/**
 * Sorts one shop item board into order and returns one when any entry moved.
 *
 * @mangled SeitonShopItemBoardSub__FP9ITEM_PACK
 * @address 0x1EBC70
 * @size 0x138
 */
int SeitonShopItemBoardSub(ITEM_PACK *pack);

/**
 * Orders two attachments for the shop's sort.
 *
 * @mangled CompAttach1__FP11ATTACH_LISTP11ATTACH_LIST
 * @address 0x1EBE40
 * @size 0xFC
 */
int CompAttach1(ATTACH_LIST *first, ATTACH_LIST *second);

/**
 * Sorts one attachment board into order and returns one when any entry moved.
 *
 * @mangled SeitonShopAttachBoardSub__FP11ATTACH_LIST
 * @address 0x1EBF40
 * @size 0x138
 */
int SeitonShopAttachBoardSub(ATTACH_LIST *attachments);

/**
 * Runs one frame of the item shop and returns the mode its input handler left.
 *
 * @mangled ItemShopLoop2__Fv
 * @address 0x1EC120
 * @size 0x50
 */
int ItemShopLoop2();

/**
 * Moves the shop cursor between the goods side and the player's side and returns zero.
 *
 * @mangled CheckSideKey2__Fv
 * @address 0x1EC170
 * @size 0x1F4
 */
int CheckSideKey2();

/**
 * Draws the price tickets of everything marked on the player's side.
 *
 * @mangled DrawSellTicket22__Fiiiii
 * @address 0x1ECD10
 * @size 0x274
 */
void DrawSellTicket22(int x, int y, int clip_top, int clip_bottom, int alpha);

/**
 * Builds the file name of a shopkeeper's model archive.
 *
 * @mangled ItemShopGetPacFileName__FiiPc
 * @address 0x1EDA80
 * @size 0xD8
 * Builds the archive file name of one shop's goods into the buffer it is given.
 */
void ItemShopGetPacFileName(int shop_kind, int shop_no, char *file_name);

/**
 * Builds the file name of a shopkeeper's texture archive.
 *
 * @mangled ItemShopGetImgFileName__FiiPc
 * @address 0x1EDB60
 * @size 0x8C
 */
void ItemShopGetImgFileName(int shop_kind, int shop_no, char *file_name);

/**
 * Takes the shop's four board tables out of its arena.
 *
 * @mangled ItemShopMemoryAlloc__Fv
 * @address 0x1EDBF0
 * @size 0x10C
 * Allocates and clears the item shop's working buffer.
 */
void ItemShopMemoryAlloc();

/**
 * Clears the marks on everything the player is carrying.
 *
 * @mangled ItemPosInfoInit__Fv
 * @address 0x1EDD00
 * @size 0x194
 * Rebuilds the shop's slot table, marking each item and weapon slot as taken or free.
 */
void ItemPosInfoInit();

/**
 * Starts the item shop up: its goods, its slot table and its buffers.
 *
 * @mangled InitItemShop2__FPiii
 * @address 0x1EE0F0
 * @size 0x190
 */
void InitItemShop2(int *tex_block, int shop_no, int mode);

/**
 * Handles one frame of item shop input and returns the mode it leaves the shop in.
 *
 * @mangled ItemShopKey2__Fv
 * @address 0x1EED70
 * @size 0x1A90
 */
int ItemShopKey2();

/**
 * Draws the item shop with its boards, tickets and shopkeeper.
 *
 * @mangled ItemShopDraw2__Fv
 * @address 0x1F0800
 * @size 0xA4C
 */
void ItemShopDraw2();

/**
 * Returns the save data's Mardan Garayan progress flag.
 *
 * @mangled GetMardanGareyanFlag__Fv
 * @address 0x1F1270
 * @size 0x18
 */
int GetMardanGareyanFlag();

/**
 * Records whether the player has received the Mardan Garayan weapon.
 *
 * @mangled SetAlreadyGetMardanWeapon__Fi
 * @address 0x1F1290
 * @size 0x2C
 */
void SetAlreadyGetMardanWeapon(int taken);

/**
 * Adds to the count of Mardan Garayan caught, holding it between zero and ten thousand.
 *
 * @mangled SetFishMardanGarayanNum__Fi
 * @address 0x1F12C0
 * @size 0x78
 */
void SetFishMardanGarayanNum(int count);

/**
 * Returns the number of fish counted toward the Mardan weapon requirement.
 *
 * @mangled GetFishMardanGarayanNum__Fv
 * @address 0x1F1340
 * @size 0x28
 */
int GetFishMardanGarayanNum();

/**
 * Clears the fish count used for the Mardan Garayan weapon requirement.
 *
 * @mangled ClearFishMardanGarayanNum__Fv
 * @address 0x1F1370
 * @size 0x2C
 */
void ClearFishMardanGarayanNum();

/**
 * Opens the fishing prize exchange and reads its data.
 *
 * @mangled InitFishingExchange__FP1Pii
 * @address 0x1F1410
 * @size 0x1C0
 */
void InitFishingExchange(u_long128 *buffer, int *tex_block, int mode);

/**
 * Handles one frame of fishing exchange input and returns the mode it leaves the exchange in.
 *
 * @mangled FishingExchangeKey__Fv
 * @address 0x1F1880
 * @size 0xB94
 */
int FishingExchangeKey();

/**
 * Draws one frame of the fishing exchange.
 *
 * @mangled FishingExchangeDraw__Fv
 * @address 0x1F2C10
 * @size 0x11C
 */
void FishingExchangeDraw();

/**
 * Leaves the fishing exchange and saves the fishing points it ends on.
 *
 * @mangled ExitFishingExchange__Fv
 * @address 0x1F2D30
 * @size 0x80
 */
void ExitFishingExchange();

/**
 * Runs one frame of the fishing exchange and returns one once its input handler has closed it.
 *
 * @mangled FishingExchangeLoop__Fv
 * @address 0x1F2DB0
 * @size 0x54
 */
int FishingExchangeLoop();

/**
 * Gives the name message of one kind of fish.
 *
 * @mangled GetFishMsgNo__Fi
 * @address 0x1F2E10
 * @size 0x3C
 */
int GetFishMsgNo(int fish_no);

/**
 * Returns one entry of the fishing leaderboard, or NULL when that rank is empty.
 *
 * @mangled GetFishingRankData__Fi
 * @address 0x1F2E50
 * @size 0x28
 */
SV_FISH_DATA *GetFishingRankData(int rank_index);

/**
 * Opens the page listing the fish that have been caught.
 *
 * @mangled InitFishRecordView__FP1Pii
 * @address 0x1F2E80
 * @size 0x120
 */
void InitFishRecordView(u_long128 *buffer, int *tex_block, int mode);

/**
 * Loads the fishing record view's textures and message buffers.
 *
 * @mangled FishRecordTextureEnter__Fv
 * @address 0x1F3000
 * @size 0x1DC
 */
void FishRecordTextureEnter();

/**
 * Runs one frame of the fish record view and returns the mode its input handler left.
 *
 * @mangled FishRecordViewLoop__Fv
 * @address 0x1F3D60
 * @size 0x38
 */
int FishRecordViewLoop();

/** Message number, less thirty, naming each kind of fish. */
extern s8 FishMsg[18];

/**
 * One prize the fishing exchange offers.
 */
struct FISH_EXCHANGE_ITEM {
    s16 item_no; /**< Prize the exchange offers. */
    s16 price;   /**< Fishing points the prize costs. */
};

STATIC_ASSERT(sizeof(FISH_EXCHANGE_ITEM) == 4);

/** The prizes the fishing exchange offers. */
extern FISH_EXCHANGE_ITEM exitemlst[35];

/**
 * Frame image of the fish exchange window.
 */
extern char FishFrameImage[];

/**
 * Message file of the fish exchange.
 */
extern char FishMessageFile[];
