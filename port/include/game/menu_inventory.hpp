#pragma once

#include "common.h"

namespace dc::game {

class CTexture;
struct ATTACH_LIST;
struct IHAVEITEM;
struct ITEM_PACK;
struct WEAPON_HAVE;

/**
 * Texture sheet used to draw personal inventory boards.
 */
extern CTexture *PerBoardTex;

/**
 * Places or exchanges a weapon selected on the personal inventory board.
 */
int PersonalBoardWeaponPush(IHAVEITEM *item, int board_pos);

/**
 * Places or exchanges an attachment selected on the personal inventory board.
 */
int PersonalBoardAttachPush(IHAVEITEM *item, int board_pos);

/**
 * Gets or exchanges the item at the selected personal-board position.
 */
int PersonalBoardItemGetorSwap(int board_pos);

/**
 * Cancels the current personal-board item operation.
 */
void PersonalBoardItemCancel();

/**
 * Returns the maximum visible entry count for a personal-board mode.
 */
int PersonalRetMax(int board_mode);

/**
 * Draws the entries in the active personal inventory board.
 */
void DrawPersonalBoard(int x, int y, int board_mode, int alpha, int unused);

/**
 * Draws the icons of one personal board page, five to a row, clipped to the board's top and bottom.
 */
void CommonIconDraw(int page, int count, int x, int y, int top, int bottom, int alpha);

/**
 * Draws the frame surrounding a personal inventory board.
 */
void PersonalBoardDrawWaku(int x, int y, CTexture *texture, int alpha);

/**
 * Draws the option area associated with a personal inventory board.
 */
void PersonalBoardOptionDraw(int board_mode, int count, int x, int y, CTexture *texture, int alpha);

/**
 * Draws the tabs above a personal inventory board, the selected one lit, and in
 * the village board the village tabs and the selected village's name.
 */
void PersonalBoardTagDraw(int tag, int x, int y, CTexture *texture, int shift, int alpha);

/**
 * Draws and updates the personal-board scroll bar.
 */
void PersonalBoardScrlBarDraw(int count, int x, int y, float &scroll, unsigned char top_row, CTexture *texture, int alpha);

/**
 * Draws the number of entries on a personal inventory board and its capacity,
 * the capacity picked out once the board is over it.
 */
void PersonalBoardMaxDraw(int num, int x, int y, CTexture *texture, int alpha);

/**
 * Draws the rows of a personal board's slot grid down to the clip bottom, with a divider bar above the third row
 * (mark 1) or above every other row (mark 2).
 */
void DrawPerBoardDraw(int mark, int count, int x, int y, int top, int bottom, CTexture *texture, int alpha);

/**
 * Draws the common trash target used by inventory boards.
 */
void CommonTrushDraw(int x, int y, int alpha);

/**
 * Tests whether the selected item may be thrown into the trash target.
 */
int IsEnableTrushThrow(int item_no);

/**
 * Draws the common money panel used by inventory menus.
 */
void CommonMoneyBoardDraw(int x, int y, int money, int alpha);

/**
 * Returns the item in one cell of an inventory board page, or -1 when the cell or page holds none.
 */
int SearchBoardNowPosItemExist(int page, int cell);

/**
 * Finds an available inventory-board slot for an item kind.
 */
int GetBoardSpace(int item_no, int *page);

/**
 * Exchanges two entries in an item pack.
 */
void SwapItem(ITEM_PACK *items, int first_pos, int second_pos);

/**
 * Compares two item identifiers in inventory sort order.
 */
int CompItem(int first_item_no, int second_item_no);

/**
 * Performs one sorting pass over an item board.
 */
int SeitonItemBoardSub(ITEM_PACK *items);

/**
 * Sorts all entries in an item board.
 */
void SeitonItemBoard(ITEM_PACK *items);

/**
 * Compares two attachment records in inventory sort order.
 */
int CompAttach(ATTACH_LIST *first, ATTACH_LIST *second);

/**
 * Performs one sorting pass over an attachment board.
 */
int SeitonAttachBoardSub(ATTACH_LIST *attachments);

/**
 * Sorts all entries in an attachment board.
 */
int SeitonAttachBoard(ATTACH_LIST *attachments);

/**
 * Returns the inventory category containing an item identifier.
 */
int WhatIsKindofItem(int item_no);

/**
 * Returns the character associated with a weapon definition, or -1 if unavailable.
 */
int WhoIsWeaponEquip(int weapon_no);

/**
 * Returns the number of attachment holes on a weapon definition.
 */
int GetWeaponHoleNum(int weapon_no);

/**
 * Returns the number of occupied attachment holes on a held weapon.
 */
int GetNowWeaponAttachNum(WEAPON_HAVE *weapon);

/**
 * Returns the ABS required for the held weapon to gain its next level.
 */
int GetWeaponMaxExp(WEAPON_HAVE *weapon);

/**
 * Counts the requested item in an item pack.
 */
int GetNowItemNum(short item_no, ITEM_PACK *items);

/**
 * Removes one used item from an item pack.
 */
void DeleteItemAfterUseItem(short item_no, ITEM_PACK *items);

/**
 * Counts the filled entries of an inventory board page and flags through the pointer when they overflow its room.
 */
int GetNowModeMaxNum(int page, int *overflow);

/**
 * Initializes a held-weapon record from its weapon definition.
 */
void WepDataListToHaveCopy(int weapon_no, WEAPON_HAVE *weapon);

/**
 * Initializes an attachment record from its attachment definition.
 */
void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment);

/**
 * Returns the initial quantity or volume for an item definition.
 */
int ItemDataToHaveCopy(int item_no);

/**
 * Draws a texture across the complete menu display rectangle.
 */
void DrawFullSizePicture(CTexture *texture, int x, int y, int alpha);

} // namespace dc::game
