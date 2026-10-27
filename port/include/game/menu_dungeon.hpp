#pragma once

#include "common.h"

#include "menu_draw.hpp"

namespace dc::game {

/**
 * Returns how much a weapon's attachments raise one of its values.
 */
int GetWeaponAttachStatusUp(WEAPON_HAVE *weapon, int stat);

/**
 * Recomputes a held weapon's values from its attachments.
 */
void SetWeaponAttachStatus(WEAPON_HAVE *result);

/**
 * Applies a weapon's attachments to its stats and writes the result into a
 * second weapon record.
 */
void WeaponAllValueSet(WEAPON_HAVE *weapon, WEAPON_HAVE *result, int full);

/**
 * Refills the attachment in a dungeon inventory slot from the attachment data
 * table, and raises its stat bonus by the level, clamped to 1-3. Returns -1
 * when the item or the slot's attachment is out of the attachment range.
 */
int SetAttachMentValue(int item_no, int slot, short level, ATTACH_LIST *unused);

/**
 * Returns the stat value an attachment adds, for its message line.
 */
int GetAttachVolumeForMsg(ATTACH_LIST *attach);

/**
 * Opens the dungeon entrance menu for a dungeon and floor.
 */
int InitDunEnterMenu(int texture_block, int dungeon, int requested_floor);

/**
 * Runs one frame of the dungeon entrance menu, and returns its result once it closes.
 */
int DunEnterMenuLoop();

/**
 * Opens the character change menu.
 */
void StartQuickChange(u_long128 *buffer, int texture_block, int *positions, int mode);

/**
 * Runs one frame of the character change menu, and returns its result.
 */
int CharaChangeLoop();

/**
 * Handles key input on the character change menu.
 */
int CharaChangeKey();

/**
 * Draws the character change menu.
 */
void CharaChangeDraw();

/**
 * Starts reading the model of the active item.
 */
int DngActItemModelReadStart(u_long128 *buffer);

/**
 * Builds the active item's model once its file has been read.
 */
int DngActItemModelBuild(int wait);

/**
 * Copies the quick-slot items' icons into the reserved item texture page, and
 * returns 1, or -1 without an item pack.
 */
int DngActiveItemTextureCopy();

/**
 * Copies the icons of the leader's equipped and default weapons into the
 * reserved item texture page, and returns 1.
 */
int DngActiveWeaponTextureCopy();

/**
 * Returns the message number of a held weapon's name.
 */
s32 GetWeaponMsgNo(WEAPON_HAVE *weapon);

/**
 * Returns the message number of an item's name.
 */
int GetWeaponMsgNo2(int item_no);

/**
 * Draws the attachments set in a weapon's holes.
 */
void DrawWepAttach(int x, int y, WEAPON_HAVE *weapon, int selected, int alpha);

/**
 * Tells whether the player has an Atla piece, held or placed.
 */
int GetAtraTipNowHave(int tip_no, int georama_no);

/**
 * Converts a value into the whole number the menus display for it.
 */
int GetDispVolumeForFloat(float volume);

/**
 * Starts reading the model the item preview shows.
 */
int InitItemPolygonView(int item_no, u_long128 *buffer);

/**
 * Draws the item preview model.
 */
void DrawItemPolygonView();

/**
 * Puts the debug item menu's cursor and text overlay at their starting state. PAL only.
 */
void InitDebugItemGet();

/**
 * Handles key input on the debug item menu.
 */
int DebugItemGetKey();

/**
 * Draws the debug item menu.
 */
void DebugItemGetDraw();

/**
 * Frame image of the character-change window.
 */
extern char chara_change_frame_image[];

/**
 * Board texture of the dungeon entrance menu ("dunenter").
 */
extern CTexture *DunLogBoard;

// Port additions.

/**
 * Chooses what the dungeon's next quick-change menu shows: the party (SELECT, as retail) or the
 * element picker (D-pad Up), whose cells are the elements the equipped weapon can take.
 */
void QuickChangeOpenElements(bool elements);

/**
 * Whether the equipped weapon has more than one element to pick between, so the picker would
 * offer a choice.
 */
bool ElementPickerHasChoice();

} // namespace dc::game
