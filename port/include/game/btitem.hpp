#pragma once

/**
 * How the large treasure box shows its item, as TreasureboxBig_itemType holds it.
 */
// clang-format off
enum TreasureboxItemPose {
    TREASUREBOX_POSE_DEFAULT   = 0, /**< Default. */
    TREASUREBOX_POSE_SLINGSHOT = 1, /**< Xiao's slingshot. */
    TREASUREBOX_POSE_GUN       = 2, /**< Osmond's gun. */
    TREASUREBOX_POSE_RING      = 3, /**< Ruby's ring, at double scale. */
};

// clang-format on

/**
 * Base name of each item's model and texture files, beginning with attachments.
 */
extern char *ITEM_NAME_TBL_NEW[];

/** Character file of each party member, as selectChrUnit loads it. */
extern char *charaNameTbl[6];

/** Step the large-treasure-box presentation is on. */
extern int BtGetTreasurebox_Sled;

/** Step the atla-ball pickup presentation is on. */
extern int BtGetAtraBoll_Sled;

/** Item the large-treasure-box presentation is showing. */
extern int TreasureboxBig_itemNo;

/** Kind of the item the large-treasure-box presentation is showing. */
extern int TreasureboxBig_itemType;

/** Scale the large-treasure-box presentation draws its item at. */
extern float TreasureboxBig_itemScale;

/** Item identifier shown by the small-treasure and attachment pickup flows. */
extern int BtGetTreasureboxSmall_itemNo;

/** Item quantity shown by the small-treasure and attachment pickup flows. */
extern int BtGetTreasureboxSmall_itemVolume;

/** Atla the atla-ball pickup presentation collected. */
extern int BtAtraGetID;

/** Map event the atla-ball pickup presentation was started from. */
extern int BtAtraGetNo;

/** Step the small character-select window is on. */
extern int BtMiniChrSelecter_Sled;

/** Selection mode the small character-select window was opened with. */
extern int BtMiniChrSel_Type;

/** Party member the small character-select window returned. */
extern int BtMiniChrSelectNo;

/** Step the small item-select window is on. */
extern int BtMiniItemSelect_Sled;

/** Item the gate-key presentation is showing. */
extern int GateKey_itemNo;

/** Step the gate-key and attachment pickup presentations are on. */
extern int GateKey_Sled;

/** Marks that the party is holding a gate key. */
extern int gateItemFlag;

/** Model the escape presentation draws. */
extern int escape_chr;

/** Step the escape presentation is on. */
extern int escape_sled;

/**
 * Puts one party member in the player's hands, loading them if need be.
 *
 * @mangled selectChrUnit__Fii
 * @address 0x1D1030
 * @size 0x368
 */
void selectChrUnit(int chara_no, int reload);

/**
 * Loads the icons for the active item slots.
 *
 * @mangled LoadActiveItemIcon__Fv
 * @address 0x1D13A0
 * @size 0x4C
 */
void LoadActiveItemIcon();

/**
 * Opens the small character-select window in the given selection mode.
 *
 * @mangled BtMiniChrSelect_Init__Fi
 * @address 0x1D3290
 * @size 0x40
 */
void BtMiniChrSelect_Init(int type);

/**
 * Opens the small item-select window.
 *
 * @mangled BtMiniItemSelect__Fv
 * @address 0x1D3400
 * @size 0x38
 */
void BtMiniItemSelect();

/**
 * Runs one frame of the small item-select window, and reports when it closes.
 *
 * @mangled BtMiniItemSelect_Loop__Fv
 * @address 0x1D3440
 * @size 0x118
 */
int BtMiniItemSelect_Loop();

/**
 * Starts the gate-key pickup presentation for the given item.
 *
 * @mangled BtGetGateKey_Init__Fi
 * @address 0x1D3560
 * @size 0x13C
 */
void BtGetGateKey_Init(int item_no);

/**
 * Gives the player an attachment and opens its acquisition message.
 *
 * @mangled BtGetAttach_Init__Fii
 * @address 0x1D3A70
 * @size 0x90
 */
void BtGetAttach_Init(int dungeon, int item_no);

/**
 * Runs one frame of the attachment pickup message, and reports when it ends.
 *
 * @mangled BtGetAttach_Loop__Fv
 * @address 0x1D3B00
 * @size 0xF0
 */
int BtGetAttach_Loop();

/**
 * Starts the presentation that carries the party off the floor.
 *
 * @mangled BtEscape_Init__Fv
 * @address 0x1D3BF0
 * @size 0x14C
 */
void BtEscape_Init();

/**
 * Builds the velocity of a shot fired at the given speed and angles.
 *
 * @mangled setShotVector__FPffff
 * @address 0x1D4100
 * @size 0x98
 */
void setShotVector(float *velocity, float speed, float angle_y, float angle_x);

/**
 * Gives the direction the main character faces, tilted by the given pitch.
 *
 * @mangled getCharacterVector__FPff
 * @address 0x1D41A0
 * @size 0xC0
 */
void getCharacterVector(float *vector, float pitch);

/**
 * Model file of the pickup, shared with the treasure chests.
 */
extern char BtAtraShortCharaFile[];

/**
 * Effect file of the short Atlamillia presentation.
 */
extern char BtAtraShortEffectFile[];

/**
 * Effect configuration entry inside an effect pack, shared by the pickup and escape presentations.
 */
extern char BtEffectInfoFile[];

/**
 * Empty name the model table gives the items that have no model of their own.
 */
extern char no_item_name[];
