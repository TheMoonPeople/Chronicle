#pragma once

#include "common.h"

namespace dc::game {

class CTexture;
struct RS_STACKDATA;
struct funcdata;
struct spRGBA;

/**
 * Gives every party member their default name.
 */
void GlobalNameInit();

/**
 * Opens the name-entry screen for a party member and starts reading its textures into a block.
 */
void InitNameRegist(int chara_no, int texture_block, u_long128 *buffer);

/**
 * Gives the name-entry screen's textures back and closes it.
 */
void ExitNameEnterFunc();

/**
 * Draws the name-entry screen.
 */
void NameEnterDraw();

/**
 * Moves the cursor across the keyboard, enters a character and reports 1 once the screen has closed.
 */
s32 NameEnterKey();

/**
 * Gives one party member their default name for the chosen language.
 */
void NameDefaultSet(int chara_no);

/**
 * Draws a party member's name on the character-select page.
 */
void CharaSelectNameDraw2(int x, int y, short *name, CTexture **textures, int blend_mode);

/**
 * Draws a party member's name centred on the save board, with a shadow and a top-to-bottom gradient.
 */
void DrawSaveBoardCharaName2(int x, int y, short *name, CTexture **textures, spRGBA top_color, spRGBA bottom_color);

/**
 * Counts the characters of a party member's name, up to ten, or gives zero outside the party.
 */
int GetMsgLengthCharaName(int chara_no);

/**
 * Opens the storybook that begins the game, reading it into the buffer and two texture blocks.
 */
void InitOpeningBook(u_long128 *buffer, int *tex_blocks);

/**
 * Turns the storybook's pages with the pad, then runs name entry; nonzero once it has closed.
 */
int OpeningBookKey();

/**
 * Draws the storybook page by page.
 */
void OpeningBookDraw();

/**
 * Reports a script runtime error and exits.
 */
void runerror(const char *message);

/**
 * Reports a script stack overflow and exits.
 */
void stkoverflow();

/**
 * Gives a script value's integer, or reports a runtime error when it is not one.
 */
int chk_int(RS_STACKDATA data, funcdata *function);

/**
 * Reports whether a script value counts as true.
 */
int is_true(RS_STACKDATA data);

/**
 * Reports a script division by zero and exits.
 */
void divby0error();

/**
 * Reports a script modulo by zero and exits.
 */
void modby0error();

/**
 * Prints script values to standard output.
 */
void print(RS_STACKDATA *data, int count);

/**
 * The name-entry screen's frame, tabs and cursor.
 */
extern CTexture *NameTemp;

/**
 * Keyboard key of each symbol, indexing the symbol keyboard's characters.
 */
extern s8 menu_kigoutbl[40];

/**
 * Character code of each key on the accented-character keyboard, per language and row; -2 ends a row.
 */
extern s16 menu_euro_codetbl[5][2][13];

/**
 * How many rows of the accented-character keyboard each language uses.
 */
extern s8 euro_code_linelimmit[5];

} // namespace dc::game
