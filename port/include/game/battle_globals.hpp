#pragma once

#include "common.h"

/**
 * What CheckName reports.
 */
// clang-format off
enum NameCheckResult {
    NAME_CHECK_REJECTED = 0, /**< Rejected and cleared. */
    NAME_CHECK_OK       = 1, /**< Accepted. */
    NAME_CHECK_EMPTY    = 2, /**< Blank; the default name is restored. */
};

// clang-format on

/**
 * Steps of the opening book, as OPENING_BOOK::step holds them.
 */
// clang-format off
enum OpeningBookStep {
    OPENING_BOOK_LOAD       = 0, /**< Loading and fading the cover. */
    OPENING_BOOK_TEXT_IN    = 1, /**< Text fading in. */
    OPENING_BOOK_WAIT       = 2, /**< Waiting for cross. */
    OPENING_BOOK_TEXT_OUT   = 3, /**< Text fading out. */
    OPENING_BOOK_FINISH     = 4, /**< Fading out before name entry. */
    OPENING_BOOK_NAME_ENTRY = 5, /**< Name entry. */
    OPENING_BOOK_UNK_6      = 6, /**< Drawn but never set. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CTexture;
struct RS_STACKDATA;
struct funcdata;
struct spRGBA;

/**
 * State of the storybook that plays before the game begins.
 */
struct OPENING_BOOK {
    s16 open;           /**< Whether the storybook's pages have been read and it draws. */
    s16 tex_block;      /**< Texture block the storybook's pages are loaded into. */
    s16 name_tex_block; /**< Texture block handed to the name-entry screen that follows the storybook. */
    s16 page;           /**< Index of the page being shown; its message is page plus 100. */
    s16 step;           /**< Which part of the opening the storybook is showing. @see OpeningBookStep. */
    s16 fade;           /**< Strength of the full-screen fade drawn over the storybook. */
    s16 text_alpha;     /**< Alpha the page text fades in and out through. */
};

STATIC_ASSERT(sizeof(OPENING_BOOK) == 0xE);

/**
 * Gives every party member their default name.
 *
 * @mangled GlobalNameInit__Fv
 * @address 0x238450
 * @size 0x48
 */
void GlobalNameInit();

/**
 * Opens the name-entry screen and reads its textures.
 *
 * @mangled InitNameRegist__FiiP1
 * @address 0x2384A0
 * @size 0x190
 */
void InitNameRegist(int chara_no, int texture_block, u_long128 *buffer);

/**
 * Gives the name-entry screen's textures back and closes it.
 *
 * @mangled ExitNameEnterFunc__Fv
 * @address 0x238630
 * @size 0x68
 */
void ExitNameEnterFunc();

/**
 * Gives the texture and cell one name character draws from.
 *
 * @mangled GetNameTextureInfo__FPP8CTextureiRiRi
 * @address 0x2386A0
 * @size 0xBC
 */
CTexture *GetNameTextureInfo(CTexture **textures, int char_code, int &cell_x, int &cell_y);

/**
 * Draws the name being entered; the character argument goes unused.
 *
 * @mangled DrawCharaName__Fiiiii
 * @address 0x238760
 * @size 0x118
 */
void DrawCharaName(int chara_no, int x, int y, int brightness, int blend_mode);

/**
 * Draws the frame around the name being entered, bobbing it with a sine.
 *
 * @mangled DrawNameRegiWaku__Fiiiii
 * @address 0x238880
 * @size 0x1F0
 */
void DrawNameRegiWaku(int x, int y, int size, int brightness, int blend_mode);

/**
 * Draws the name being entered above the keyboard.
 *
 * @mangled DrawCharaNameUp__Fiiii
 * @address 0x238A70
 * @size 0x628
 */
void DrawCharaNameUp(int x, int y, int brightness, int blend_mode);

/**
 * Draws the accented-character rows of the alphabet keyboard for one European language. PAL only.
 *
 * @mangled DrawEuroSpecialFont__Fiiiii
 * @address 0x23F4A0
 * @size 0x25C
 */
void DrawEuroSpecialFont(int x, int y, int language, int brightness, int blend_mode);

/**
 * Reports whether the entered name may be used.
 *
 * @mangled CheckName__Fv
 * @address 0x239A40
 * @size 0x160
 */
int CheckName();

/**
 * Draws the name-entry screen.
 *
 * @mangled NameEnterDraw__Fv
 * @address 0x239BA0
 * @size 0xC9C
 */
void NameEnterDraw();

/**
 * Moves the cursor across the keyboard, enters a character and reports a transition.
 *
 * @mangled NameEnterKey__Fv
 * @address 0x23A840
 * @size 0x1F28
 */
s32 NameEnterKey();

/**
 * Gives one party member their default name for the chosen language.
 *
 * @mangled NameDefaultSet__Fi
 * @address 0x23C770
 * @size 0x110
 */
void NameDefaultSet(int chara_no);

/**
 * Draws a party member's name on the character-select page.
 *
 * @mangled CharaSelectNameDraw2__FiiPsPP8CTexturei
 * @address 0x23C900
 * @size 0x250
 */
void CharaSelectNameDraw2(int x, int y, short *name, CTexture **textures, int blend_mode);

/**
 * Draws a party member's name on the save board, in a gradient.
 *
 * @mangled DrawSaveBoardCharaName2__FiiPsPP8CTexture6spRGBA6spRGBA
 * @address 0x23CB50
 * @size 0x284
 */
void DrawSaveBoardCharaName2(int x, int y, short *name, CTexture **textures, spRGBA top_color, spRGBA bottom_color);

/**
 * Gives how wide a party member's name draws.
 *
 * @mangled GetMsgLengthCharaName__Fi
 * @address 0x23CDE0
 * @size 0x78
 * Counts the characters of a party member's name, up to ten, or gives zero for
 * an index outside the party.
 */
int GetMsgLengthCharaName(int chara_no);

/**
 * Opens the storybook that begins the game.
 *
 * @mangled InitOpeningBook__FP1Pi
 * @address 0x23CE60
 * @size 0xB0
 */
void InitOpeningBook(void /* CW back-ref target unresolved */ *buffer, int *tex_blocks);

/**
 * Turns the storybook's pages with the pad.
 *
 * @mangled OpeningBookKey__Fv
 * @address 0x23CF10
 * @size 0x664
 */
int OpeningBookKey();

/**
 * Draws the storybook page by page.
 *
 * @mangled OpeningBookDraw__Fv
 * @address 0x23D580
 * @size 0x134
 */
void OpeningBookDraw();

/**
 * @mangled runerror__FPCc
 * @address 0x23D6C0
 * @size 0x44
 * @unknownret
 */
void runerror(const char *message);

/**
 * @mangled stkoverflow__Fv
 * @address 0x23D710
 * @size 0x28
 * @unknownret
 */
void stkoverflow();

/**
 * @mangled chk_int__F12RS_STACKDATAP8funcdata
 * @address 0x23D740
 * @size 0x68
 * @unknownret
 */
int chk_int(RS_STACKDATA data, funcdata *function);

/**
 * @mangled is_true__F12RS_STACKDATA
 * @address 0x23D7B0
 * @size 0x3C
 * @unknownret
 */
int is_true(RS_STACKDATA data);

/**
 * @mangled divby0error__Fv
 * @address 0x23D7F0
 * @size 0x28
 * @unknownret
 */
void divby0error();

/**
 * @mangled modby0error__Fv
 * @address 0x23D820
 * @size 0x28
 * @unknownret
 */
void modby0error();

/**
 * @mangled print__FP12RS_STACKDATAi
 * @address 0x23D850
 * @size 0xE8
 * @unknownret
 */
void print(RS_STACKDATA *data, int count);

/**
 * Packed texture file of the name-entry screen.
 */
extern char NameEntryTextureFile[];

/**
 * Frame image descriptor of the name-entry background.
 */
extern char NameEntryImageDescriptor[];

/**
 * Frame texture shown around the keyboard.
 */
extern char NameEntryFrameTexture[];

/**
 * Texture name of the name-entry frame.
 */
extern char NameEntryTempTexture[];

/**
 * Texture name of the hiragana keyboard.
 */
extern char NameEntryHiraganaTexture[];

/**
 * Texture name of the katakana keyboard.
 */
extern char NameEntryKatakanaTexture[];

/**
 * Texture name of the alphabet keyboard.
 */
extern char NameEntryAlphabetTexture[];

/**
 * Texture name of the party member faces.
 */
extern char NameEntryFaceTexture[];

/**
 * First message file of the name-entry screen.
 */
extern char NameEntryMessageFile[];

/**
 * Second message file of the name-entry screen.
 */
extern char NameEntryMessageFile2[];
