#pragma once

#include "common.h"

class CTexture;
struct IHAVEITEM;
struct WEAPON_HAVE;

/** Texture sheet used to draw personal inventory boards. */
extern CTexture *PerBoardTex;

/**
 * Places or exchanges a weapon selected on the personal inventory board.
 *
 * @mangled PersonalBoardWeaponPush__FP9IHAVEITEMi
 * @address 0x0022F0A0
 * @size 0x350
 */
int PersonalBoardWeaponPush(IHAVEITEM *item, int board_pos);

/**
 * Places or exchanges an attachment selected on the personal inventory board.
 *
 * @mangled PersonalBoardAttachPush__FP9IHAVEITEMi
 * @address 0x0022F3F0
 * @size 0xD8
 */
int PersonalBoardAttachPush(IHAVEITEM *item, int board_pos);

/**
 * Gets or exchanges the item at the selected personal-board position.
 *
 * @mangled PersonalBoardItemGetorSwap__Fi
 * @address 0x0022F4D0
 * @size 0x7C
 */
int PersonalBoardItemGetorSwap(int board_pos);

/**
 * Cancels the current personal-board item operation.
 *
 * @mangled PersonalBoardItemCancel__Fv
 * @address 0x0022F550
 * @size 0x208
 */
void PersonalBoardItemCancel();

/**
 * Returns the maximum visible entry count for a personal-board mode.
 *
 * @mangled PersonalRetMax__Fi
 * @address 0x0022F760
 * @size 0x64
 */
int PersonalRetMax(int board_mode);

/**
 * Draws the entries in the active personal inventory board.
 *
 * @mangled DrawPersonalBoard__Fiiiii
 * @address 0x0022F7D0
 * @size 0x1FC
 */
void DrawPersonalBoard(int x, int y, int board_mode, int alpha, int unused);

/**
 * Draws the icons of one personal board page, five to a row, clipped to the board's top and bottom.
 *
 * @mangled CommonIconDraw__Fiiiiiii
 * @address 0x0022FAD0
 * @size 0x4F0
 */
void CommonIconDraw(int page, int count, int x, int y, int top, int bottom, int alpha);

/**
 * Draws the frame surrounding a personal inventory board.
 *
 * @mangled PersonalBoardDrawWaku__FiiP8CTexturei
 * @address 0x0022FFC0
 * @size 0x17C
 */
void PersonalBoardDrawWaku(int x, int y, CTexture *texture, int alpha);

/**
 * Draws the option area associated with a personal inventory board.
 *
 * @mangled PersonalBoardOptionDraw__FiiiiP8CTexturei
 * @address 0x00230140
 * @size 0xC8
 */
void PersonalBoardOptionDraw(int board_mode, int count, int x, int y, CTexture *texture, int alpha);

/**
 * Draws the tabs above a personal inventory board, the selected one lit, and in
 * the village board the village tabs and the selected village's name.
 *
 * @mangled PersonalBoardTagDraw__FiiiP8CTextureii
 * @address 0x00230210
 * @size 0x50C
 */
void PersonalBoardTagDraw(int tag, int x, int y, CTexture *texture, int shift, int alpha);

/**
 * Draws and updates the personal-board scroll bar.
 *
 * @mangled PersonalBoardScrlBarDraw__FiiiRfUcP8CTexturei
 * @address 0x00230720
 * @size 0x294
 */
void PersonalBoardScrlBarDraw(int count, int x, int y, float &scroll, unsigned char top_row, CTexture *texture, int alpha);

/**
 * Draws the number of entries on a personal inventory board and its capacity,
 * the capacity picked out once the board is over it.
 *
 * @mangled PersonalBoardMaxDraw__FiiiP8CTexturei
 * @address 0x002309C0
 * @size 0x238
 */
void PersonalBoardMaxDraw(int num, int x, int y, CTexture *texture, int alpha);

/**
 * Draws the rows of a personal board's slot grid down to the clip bottom, with a divider bar above the third row
 * (mark 1) or above every other row (mark 2).
 *
 * @mangled DrawPerBoardDraw__FiiiiiiP8CTexturei
 * @address 0x00230DC0
 * @size 0x298
 */
void DrawPerBoardDraw(int mark, int count, int x, int y, int top, int bottom, CTexture *texture, int alpha);

/**
 * Draws the common trash target used by inventory boards.
 *
 * @mangled CommonTrushDraw__Fiii
 * @address 0x00231060
 * @size 0x13C
 */
void CommonTrushDraw(int x, int y, int alpha);

/**
 * Tests whether the selected item may be thrown into the trash target.
 *
 * @mangled IsEnableTrushThrow__Fi
 * @address 0x002311A0
 * @size 0x1B4
 */
int IsEnableTrushThrow(int item_no);

/**
 * Draws the common money panel used by inventory menus.
 *
 * @mangled CommonMoneyBoardDraw__Fiiii
 * @address 0x00231360
 * @size 0x174
 */
void CommonMoneyBoardDraw(int x, int y, int money, int alpha);

/**
 * Returns the item in one cell of an inventory board page, or -1 when the cell or page holds none.
 *
 * @mangled SearchBoardNowPosItemExist__Fii
 * @address 0x002314E0
 * @size 0xE0
 */
int SearchBoardNowPosItemExist(int page, int cell);

/**
 * Finds an available inventory-board slot for an item kind.
 *
 * @mangled GetBoardSpace__FiPi
 * @address 0x002315C0
 * @size 0x200
 */
int GetBoardSpace(int item_no, int *page);

/**
 * Exchanges two entries in an item pack.
 *
 * @mangled SwapItem__FP9ITEM_PACKii
 * @address 0x002317C0
 * @size 0x38
 */
void SwapItem(ITEM_PACK *items, int first_pos, int second_pos);

/**
 * Compares two item identifiers in inventory sort order.
 *
 * @mangled CompItem__Fii
 * @address 0x00231800
 * @size 0x11C
 */
int CompItem(int first_item_no, int second_item_no);

/**
 * Performs one sorting pass over an item board.
 *
 * @mangled SeitonItemBoardSub__FP9ITEM_PACK
 * @address 0x00231920
 * @size 0x11C
 */
int SeitonItemBoardSub(ITEM_PACK *items);

/**
 * Sorts all entries in an item board.
 *
 * @mangled SeitonItemBoard__FP9ITEM_PACK
 * @address 0x00231A40
 * @size 0x88
 */
void SeitonItemBoard(ITEM_PACK *items);

/**
 * Compares two attachment records in inventory sort order.
 *
 * @mangled CompAttach__FP11ATTACH_LISTP11ATTACH_LIST
 * @address 0x00231B70
 * @size 0xFC
 */
int CompAttach(ATTACH_LIST *first, ATTACH_LIST *second);

/**
 * Performs one sorting pass over an attachment board.
 *
 * @mangled SeitonAttachBoardSub__FP11ATTACH_LIST__2
 * @address 0x00231C70
 * @size 0x11C
 */
int SeitonAttachBoardSub(ATTACH_LIST *attachments);

/**
 * Sorts all entries in an attachment board.
 *
 * @mangled SeitonAttachBoard__FP11ATTACH_LIST
 * @address 0x00231D90
 * @size 0x90
 */
int SeitonAttachBoard(ATTACH_LIST *attachments);

/**
 * Returns the inventory category containing an item identifier.
 *
 * @mangled WhatIsKindofItem__Fi
 * @address 0x00231E20
 * @size 0x64
 */
int WhatIsKindofItem(int item_no);

/**
 * Returns the character associated with a weapon definition, or -1 if unavailable.
 *
 * @mangled WhoIsWeaponEquip__Fi
 * @address 0x00231E90
 * @size 0x74
 */
int WhoIsWeaponEquip(int weapon_no);

/**
 * Returns the number of attachment holes on a weapon definition.
 *
 * @mangled GetWeaponHoleNum__Fi
 * @address 0x00231F10
 * @size 0xB4
 */
int GetWeaponHoleNum(int weapon_no);

/**
 * Returns the number of occupied attachment holes on a held weapon.
 *
 * @mangled GetNowWeaponAttachNum__FP11WEAPON_HAVE
 * @address 0x00231FD0
 * @size 0xA4
 */
int GetNowWeaponAttachNum(WEAPON_HAVE *weapon);

/**
 * Returns the ABS required for the held weapon to gain its next level.
 *
 * @mangled GetWeaponMaxExp__FP11WEAPON_HAVE
 * @address 0x00232080
 * @size 0xA4
 */
int GetWeaponMaxExp(WEAPON_HAVE *weapon);

/**
 * Counts the requested item in an item pack.
 *
 * @mangled GetNowItemNum__FsP9ITEM_PACK
 * @address 0x00232130
 * @size 0x8C
 */
int GetNowItemNum(short item_no, ITEM_PACK *items);

/**
 * Removes one used item from an item pack.
 *
 * @mangled DeleteItemAfterUseItem__FsP9ITEM_PACK
 * @address 0x002321C0
 * @size 0xCC
 */
void DeleteItemAfterUseItem(short item_no, ITEM_PACK *items);

/**
 * Counts the filled entries of an inventory board page and flags through the pointer when they overflow its room.
 *
 * @mangled GetNowModeMaxNum__FiPi
 * @address 0x00232290
 * @size 0x1FC
 */
int GetNowModeMaxNum(int page, int *overflow);

/**
 * Initializes a held-weapon record from its weapon definition.
 *
 * @mangled WepDataListToHaveCopy__FiP11WEAPON_HAVE
 * @address 0x00232490
 * @size 0x1B4
 */
void WepDataListToHaveCopy(int weapon_no, WEAPON_HAVE *weapon);

/**
 * Initializes an attachment record from its attachment definition.
 *
 * @mangled AttachDataListToHaveCopy__FiP11ATTACH_LIST
 * @address 0x00232650
 * @size 0x68
 */
void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment);

/**
 * Returns the initial quantity or volume for an item definition.
 *
 * @mangled ItemDataToHaveCopy__Fi
 * @address 0x002326C0
 * @size 0x104
 */
int ItemDataToHaveCopy(int item_no);

/**
 * Draws a texture across the complete menu display rectangle.
 *
 * @mangled DrawFullSizePicture__FP8CTextureiii
 * @address 0x002327D0
 * @size 0x54
 */
void DrawFullSizePicture(CTexture *texture, int x, int y, int alpha);
