#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 15

#include "battle_globals.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "rect.hpp"
#include "runscript.hpp"
#include "savedata.hpp"
#include "texture.hpp"

/**
 * Keyboards of the name-entry screen, as NAME_SELECT::input_mode holds them.
 */
// clang-format off
enum NameInputMode {
    NAME_INPUT_KATAKANA = 0, /**< Katakana. */
    NAME_INPUT_HIRAGANA = 1, /**< Hiragana. */
    NAME_INPUT_ALPHABET = 2, /**< The alphabet. */
    NAME_INPUT_SYMBOL   = 3, /**< Symbols. */
};

// clang-format on

/**
 * Parts of the name-entry screen the cursor can be in, as NAME_SELECT::area holds them.
 */
// clang-format off
enum NameSelectArea {
    NAME_AREA_KEYBOARD     = 4, /**< The keyboard's keys and voicing column. */
    NAME_AREA_TABS         = 5, /**< The tabs above the keyboard. */
    NAME_AREA_HELP         = 6, /**< The help message. */
    NAME_AREA_CONFIRM      = 7, /**< The prompt confirming the name. */
    NAME_AREA_EMPTY_NOTICE = 8, /**< The notice that an empty name became the default name. */
};

// clang-format on

/**
 * What the name-entry screen is doing, as NAME_SELECT::state holds it.
 */
// clang-format off
enum NameSelectState {
    NAME_STATE_IDLE      = 0, /**< Taking input. */
    NAME_STATE_FADE_IN   = 1, /**< Fading in. */
    NAME_STATE_FADE_OUT  = 2, /**< Fading out. */
    NAME_STATE_EXIT      = 3, /**< Closing. */
    NAME_STATE_TAB_FLASH = 6, /**< Flashing the tab that was pressed. */
    NAME_STATE_HELP      = 8, /**< Showing the help message. */
};

// clang-format on

/**
 * Tabs of the name-entry screen, as NAME_SELECT::cursor holds them while the cursor is on the tabs.
 */
// clang-format off
enum NameSelectTab {
    NAME_TAB_KATAKANA     = 0,  /**< Shows the katakana keyboard. */
    NAME_TAB_HIRAGANA     = 1,  /**< Shows the hiragana keyboard. */
    NAME_TAB_ALPHABET     = 2,  /**< Shows the alphabet keyboard. */
    NAME_TAB_SYMBOL       = 3,  /**< Shows the symbol keyboard. */
    NAME_TAB_DEFAULT_NAME = 4,  /**< Restores the default name. */
    NAME_TAB_OK           = 5,  /**< Accepts the name. */
    NAME_TAB_LEFT         = 6,  /**< Moves the name's cursor left. */
    NAME_TAB_RIGHT        = 7,  /**< Moves the name's cursor right. */
    NAME_TAB_DELETE       = 8,  /**< Deletes the character under the name's cursor. */
    NAME_TAB_INSERT       = 9,  /**< Inserts a space under the name's cursor. */
    NAME_TAB_HELP         = 10, /**< Shows the help message. */
};

// clang-format on

/**
 * Actions the name-entry screen's input asks for in one frame.
 */
// clang-format off
enum NameEnterAction {
    NAME_ACTION_NONE           = -1,  /**< Nothing. */
    NAME_ACTION_DECIDE         = 49,  /**< Checks the name and asks to confirm it. */
    NAME_ACTION_FINISH         = 50,  /**< Accepts the name and fades out. */
    NAME_ACTION_INPUT          = 100, /**< Enters the key under the cursor. */
    NAME_ACTION_DELETE         = 200, /**< Deletes the character under the name's cursor. */
    NAME_ACTION_BACKSPACE      = 250, /**< Blanks the character before the name's cursor. */
    NAME_ACTION_INSERT         = 300, /**< Inserts a space under the name's cursor. */
    NAME_ACTION_NEXT_MODE      = 400, /**< Shows the next keyboard. */
    NAME_ACTION_PREV_MODE      = 450, /**< Shows the previous keyboard. */
    NAME_ACTION_LEFT           = 500, /**< Moves the name's cursor left. */
    NAME_ACTION_RIGHT          = 600, /**< Moves the name's cursor right. */
    NAME_ACTION_DEFAULT_NAME   = 700, /**< Restores the default name. */
    NAME_ACTION_HELP           = 800, /**< Shows the help message. */
    NAME_ACTION_TOGGLE_VOICING = 900, /**< Toggles the voicing or case of the character under the name's cursor; never requested. */
};

// clang-format on

/**
 * Holds the state of the name-entry screen.
 */
struct NAME_SELECT {
    s16   chara_no;      /**< The party member being named. */
    s16   area;          /**< The part of the screen the cursor is in. @see NameSelectArea. */
    s16   name_pos;      /**< The position in the name the next character goes to. */
    s16   side_row;      /**< The row of the voicing column the cursor is on, or 0 when it is on the keyboard. */
    s16   pushed_tab;    /**< The tab that was last pressed. @see NameSelectTab. */
    s16   input_mode;    /**< The keyboard being shown. @see NameInputMode. */
    s32   cursor;        /**< The key or tab the cursor is on. @see NameSelectTab. */
    s16   state;         /**< What the screen is doing: fading, flashing a tab or leaving. @see NameSelectState. */
    s32   state_count;   /**< How many frames the state has lasted. */
    float cursor_x;      /**< Where the hand cursor is drawn across the screen. */
    float cursor_y;      /**< Where the hand cursor is drawn down the screen. */
    s32   frame;         /**< How many frames the screen has been drawn, for the cursor's sway. */
    s16   language;      /**< The language the menus are in. */
    s16   texture_block; /**< The texture block the screen's textures are read into. */
    s16   loaded;        /**< Whether the screen's textures have been read. */
};

STATIC_ASSERT(sizeof(NAME_SELECT) == 0x2C);

/** The state of the name-entry screen. */
NAME_SELECT NameSelect;

/** The name currently being edited. */
extern "C" {
s16 *CharaName;
}

/** The face of the party member being named. */
CTexture *CharaFace;

/** The name-entry screen's frame, tabs and cursor. */
CTexture *NameTemp;

/** The hiragana keyboard's characters. */
CTexture *HiraTex;

/** The katakana keyboard's characters. */
CTexture *KataTex;

/** The alphabet and symbol keyboards' characters. */
CTexture *AlphaTex;

/** The accented European characters' keyboard. */
CTexture *EuroTex;

/** How many keys each keyboard has in a row. */
s8 InputModeOrikaeshi[4] = {10, 10, 13, 10};

/** How far the cursor moves across and down each keyboard, per key. */
s8 InputModeMovetbl[4][2] = {
    {0x26, 0x1A},
    {0x26, 0x1A},
    {0x22, 0x1A},
    {0x26, 0x1A}
};

#include "gameutil.hpp"
#include "snd.hpp"

/**
 * Gives every party member their default name.
 *
 * @mangled GlobalNameInit__Fv
 * @address 0x238450
 * @size 0x48
 */
void GlobalNameInit() {
    for (int chara_no = 0; chara_no < 6; chara_no++) {
        NameDefaultSet(chara_no);
    }
}

/**
 * Opens the name-entry screen and reads its textures.
 *
 * @mangled InitNameRegist__FiiP1
 * @address 0x2384A0
 * @size 0x190
 */
void InitNameRegist(int chara_no, int texture_block, u_long128 *buffer) {
    StartReadBG();

    if (buffer == NULL) {
        buffer = (u_long128 *) read_buffer;
    } else {
        buffer = (u_long128 *) buffer;
    }

    LoadFileBGMenuData("nameregi.pak", MenuCalcBufAlignment(buffer));
    NameSelect.language = GetMenuLangFlag();
    GamePad.MenuModeOn(0x78);
    GamePad.SetAutoRepeat(PAD_DPAD, 30, 9);

    NameSelect.chara_no = chara_no;
    NameSelect.area = NAME_AREA_KEYBOARD;
    NameSelect.cursor = 0;

    switch (NameSelect.language) {
        case LANG_JAPANESE:
            NameSelect.input_mode = NAME_INPUT_KATAKANA;
            break;
        default:
            NameSelect.input_mode = NAME_INPUT_ALPHABET;
            break;
    }

    NameSelect.side_row = 0;
    NameSelect.state = NAME_STATE_FADE_IN;
    NameSelect.state_count = 0;
    NameSelect.loaded = false;
    NameSelect.texture_block = texture_block;
    NameSelect.cursor_x = 100.0f;
    NameSelect.cursor_y = 242.0f;
    NameSelect.frame = 0;
    CharaName = SaveData->GetCharaName(NameSelect.chara_no);
    NameDefaultSet(NameSelect.chara_no);
    NameSelect.name_pos = 0;
}

/**
 * Gives the name-entry screen's textures back and closes it.
 *
 * @mangled ExitNameEnterFunc__Fv
 * @address 0x238630
 * @size 0x68
 */
void ExitNameEnterFunc() {
    int texture_blocks[2] = {NameSelect.texture_block, -1};
    MenuTextureDelete(texture_blocks);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

/**
 * Gives the texture and cell one name character draws from.
 *
 * @mangled GetNameTextureInfo__FPP8CTextureiRiRi
 * @address 0x2386A0
 * @size 0xBC
 */
CTexture *GetNameTextureInfo(CTexture **textures, int char_code, int &cell_x, int &cell_y) {
    CTexture *texture;

    if (char_code == 0) {
        char_code = 0xE6;
        texture = textures[0];
    } else if (char_code < 0x52) {
        char_code -= 1;
        texture = textures[1];
    } else if (char_code < 0xA2) {
        char_code -= 0x52;
        texture = textures[2];
    } else if (char_code < 0x104) {
        char_code -= 0xA2;
        texture = textures[0];
    } else {
        char_code -= 0x104;
        texture = textures[3];
    }

    cell_x = (char_code % 10) * 0x16;
    cell_y = (char_code / 10) * 0x16;
    return texture;
}

/**
 * Draws a party member's name.
 *
 * @mangled DrawCharaName__Fiiiii
 * @address 0x238760
 * @size 0x118
 */
void DrawCharaName(int chara_no, int x, int y, int brightness, int blend_mode) {
    int draw_x = x;
    CTexture *textures[4] = {AlphaTex, KataTex, HiraTex, EuroTex};

    for (int slot = 0; slot < 10; slot++) {
        int       cell_x;
        int       cell_y;
        CTexture *texture = GetNameTextureInfo(textures, CharaName[slot], cell_x, cell_y);
        DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 22, 22), CRect_i_(cell_x, cell_y, 22, 22), brightness, brightness, brightness, blend_mode);
        draw_x += 22;
    }
}

/**
 * Draws the four corners of the frame around the name being entered, pulling them inward as the frame counter cycles.
 *
 * @mangled DrawNameRegiWaku__Fiiiii
 * @address 0x238880
 * @size 0x1F0
 */
void DrawNameRegiWaku(int x, int y, int size, int brightness, int blend_mode) {
    float inset = 0.1f * (NameSelect.frame % 29);
    int   edge[4];

    edge[0] = x + inset;
    edge[1] = y + inset;
    edge[2] = x + size - inset;
    edge[3] = y + size - inset;
    s16 corner[4][2] = {
        {edge[0], edge[1]},
        {edge[2], edge[1]},
        {edge[0], edge[3]},
        {edge[2], edge[3]}
    };
    s16 cell[4][2] = {
        {480, 296},
        {496, 296},
        {480, 312},
        {496, 312}
    };

    for (int corner_no = 0; corner_no < 4; corner_no++) {
        DrawMenu2DSprite(NameTemp, CRect_i_(corner[corner_no][0], corner[corner_no][1], 12, 12), CRect_i_(cell[corner_no][0], cell[corner_no][1], 16, 16), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);
    }
}

/**
 * Draws the top of the name-entry screen: the party member's face and title, the name being entered and the cursor over it.
 *
 * @mangled DrawCharaNameUp__Fiiii
 * @address 0x238A70
 * @size 0x628
 */
void DrawCharaNameUp(int x, int y, int brightness, int blend_mode) {
    int left;
    int top;
    int chara_no = NameSelect.chara_no;
    left = x - 2;
    top = y + 14;
    int face_x = 0;
    int face_y = chara_no * 106;
    int name_length;

    if (chara_no > 2) {
        face_x = 106;
        face_y = (chara_no - 3) * 106;
    }

    DrawMenu2DSprite(CharaFace, CRect_i_(left, top, 88, 88), CRect_i_(face_x, face_y, 106, 106), blend_mode);
    DrawMenuHelpWindow(TexManager.GetTexture("window", -1), -1, left + 102, top + 16, 8.6f, 1.0f, blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 97, top + 16, 26, 23), CRect_i_(0, 256, 26, 24), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 123, top + 16, 210, 23), CRect_i_(26, 256, 172, 24), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 333, top + 16, 26, 23), CRect_i_(198, 256, 26, 24), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);

    MenuTextureReload(AtoraNameMes.tex_block);
    int   message_length = AtoraNameMes.GetMesLen_system(chara_no - 1220);
    float char_width[7] = {18.0f, 12.0f, 12.0f, 12.0f, 12.0f, 12.0f, 12.0f};
    float title_char_width = char_width[NameSelect.language];
    AtoraNameMes.line_pos[0].x = 290.0f - title_char_width * (message_length / 2.0f);
    AtoraNameMes.line_pos[0].y = 82;
    AtoraNameMes.edge_alpha = blend_mode;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();
    MenuTextureReload(NameSelect.texture_block);

    if (NameSelect.area < NAME_AREA_HELP) {
        CRect_i_ destination(left + 89, top + 25, 24, 24);
        CRect_i_ source(24, 472, 24, 24);
        DrawMenu2DSprite(NameTemp, destination, source, blend_mode);
        source.x += 25;
        destination.x = left + 331;
        DrawMenu2DSprite(NameTemp, destination, source, blend_mode);
    }

    left = x + 120;
    top = y + 58;
    DrawCharaName(chara_no, left, top, brightness, blend_mode);

    left = x + 122;
    top = y + 82;

    if (NameSelect.area == NAME_AREA_CONFIRM) {
        for (name_length = 10; name_length > 0; name_length--) {
            if (CharaName[name_length - 1] != 0 && CharaName[name_length - 1] != 0xE6) {
                break;
            }
        }
    }

    for (int slot = 0; slot < 10; slot++) {
        int underline_brightness = brightness;

        if (NameSelect.area == NAME_AREA_CONFIRM && name_length <= slot) {
            underline_brightness = 0x38;
        }

        DrawMenu2DSprite(NameTemp, CRect_i_(left, top, 18, 2), CRect_i_(470, 328, 18, 2), (u8) underline_brightness, (u8) underline_brightness, (u8) underline_brightness, blend_mode);
        left += 22;
    }

    int cursor_slot = NameSelect.name_pos;

    if (cursor_slot >= 10) {
        cursor_slot = 9;
    }

    left = x + 110 + cursor_slot * 22;
    top = y + 48;

    switch (NameSelect.state) {
        case NAME_STATE_FADE_IN:
        case NAME_STATE_FADE_OUT:
            break;
        default:
            if (NameSelect.area < NAME_AREA_HELP) {
                DrawNameRegiWaku(left, top, 30, brightness, blend_mode);
                int bob_frame = NameSelect.frame - 15;

                if (NameSelect.area >= NAME_AREA_HELP) {
                    bob_frame = 0;
                }

                top = (top - 20) + 4.0f * sinf(0.20943952f * (bob_frame % 31));
                DrawMenu2DSprite(NameTemp, CRect_i_(left + 20, top, 24, 24), CRect_i_(488, 328, 24, 24), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);
            }

            break;
    }
}

/** Keyboard key of each symbol, indexing the symbol keyboard's characters. */
s8 menu_kigoutbl[40] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 17, 18, 19, 20, 21, 41, 42, 43, 44, 16, 16, 16, 16, 16, 16, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37};

/** Character code of each key on the accented-character keyboard, per language and row; -2 ends a row. */
s16 menu_euro_codetbl[5][2][13] = {
    {
     {260, 261, 262, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     {260, 261, 262, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1},
     },
    {
     {260, 290, 270, 300, 280, 265, 295, 275, 305, 285, -2, -1, -1},
     {262, 292, 282, 267, 297, 287, 291, 296, 312, 313, -2, -1, -1},
     },
    {
     {260, 300, 280, 265, 305, 285, 320, -2, -1, -1, -1, -1, -1},
     {-1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1},
     },
    {
     {261, 291, 271, 301, 281, 266, 296, 276, 306, 286, -2, -1, -1},
     {262, 292, 272, 302, 282, 267, 297, 277, 307, 285, -1, -1, 0},
     },
    {
     {261, 291, 271, 301, 281, 266, 296, 276, 306, 286, 280, 285, -2},
     {-1, -1, -1, -2, -1, -1, -2, -1, -1, -1, -1, -1, -1},
     },
};

/** How many rows of the accented-character keyboard each language uses. */
s8 euro_code_linelimmit[5] = {2, 2, 1, 2, 1};

void DrawEuroSpecialFont(int x, int y, int language, int brightness, int blend_mode) {
    int key_x;
    int key_y;
    int cell_x;
    int cell_y;
    int row;
    int column;

    EuroTex = TexManager.GetTexture("euro", -1);

    for (row = 0; row < 2; row++) {
        for (column = 0; column < 13; column++) {
            int key = column + row * 13;

            if (key >= 26) {
                break;
            }

            int char_code = menu_euro_codetbl[language][0][key] - 0x104;

            if (char_code == -2) {
                break;
            }

            if (char_code >= 0) {
                cell_x = (char_code % 10) * 22;
                cell_y = (char_code / 10) * 22;
                key_x = x + column * 34;
                key_y = y + row * 26;
                DrawMenu2DSprite(EuroTex, CRect_i_(key_x + 1, key_y + 1, 22, 22), CRect_i_(cell_x, cell_y, 22, 23), 0, 0, 0, (blend_mode * 80) >> 7);
                DrawMenu2DSprite(EuroTex, CRect_i_(key_x, key_y, 22, 22), CRect_i_(cell_x, cell_y, 22, 23), (u8) brightness, (u8) brightness, (u8) brightness, blend_mode);
            }
        }
    }
}

/**
 * Gives where a keyboard tab is drawn across the name-entry frame for a language. PAL only.
 *
 * @mangled Get_NameTemp_PutX__Fii
 * @address 0x23F700
 * @size 0x70
 */
static s16 Get_NameTemp_PutX(int language, int tab) {
    s16 tab_x[7][11] = {
        {40, 125, 209, 261, 311, 378, 40, 74, 111, 168, 223},
        {0,  0,   40,  223, 303, 384, 40, 74, 111, 168, 223},
        {0,  0,   40,  144, 303, 384, 40, 74, 111, 168, 223},
        {0,  0,   40,  140, 303, 384, 40, 74, 111, 168, 223},
        {0,  0,   40,  150, 303, 384, 40, 74, 111, 168, 223},
        {0,  0,   40,  142, 303, 384, 40, 74, 111, 168, 223},
        {0,  0,   40,  132, 283, 384, 40, 74, 111, 168, 223},
    };
    return tab_x[language][tab];
}

char NameEntryImageDescriptor[] __attribute__((section(".rodata"))) = "#frame_image_name#640#" SCREEN_HEIGHT_STR "#4";
char NameEntryTextureFile[] __attribute__((section(".rodata"))) = "nameregi.img";
char NameEntryTempTexture[] __attribute__((section(".rodata"))) = "nametemp";
char NameEntryHiraganaTexture[] __attribute__((section(".rodata"))) = "hira";
char NameEntryKatakanaTexture[] __attribute__((section(".rodata"))) = "kata";
char NameEntryAlphabetTexture[] __attribute__((section(".rodata"))) = "alphabet";
char NameEntryEuroTextureFormat[] __attribute__((section(".rodata"))) = "euro tex is %p\n";
char NameEntryFaceTexture[] __attribute__((section(".rodata"))) = "charaface";
char NameEntryMessageFile[] __attribute__((section(".rodata"))) = "nameregi.bin";
char NameEntryMessageFile2[] __attribute__((section(".rodata"))) = "nameregi2.bin";
char NameEntryFrameTexture[] __attribute__((section(".rodata"))) = "frame_image";

/**
 * Draws the character keyboard the name is entered from.
 *
 * @mangled DrawNameTemplete__Fiiii
 * @address 0x2390A0
 * @size 0x930
 */
static void DrawNameTemplete(int x, int y, int brightness, int blend_mode) {
    int input_mode = NameSelect.input_mode;
    int pushed_tab = NameSelect.pushed_tab;
    int draw_x;
    int draw_y;
    int tex_u;
    int tex_v;
    int i;
    int key_x;
    int key_y;
    s16 key_count[4] = {89, 88, 52, 40};

    DrawMenu2DSprite(NameTemp, CRect_i_(x, y, 0x200, 0x100), CRect_i_(0, 0, 0x200, 0x100), brightness, brightness, brightness, blend_mode);

    int language = NameSelect.language;
    int tab_height = 0x18;
    u8  tab_width[7][11] = {
        {78, 78, 44,  44,  60, 52, 24, 24, 48, 48, 112},
        {0,  0,  176, 75,  74, 70, 24, 24, 48, 48, 112},
        {0,  0,  96,  155, 74, 70, 24, 24, 48, 48, 112},
        {0,  0,  92,  159, 76, 70, 24, 24, 48, 48, 112},
        {0,  0,  103, 148, 74, 70, 24, 24, 48, 48, 112},
        {0,  0,  94,  157, 75, 70, 24, 24, 48, 48, 112},
        {0,  0,  86,  145, 95, 70, 24, 24, 48, 48, 112},
    };

    if (NameSelect.state == NAME_STATE_TAB_FLASH) {
        draw_x = x + Get_NameTemp_PutX(language, pushed_tab);
        draw_y = y + 6 + (pushed_tab / 6) * 27;

        tex_u = 0;

        if (pushed_tab < 6) {
            i = 0;
            tex_v = 0x148;
        } else {
            i = 6;
            tex_v = 0x160;
        }

        for (; i < pushed_tab; i++) {
            tex_u += tab_width[language][i];
        }

        if (pushed_tab == NAME_TAB_OK) {
            draw_y += 4;
            tab_height = 0x28;
        }

        DrawMenu2DSprite(NameTemp, CRect_i_(draw_x, draw_y, tab_width[language][pushed_tab], tab_height), CRect_i_(tex_u, tex_v, tab_width[language][pushed_tab], tab_height), brightness, brightness, brightness, blend_mode);
    }

    draw_x = x + Get_NameTemp_PutX(language, input_mode);
    draw_y = y + 6;

    for (i = 0, tex_u = 0; i < input_mode; i++) {
        tex_u += tab_width[language][i];
    }

    DrawMenu2DSprite(NameTemp, CRect_i_(draw_x, draw_y, tab_width[language][input_mode], 0x18), CRect_i_(tex_u, 0x148, tab_width[language][input_mode], 0x18), brightness, brightness, brightness, blend_mode);

    int       base_x = x + 0x38;
    int       base_y = y + 0x54;
    CTexture *texture;

    draw_y = base_y;

    if (input_mode == NAME_INPUT_KATAKANA) {
        texture = KataTex;
    }

    if (input_mode == NAME_INPUT_HIRAGANA) {
        texture = HiraTex;
    }

    switch (input_mode) {
        case NAME_INPUT_KATAKANA:
        case NAME_INPUT_HIRAGANA:
            draw_x = base_x - 8;

            for (int key = 0; key < key_count[input_mode]; key++) {
                tex_u = (key % 10) * 22;
                tex_v = (key / 10) * 22;

                CRect_i_ tex_rect(tex_u, tex_v, 22, 23);

                DrawMenu2DSprite(texture, CRect_i_(draw_x + 1, draw_y + 1, 22, 22), tex_rect, 0, 0, 0, (blend_mode * 0x50) >> 7);
                DrawMenu2DSprite(texture, CRect_i_(draw_x, draw_y, 22, 22), tex_rect, brightness, brightness, brightness, blend_mode);

                if (key < 0x23 && key != 0x1D) {
                    draw_x += 0x26;

                    if (key % 5 == 4) {
                        draw_x -= 0xBE;
                        draw_y += 0x1A;
                    }
                } else if (key >= 0x50) {
                    draw_y += 0x1A;
                } else {
                    switch (key) {
                        case 0x1D:
                            draw_x = base_x + 0xCA;
                            draw_y = base_y;
                            break;
                        case 0x23:
                        case 0x24:
                        case 0x2B:
                        case 0x2C:
                            draw_x += 0x4C;
                            break;
                        case 0x31:
                            draw_x -= 0x72;
                            draw_y += 0x1A;
                            break;
                        case 0x25:
                        case 0x2A:
                        case 0x2D:
                            draw_x -= 0x98;
                            draw_y += 0x1A;
                            break;
                        case 0x36:
                            draw_y = base_y;

                            if (input_mode == NAME_INPUT_KATAKANA) {
                                key = 0x50;
                            }

                            if (input_mode == NAME_INPUT_HIRAGANA) {
                                key = 0x4F;
                            }
                        default:
                            draw_x += 0x26;
                            break;
                    }
                }
            }

            break;
        case NAME_INPUT_ALPHABET:
            draw_x = base_x - 0xE;

            // The European keyboards move up to make room for their accented rows.
            if (NameSelect.language > LANG_ENGLISH_UK) {
                base_y += euro_code_linelimmit[NameSelect.language - 2] * -3;
            }

            for (int key = 0; key < 0x34; key++) {
                tex_u = (key % 10) * 22;
                tex_v = (key / 10) * 22;

                CRect_i_ tex_rect(tex_u, tex_v, 22, 23);
                key_x = draw_x + (key % 13) * 34;

                key_y = base_y + (key / 13) * 26;
                DrawMenu2DSprite(AlphaTex, CRect_i_(key_x + 1, key_y + 1, 22, 22), tex_rect, 0, 0, 0, (blend_mode * 0x50) >> 7);
                DrawMenu2DSprite(AlphaTex, CRect_i_(key_x, key_y, 22, 22), tex_rect, brightness, brightness, brightness, blend_mode);
            }

            if (NameSelect.language > LANG_ENGLISH_UK) {
                DrawEuroSpecialFont(draw_x, key_y + 26, NameSelect.language - 2, brightness, blend_mode);
            }

            break;
        case NAME_INPUT_SYMBOL:
            for (int key = 0; key < key_count[3]; key++) {
                int cell_no = menu_kigoutbl[key] + 2;

                if (cell_no >= 0) {
                    tex_u = (cell_no % 10) * 22;
                    tex_v = (cell_no / 10) * 22 + 0x6E;
                    key_x = base_x + (key % 10) * 38;
                    key_y = base_y + (key / 10) * 26;
                    DrawMenu2DSprite(AlphaTex, CRect_i_(key_x, key_y, 22, 22), CRect_i_(tex_u, tex_v, 22, 23), brightness, brightness, brightness, blend_mode);
                }
            }

            break;
    }
}

/**
 * Reports whether two names are the same.
 *
 * @mangled NameCompare__FPsPs
 * @address 0x2399D0
 * @size 0x6C
 */
static int NameCompare(short *first, short *second) {
    int matching = 0;

    for (int i = 0; i < 10; i++) {
        if (first[i] == second[i]) {
            matching++;
        }
    }

    // Every character has to agree before the names count as one.
    if (matching >= 10) {
        return 0;
    }

    return 1;
}

/**
 * Reports whether the entered name may be used.
 *
 * @mangled CheckName__Fv
 * @address 0x239A40
 * @size 0x160
 */
int CheckName() {
    s16 blank_names[2][10] = {
        {0,   0,   0,   0,   0,   0,   0,   0,   0,   0  },
        {230, 230, 230, 230, 230, 230, 230, 230, 230, 230},
    };
    int blank_count = 0;

    for (int slot = 0; slot < 10; slot++) {
        if (CharaName[slot] == 0 || CharaName[slot] == 230) {
            blank_count++;
        }
    }

    if (blank_count >= 10) {
        return NAME_CHECK_EMPTY;
    }

    for (int blank_no = 0; blank_no < 2; blank_no++) {
        if (NameCompare(CharaName, blank_names[blank_no]) == 0) {
            return NAME_CHECK_REJECTED;
        }
    }

    for (int other_chara_no = NameSelect.chara_no - 1; 0 <= other_chara_no; other_chara_no--) {
        if (NameCompare(CharaName, SaveData->GetCharaName(other_chara_no)) == 0) {
            return NAME_CHECK_REJECTED;
        }
    }

    return NAME_CHECK_OK;
}

/**
 * Draws the name-entry screen.
 *
 * @mangled NameEnterDraw__Fv
 * @address 0x239BA0
 * @size 0xC9C
 */
void NameEnterDraw() {
    int   language;
    int   chara_no;
    int   fade;
    int   brightness;
    float cursor_x;
    float cursor_y;

    setbilinear(0);

    if (NameSelect.chara_no != 0) {
        AllFadeForMenu(0x80);
    }

    if (NameSelect.loaded == 0) {
        ReadBG();

        if (ReadBGSync() == 0) {
            LOADTEXTURE_INFO2 info[3] = {
                {NameEntryImageDescriptor, 0, 0},
                {NULL,                     0, 0},
                {NULL,                     0, 0}
            };

            info[0].block_no = NameSelect.texture_block;
            info[1].block_no = NameSelect.texture_block;

            BG_READ_INFO *read_file = GetReadBGFile(0);
            info[1].name = (char *) GetPackFile((u_int *) read_file->buffer, NameEntryTextureFile, NULL);
            TexManager.DeleteTextureBlock(NameSelect.texture_block);
            TexManager.LoadTextureBlockEX(-1, info);
            NameTemp = TexManager.GetTexture(NameEntryTempTexture, -1);
            HiraTex = TexManager.GetTexture(NameEntryHiraganaTexture, -1);
            KataTex = TexManager.GetTexture(NameEntryKatakanaTexture, -1);
            AlphaTex = TexManager.GetTexture(NameEntryAlphabetTexture, -1);
            EuroTex = TexManager.GetTexture("euro", -1);
            printf(NameEntryEuroTextureFormat, EuroTex);
            CharaFace = TexManager.GetTexture(NameEntryFaceTexture, -1);
            NameSelect.loaded = true;

            short *messages = (short *) GetPackFile((u_int *) read_file->buffer, NameEntryMessageFile, NULL);
            short *messages2 = (short *) GetPackFile((u_int *) read_file->buffer, NameEntryMessageFile2, NULL);
            InitMenuMesSet(MENU_MES_SET_NAME_ENTRY, messages);
            CommonMenuMes2.SetBuff(messages2);

            s8 char_size[7][2] = {
                {18, 22},
                {11, 20},
                {11, 20},
                {11, 20},
                {11, 20},
                {11, 20},
                {11, 20}
            };
            CommonMenuMes2.char_width = char_size[NameSelect.language][0];
            CommonMenuMes2.char_height = char_size[NameSelect.language][1];
            CommonMenuMes2.stay_frame = false;
            CommonMenuMes3.stay_frame = false;
            AtoraNameMes.mes_no[0] = NameSelect.chara_no + 0x3C;
            AtoraNameMes.MakeMesWin(0x1E);
        }

        if (NameSelect.chara_no == 0) {
            return;
        }
    }

    language = NameSelect.language;
    MenuTextureReload(NameSelect.texture_block);
    chara_no = NameSelect.chara_no;
    brightness = fade = 0x80;

    switch (NameSelect.state) {
        case NAME_STATE_FADE_IN:
            fade = NameSelect.state_count * 5;

            if (fade > 0x80) {
                fade = 0x80;
                NameSelect.state = NAME_STATE_IDLE;
            }

            break;
    }

    if (NameSelect.area >= NAME_AREA_HELP) {
        brightness = 0x38;
    }

    CTexture *frame_texture = TexManager.GetTexture(NameEntryFrameTexture, -1);

    if ((chara_no != 0 || NameSelect.loaded == 0) && frame_texture != NULL) {
        FrameImageDraw(0x80, 0x80);
    }

    if (NameSelect.loaded != 0) {
        MenuTextureReload(NameSelect.texture_block);
        DrawMenu2DSprite(NameTemp, CRect_i_(0x30, 0x21, 0x3C, 0x27), CRect_i_(0x1C0, 0x100, 0x3C, 0x28), fade);
        DrawMenu2DSprite(NameTemp, CRect_i_(0x6E, 0x24, 0xAE, 0x1F), CRect_i_(0xC0, 0x178, 0xAE, 0x20), fade);

        int name_color = brightness;

        if (NameSelect.area >= NAME_AREA_HELP) {
            name_color = 0x80;
        }

        DrawCharaNameUp(0x3E, 0x36, name_color, fade);
        DrawNameTemplete(0x42, 0x9E, brightness, fade);

        switch (NameSelect.area) {
            case NAME_AREA_TABS:
                switch (language) {
                    case LANG_JAPANESE: {
                        s16 tab_x[11] = {88, 160, 252, 310, 358, 420, 88, 114, 154, 216, 268};
                        cursor_x = tab_x[NameSelect.cursor];
                        break;
                    }
                    case LANG_ENGLISH_US: {
                        s16 tab_x[11] = {88, 88, 88, 256, 350, 436, 88, 114, 154, 216, 268};
                        cursor_x = tab_x[NameSelect.cursor];
                        break;
                    }
                    default:
                        cursor_x = (66.0f + Get_NameTemp_PutX(NameSelect.language, NameSelect.cursor)) - 28.0f;
                        break;
                }

                if (NameSelect.cursor < NAME_TAB_LEFT) {
                    cursor_y = 164.0f;

                    if (NameSelect.cursor == NAME_TAB_OK) {
                        cursor_y += 4.0f;
                    }
                } else {
                    cursor_y = 188.0f;
                }

                break;
            case NAME_AREA_KEYBOARD: {
                int keys_per_row = InputModeOrikaeshi[NameSelect.input_mode];
                int step_x = InputModeMovetbl[NameSelect.input_mode][0];
                int step_y = InputModeMovetbl[NameSelect.input_mode][1];
                int cursor_key = NameSelect.cursor;
                int column = cursor_key % keys_per_row;

                cursor_x = step_x * column + 0x62;
                cursor_y = step_y * (cursor_key / keys_per_row) + 0xEE;

                if (NameSelect.input_mode < NAME_INPUT_ALPHABET) {
                    cursor_x -= 8.0f;

                    if (column > 4) {
                        cursor_x += 20.0f;
                    }

                    if (NameSelect.side_row > 0) {
                        cursor_x = step_x * keys_per_row + 0x6E;
                        cursor_y = step_y * (NameSelect.side_row - 1) + 0xEE;
                    }
                }

                if (NameSelect.input_mode == NAME_INPUT_ALPHABET) {
                    cursor_x -= 14.0f;

                    if (NameSelect.language > LANG_ENGLISH_UK) {
                        cursor_y += euro_code_linelimmit[NameSelect.language - 2] * -3;
                    }
                }

                break;
            }
        }

        if (NameSelect.area < NAME_AREA_HELP) {
            if (abs((int) (cursor_x - NameSelect.cursor_x)) < 0x144) {
                NameSelect.cursor_x += (cursor_x - NameSelect.cursor_x) / 4.0f;
                NameSelect.cursor_y += (cursor_y - NameSelect.cursor_y) / 4.0f;
            } else {
                NameSelect.cursor_x = cursor_x;
            }
        }

        switch (NameSelect.state) {
            case NAME_STATE_FADE_IN:
            case NAME_STATE_FADE_OUT:
                break;
            default: {
                if (NameSelect.area == NAME_AREA_KEYBOARD && 230.0f <= NameSelect.cursor_y) {
                    DrawNameRegiWaku((int) (16.0f + cursor_x), (int) (cursor_y - 6.0f), 0x1C, brightness, fade);
                }

                // The hand sways on its own counter, which holds still while a tab flashes.
                static int lct = 0;

                if (NameSelect.state != NAME_STATE_TAB_FLASH) {
                    lct++;
                }

                if (lct > 100000) {
                    lct = 0;
                }

                cursor_x = NameSelect.cursor_x + 5.0f * cosf(0.07853981852531433f * (float) lct);
                cursor_y = NameSelect.cursor_y + 3.0f * sinf(0.13089969754219055f * (float) lct);

                if (NameSelect.area < NAME_AREA_EMPTY_NOTICE) {
                    CRect_i_ hand_rect(0x1C0, 0x128, 0x20, 0x20);

                    DrawMenu2DSprite(NameTemp, CRect_i_((int) (2.0f + cursor_x), (int) (2.0f + cursor_y), 0x20, 0x20), hand_rect, 0, 0, 0, (fade * 0x50) >> 7);
                    DrawMenu2DSprite(NameTemp, CRect_i_((int) cursor_x, (int) cursor_y, 0x20, 0x20), hand_rect, brightness, brightness, brightness, fade);
                }

                break;
            }
        }

        NameSelect.frame++;

        if (NameSelect.frame > 80000) {
            NameSelect.frame = 0;
        }

        CommonMenuMes3.text_x = 0x1B6;
        CommonMenuMes3.text_y = 0x58;
        DrawMenu2DSprite(NameTemp, CRect_i_(0x1A3, 0x46, 0xB8, 0x51), CRect_i_(0, 0x178, 0xC0, 0x61), 0x64, 0x64, 0x64, fade);

        if (NameSelect.area >= NAME_AREA_HELP) {
            int win_x = 0xAE;
            int win_y = 0xAA;
            int win_width = 0x12C;

            switch (NameSelect.area) {
                case NAME_AREA_CONFIRM:
                    win_x = 0xD8;
                    win_y = 0xB8;
                    win_width = 0xDA;
                    break;
                case NAME_AREA_EMPTY_NOTICE:
                    win_x = 0xBA;
                    win_y = 0xB8;
                    win_width = 0x116;
                    break;
            }

            DrawMenu2DSprite(NameTemp, CRect_i_(win_x, win_y, win_width, 0x60), CRect_i_(0, 0x178, 0xC0, 0x61), 0x64, 0x64, 0x64, fade);
            CommonMenuMes2.text_x = win_x + 0x10;
            CommonMenuMes2.text_y = win_y + 0xE;
        }

        MenuTextureReload(CommonMenuMes2.tex_block);
        CommonMenuMes3.edge_alpha = fade;
        CommonMenuMes3.Step();
        CommonMenuMes3.DrawMesWin();

        switch (NameSelect.area) {
            case NAME_AREA_HELP:
            case NAME_AREA_CONFIRM:
            case NAME_AREA_EMPTY_NOTICE:
                CommonMenuMes2.edge_alpha = fade;
                CommonMenuMes2.Step();
                CommonMenuMes2.DrawMesWin();
                break;
        }

        setbilinear(0);

        switch (NameSelect.state) {
            case NAME_STATE_FADE_IN:
                fade = 0x80 - NameSelect.state_count * 6;

                if (fade < 0) {
                    fade = 0;
                }

                if (NameSelect.chara_no != 0) {
                    FrameImageDraw(0x80, fade);
                }

                break;
            case NAME_STATE_EXIT:
                ExitNameEnterFunc();
            case NAME_STATE_FADE_OUT:
                fade = NameSelect.state_count * 3;

                if (fade > 0x80) {
                    fade = 0x80;
                }

                AllFadeForMenu(fade);
                break;
        }

        if (NameSelect.state != NAME_STATE_IDLE) {
            NameSelect.state_count++;
        }

        setbilinear(1);
    }
}

/**
 * Moves the cursor across the keyboard and enters the character it settles on.
 *
 * @mangled NameEnterKey__Fv
 * @address 0x23A840
 * @size 0x1F28
 */
s32 NameEnterKey() {
    int language;
    int chara_no;
    int cursor;
    int side_row;
    int name_pos;
    int action;
    int mes_no;
    int input_mode;
    int sound;

    if (NameSelect.loaded == 0) {
        return 0;
    }

    language = NameSelect.language;
    chara_no = NameSelect.chara_no;

    switch (NameSelect.state) {
        case NAME_STATE_FADE_OUT:
            if (NameSelect.state_count >= 84) {
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                NameSelect.state = NAME_STATE_EXIT;
                return 1;
            }

            return 0;
        case NAME_STATE_TAB_FLASH:
            if (NameSelect.state_count > 8) {
                NameSelect.state = NAME_STATE_IDLE;
            }

            break;
    }

    cursor = NameSelect.cursor;
    side_row = NameSelect.side_row;
    name_pos = NameSelect.name_pos;
    action = NAME_ACTION_NONE;
    mes_no = -1;

    switch (NameSelect.area) {
        case NAME_AREA_HELP:
            mes_no = chara_no + 100;

            if (GamePad.Down(PAD_CROSS) || GamePad.Down(PAD_CIRCLE)) {
                NameSelect.area = NAME_AREA_TABS;
                NameSelect.state = NAME_STATE_IDLE;
            }

            break;
        case NAME_AREA_CONFIRM:
        case NAME_AREA_EMPTY_NOTICE:
            if (NameSelect.area == NAME_AREA_CONFIRM) {
                mes_no = 91;
            }

            if (NameSelect.area == NAME_AREA_EMPTY_NOTICE) {
                mes_no = 95;
            }

            if (GamePad.Down(PAD_CROSS)) {
                NameSelect.state = NAME_STATE_IDLE;
                action = NAME_ACTION_FINISH;
            } else if (GamePad.Down(PAD_CIRCLE)) {
                NameSelect.area = NAME_AREA_KEYBOARD;
                NameSelect.state = NAME_STATE_IDLE;
            }

            break;
        case NAME_AREA_KEYBOARD:
            if (NameSelect.input_mode > NAME_INPUT_HIRAGANA) {
                NameSelect.side_row = 0;
            }

            switch (NameSelect.side_row) {
                case 0: {
                    int keys_per_row = InputModeOrikaeshi[NameSelect.input_mode];

                    if (GamePad.Down(PAD_DOWN)) {
                        int rows = 5;

                        switch (NameSelect.input_mode) {
                            case NAME_INPUT_SYMBOL:
                                rows -= 2;
                                break;
                            case NAME_INPUT_ALPHABET:
                                rows = 4;

                                if (NameSelect.language > LANG_ENGLISH_UK) {
                                    rows = euro_code_linelimmit[NameSelect.language - 2] + 4;
                                }

                                break;
                        }

                        if (NameSelect.cursor / keys_per_row < rows) {
                            NameSelect.cursor += keys_per_row;
                        }
                    } else if (GamePad.Down(PAD_LEFT)) {
                        if (NameSelect.cursor % keys_per_row != 0) {
                            NameSelect.cursor--;
                        } else if (NameSelect.input_mode > NAME_INPUT_HIRAGANA) {
                            NameSelect.cursor += keys_per_row - 1;
                        } else {
                            NameSelect.side_row = NameSelect.cursor / keys_per_row + 1;
                        }
                    } else if (GamePad.Down(PAD_RIGHT)) {
                        if (keys_per_row - 1 != NameSelect.cursor % keys_per_row) {
                            NameSelect.cursor++;
                        } else if (NameSelect.input_mode > NAME_INPUT_HIRAGANA) {
                            NameSelect.cursor -= keys_per_row - 1;
                        } else {
                            NameSelect.side_row = NameSelect.cursor / keys_per_row + 1;
                        }
                    } else if (GamePad.Down(PAD_UP)) {
                        int row = NameSelect.cursor / keys_per_row;

                        if (0 < row) {
                            NameSelect.cursor -= keys_per_row;
                        } else if (row <= 0) {
                            NameSelect.area = NAME_AREA_TABS;

                            if (NameSelect.cursor < 4) {
                                NameSelect.cursor += 6;
                            } else if (NameSelect.cursor >= 4 && NameSelect.cursor < 7) {
                                NameSelect.cursor = NAME_TAB_HELP;
                            } else {
                                NameSelect.cursor = NAME_TAB_OK;
                            }
                        }
                    }

                    break;
                }
                default: {
                    int column = -1;

                    if (GamePad.Down(PAD_UP)) {
                        if (NameSelect.side_row == 1) {
                            NameSelect.side_row = 0;
                            NameSelect.area = NAME_AREA_TABS;
                            NameSelect.cursor = NAME_TAB_OK;
                        } else {
                            NameSelect.side_row--;
                        }
                    } else if (GamePad.Down(PAD_DOWN)) {
                        if (NameSelect.side_row < 6) {
                            NameSelect.side_row++;
                        }
                    } else if (GamePad.Down(PAD_LEFT)) {
                        column = 1;
                    } else if (GamePad.Down(PAD_RIGHT)) {
                        column = 0;
                    }

                    if (column != -1) {
                        NameSelect.cursor = (NameSelect.side_row - 1) * 10 + column * 9;
                        NameSelect.side_row = 0;
                    }

                    break;
                }
            }

            if (GamePad.Down(PAD_CROSS)) {
                action = NAME_ACTION_INPUT;
            } else if (GamePad.Down(PAD_CIRCLE)) {
                action = NAME_ACTION_BACKSPACE;
            }

            break;
        case NAME_AREA_TABS: {
            if (GamePad.Down(PAD_UP)) {
                switch (language) {
                    case LANG_JAPANESE:
                        if (NameSelect.cursor >= NAME_TAB_LEFT) {
                            if (NameSelect.cursor < NAME_TAB_DELETE) {
                                NameSelect.cursor = NAME_TAB_KATAKANA;
                            } else if (NameSelect.cursor < NAME_TAB_HELP) {
                                NameSelect.cursor = NAME_TAB_HIRAGANA;
                            } else if (NameSelect.cursor == NAME_TAB_HELP) {
                                NameSelect.cursor = NAME_TAB_ALPHABET;
                            }
                        }

                        break;
                    case LANG_ENGLISH_US:
                    default:
                        if (NameSelect.cursor >= NAME_TAB_LEFT) {
                            if (NameSelect.cursor < NAME_TAB_HELP) {
                                NameSelect.cursor = NAME_TAB_ALPHABET;
                            } else if (NameSelect.cursor < 11) {
                                NameSelect.cursor = NAME_TAB_SYMBOL;
                            } else if (NameSelect.cursor == NAME_TAB_HELP) {
                                NameSelect.cursor = NAME_TAB_DEFAULT_NAME;
                            }
                        }

                        break;
                }
            } else if (GamePad.Down(PAD_DOWN)) {
                if (NameSelect.cursor >= NAME_TAB_LEFT) {
                    NameSelect.area = NAME_AREA_KEYBOARD;
                    s8 below_key[7] = {0, 0, 1, 2, 3, 4, 4};
                    NameSelect.cursor = below_key[NameSelect.cursor - NAME_TAB_LEFT];
                } else if (NameSelect.cursor < NAME_TAB_LEFT) {
                    switch (language) {
                        case LANG_JAPANESE:
                            switch (NameSelect.cursor) {
                                case NAME_TAB_KATAKANA:
                                    NameSelect.cursor = NAME_TAB_LEFT;
                                    break;
                                case NAME_TAB_HIRAGANA:
                                    NameSelect.cursor = NAME_TAB_DELETE;
                                    break;
                                case NAME_TAB_OK:
                                    NameSelect.cursor = 8;
                                    NameSelect.area = NAME_AREA_KEYBOARD;
                                    break;
                                default:
                                    NameSelect.cursor = NAME_TAB_HELP;
                                    break;
                            }

                            break;
                        case LANG_ENGLISH_US:
                        default:
                            switch (NameSelect.cursor) {
                                case NAME_TAB_ALPHABET:
                                    NameSelect.cursor = NAME_TAB_LEFT;
                                    break;
                                case NAME_TAB_SYMBOL:
                                    NameSelect.cursor = NAME_TAB_INSERT;
                                    break;
                                case NAME_TAB_OK:
                                    NameSelect.cursor = 8;
                                    NameSelect.area = NAME_AREA_KEYBOARD;
                                    break;
                                default:
                                    NameSelect.cursor = NAME_TAB_HELP;
                                    break;
                            }

                            break;
                    }
                }
            }

            static s8 up_or_down = 0;

            if (GamePad.Down(PAD_LEFT)) {
                switch (language) {
                    case LANG_JAPANESE:
                        switch (NameSelect.cursor) {
                            case NAME_TAB_KATAKANA:
                            case NAME_TAB_LEFT:
                                NameSelect.cursor = NAME_TAB_OK;
                                break;
                            case NAME_TAB_OK:
                                if (up_or_down != 0) {
                                    NameSelect.cursor = NAME_TAB_DEFAULT_NAME;
                                } else {
                                    NameSelect.cursor = NAME_TAB_HELP;
                                }

                                break;
                            default:
                                NameSelect.cursor--;
                                break;
                        }

                        break;
                    case LANG_ENGLISH_US:
                    default:
                        switch (NameSelect.cursor) {
                            case NAME_TAB_ALPHABET:
                            case NAME_TAB_LEFT:
                                NameSelect.cursor = NAME_TAB_OK;
                                break;
                            case NAME_TAB_OK:
                                if (up_or_down != 0) {
                                    NameSelect.cursor = NAME_TAB_DEFAULT_NAME;
                                } else {
                                    NameSelect.cursor = NAME_TAB_HELP;
                                }

                                break;
                            default:
                                NameSelect.cursor--;
                                break;
                        }

                        break;
                }
            } else if (GamePad.Down(PAD_RIGHT)) {
                switch (language) {
                    case LANG_JAPANESE:
                        switch (NameSelect.cursor) {
                            case NAME_TAB_OK:
                                if (up_or_down == 0) {
                                    NameSelect.cursor = NAME_TAB_KATAKANA;
                                } else {
                                    NameSelect.cursor = NAME_TAB_LEFT;
                                }

                                break;
                            case NAME_TAB_HELP:
                                NameSelect.cursor = NAME_TAB_OK;
                                break;
                            default:
                                NameSelect.cursor++;
                                break;
                        }

                        break;
                    case LANG_ENGLISH_US:
                    default:
                        switch (NameSelect.cursor) {
                            case NAME_TAB_OK:
                                if (up_or_down == 0) {
                                    NameSelect.cursor = NAME_TAB_ALPHABET;
                                } else {
                                    NameSelect.cursor = NAME_TAB_LEFT;
                                }

                                break;
                            case NAME_TAB_HELP:
                                NameSelect.cursor = NAME_TAB_OK;
                                break;
                            default:
                                NameSelect.cursor++;
                                break;
                        }

                        break;
                }
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                action = NAME_ACTION_BACKSPACE;
            } else if (GamePad.Down(PAD_CROSS)) {
                NameSelect.pushed_tab = NameSelect.cursor;

                if (NameSelect.cursor < NAME_TAB_DEFAULT_NAME) {
                    if (NameSelect.input_mode != NameSelect.cursor) {
                        NameSelect.input_mode = NameSelect.cursor;
                    }
                } else {
                    NameSelect.state = NAME_STATE_TAB_FLASH;
                    NameSelect.state_count = 0;
                    int tab_action[7] = {NAME_ACTION_DEFAULT_NAME, NAME_ACTION_DECIDE, NAME_ACTION_LEFT, NAME_ACTION_RIGHT, NAME_ACTION_DELETE, NAME_ACTION_INSERT, NAME_ACTION_HELP};
                    action = tab_action[NameSelect.cursor - NAME_TAB_DEFAULT_NAME];
                }
            }

            break;
        }
    }

    if (GamePad.Down(PAD_START) && NameSelect.area < NAME_AREA_HELP) {
        action = NAME_ACTION_DECIDE;
    }

    GamePad.Down(PAD_TRIANGLE);

    if (GamePad.Down(PAD_L1)) {
        action = NAME_ACTION_LEFT;
    }

    if (GamePad.Down(PAD_R1)) {
        action = NAME_ACTION_RIGHT;
    }

    if (NameSelect.area < NAME_AREA_HELP) {
        if (GamePad.Down(PAD_R2)) {
            action = NAME_ACTION_NEXT_MODE;
        }

        if (GamePad.Down(PAD_L2)) {
            action = NAME_ACTION_PREV_MODE;
        }
    }

    input_mode = NameSelect.input_mode;

    switch (action) {
        case NAME_ACTION_NONE:
            break;
        case NAME_ACTION_INPUT: {
            int slot;
            int code_base;
            int key;
            int code;

            key = NameSelect.cursor;
            slot = NameSelect.name_pos;

            switch (input_mode) {
                case NAME_INPUT_HIRAGANA:
                case NAME_INPUT_KATAKANA:
                    if (input_mode == NAME_INPUT_HIRAGANA) {
                        code_base = 82;
                    } else {
                        code_base = 1;
                    }

                    if (NameSelect.side_row == 0) {
                        int column = NameSelect.cursor % 10;

                        if (column < 5) {
                            key = column + (NameSelect.cursor / 10) * 5;
                        } else {
                            key = column + 25 + (NameSelect.cursor / 10) * 5;
                        }

                        if (key < 36) {
                            code = key;
                        } else {
                            if (key >= 40 && key < 46) {
                                code = key - 2;
                            } else if (key >= 50 && key < 54) {
                                code = key - 4;
                            } else if (key >= 55 && key < 60) {
                                code = key - 5;
                            } else {
                                switch (key) {
                                    case 49:
                                        key--;
                                    case 47:
                                        key--;
                                    case 45:
                                    case 39:
                                        key--;
                                    case 37:
                                        key--;
                                        break;
                                    default:
                                        key = 230 - code_base;
                                        break;
                                }

                                code = key;
                            }
                        }

                        if (slot >= 10) {
                            slot--;
                        }

                        CharaName[slot] = code + code_base;

                        if (NameSelect.name_pos < 10) {
                            NameSelect.name_pos++;
                        }
                    } else {
                        switch (NameSelect.side_row) {
                            case 1:
                                if (0 < slot) {
                                    int previous_code = CharaName[slot - 1];

                                    if ((previous_code >= 6 && previous_code < 21) || (previous_code >= 26 && previous_code < 31) || (previous_code >= 87 && previous_code < 102) || (previous_code >= 107 && previous_code < 112)) {
                                        CharaName[slot - 1] += 50;
                                    } else if (previous_code == 3) {
                                        CharaName[slot - 1] = 81;
                                    }
                                }

                                break;
                            case 2:
                                if (0 < slot) {
                                    int previous_code = CharaName[slot - 1];

                                    if ((previous_code >= 26 && previous_code < 31) || (previous_code >= 107 && previous_code < 112)) {
                                        CharaName[slot - 1] += 45;
                                    }
                                }

                                break;
                            case 3:
                                key = 222 - code_base;
                                code = key;

                                if (slot >= 10) {
                                    slot--;
                                }

                                CharaName[slot] = code + code_base;

                                if (NameSelect.name_pos < 10) {
                                    NameSelect.name_pos++;
                                }

                                break;
                            default:
                                CharaName[slot] = 230;
                                break;
                        }
                    }

                    break;
                case NAME_INPUT_ALPHABET:
                    if (NameSelect.cursor < 52) {
                        code = NameSelect.cursor + 162;
                    } else if (NameSelect.language > LANG_ENGLISH_UK) {
                        code = menu_euro_codetbl[NameSelect.language - 2][0][NameSelect.cursor - 52];
                    }

                    if (slot >= 10) {
                        slot--;
                    }

                    CharaName[slot] = code;

                    if (NameSelect.name_pos < 10) {
                        NameSelect.name_pos++;
                    }

                    break;
                case NAME_INPUT_SYMBOL:
                    if (language == 0) {
                        if (key < 28) {
                            code = key;
                        } else if (key >= 30 && key < 40) {
                            code = key - 2;
                        } else if (key == 28 || key == 29) {
                            code = key + 10;
                        } else {
                            code = 16;
                        }

                        if (slot >= 10) {
                            slot--;
                        }

                        CharaName[slot] = code + 214;

                        if (NameSelect.name_pos < 10) {
                            NameSelect.name_pos++;
                        }
                    } else {
                        if (language < 2) {
                            int symbol = menu_kigoutbl[NameSelect.cursor];

                            code = symbol + 214;

                            if (symbol < 0) {
                                code = 230;
                            }
                        } else {
                            if (NameSelect.cursor < 40) {
                                code = menu_kigoutbl[NameSelect.cursor] + 214;
                            }

                            if (menu_kigoutbl[NameSelect.cursor] < 0) {
                                code = 230;
                            }
                        }

                        if (slot >= 10) {
                            slot--;
                        }

                        CharaName[slot] = code;

                        if (NameSelect.name_pos < 10) {
                            NameSelect.name_pos++;
                        }
                    }

                    printf("now Input CHaraID = %d\n", CharaName[slot]);
                    break;
            }

            break;
        }
        case NAME_ACTION_DELETE:
            for (int i = NameSelect.name_pos + 1; i <= 11; i++) {
                CharaName[i - 1] = CharaName[i];
            }

            break;
        case NAME_ACTION_BACKSPACE:
            if (NameSelect.name_pos >= 10) {
                NameSelect.name_pos--;
                CharaName[NameSelect.name_pos] = 230;
            } else if (0 < NameSelect.name_pos) {
                CharaName[NameSelect.name_pos] = 230;
                NameSelect.name_pos--;
            } else {
                CharaName[0] = 230;
            }

            break;
        case NAME_ACTION_INSERT: {
            int slot = NameSelect.name_pos;
            int i;

            for (i = 9; i >= slot; i--) {
                CharaName[i + 1] = CharaName[i];
            }

            CharaName[i + 1] = 230;

            for (i = 10; i < 32; i++) {
                CharaName[i] = 230;
            }

            break;
        }
        case NAME_ACTION_NEXT_MODE:
            switch (language) {
                case LANG_JAPANESE:
                    if (input_mode < NAME_INPUT_SYMBOL) {
                        NameSelect.input_mode = input_mode + 1;
                    } else {
                        NameSelect.input_mode = NAME_INPUT_KATAKANA;
                    }

                    break;
                case LANG_ENGLISH_US:
                default:
                    if (input_mode < NAME_INPUT_SYMBOL) {
                        NameSelect.input_mode++;
                    } else {
                        NameSelect.input_mode = NAME_INPUT_ALPHABET;
                    }

                    break;
            }

            break;
        case NAME_ACTION_PREV_MODE:
            switch (language) {
                case LANG_JAPANESE:
                    if (NAME_INPUT_KATAKANA < input_mode) {
                        NameSelect.input_mode = input_mode - 1;
                    } else {
                        NameSelect.input_mode = NAME_INPUT_SYMBOL;
                    }

                    break;
                case LANG_ENGLISH_US:
                default:
                    if (input_mode > NAME_INPUT_ALPHABET) {
                        NameSelect.input_mode--;
                    } else {
                        NameSelect.input_mode = NAME_INPUT_SYMBOL;
                    }

                    break;
            }

            break;
        case NAME_ACTION_LEFT:
            if (0 < NameSelect.name_pos) {
                if (NameSelect.name_pos >= 10) {
                    NameSelect.name_pos--;
                }

                NameSelect.name_pos--;
            }

            break;
        case NAME_ACTION_RIGHT:
            if (NameSelect.name_pos < 10) {
                NameSelect.name_pos++;
            }

            break;
        case NAME_ACTION_HELP:
            NameSelect.area = NAME_AREA_HELP;
            NameSelect.state = NAME_STATE_HELP;
            break;
        case NAME_ACTION_DEFAULT_NAME:
            NameDefaultSet(NameSelect.chara_no);
            break;
        case NAME_ACTION_DECIDE:
            switch (CheckName()) {
                case NAME_CHECK_OK: {
                    int length;
                    int i;

                    NameSelect.area = NAME_AREA_CONFIRM;

                    for (length = 10; length > 0; length--) {
                        if (CharaName[length - 1] != 0 && CharaName[length - 1] != 230) {
                            break;
                        }
                    }

                    for (i = 0; i < length; i++) {
                        if (CharaName[i] == 0) {
                            CharaName[i] = 230;
                        }
                    }

                    for (; length < 32; length++) {
                        CharaName[length] = 0;
                    }

                    break;
                }
                case NAME_CHECK_REJECTED:
                    for (int i = 0; i < 10; i++) {
                        CharaName[i] = 0;
                    }

                    NameSelect.name_pos = 0;
                    break;
                case NAME_CHECK_EMPTY:
                    NameDefaultSet(NameSelect.chara_no);
                    mes_no = 95;
                    NameSelect.area = NAME_AREA_EMPTY_NOTICE;
                    break;
            }

            break;
        case NAME_ACTION_FINISH:
            for (int i = 0; i < 10; i++) {
                printf("CharaName[%d][%d] = %d\n", chara_no, i, CharaName[i]);
            }

            GamePad.AutoRepeatOff();
            GamePad.MenuModeOff();
            NameSelect.area = NAME_AREA_KEYBOARD;
            NameSelect.state = NAME_STATE_FADE_OUT;
            NameSelect.state_count = 0;
            break;
    }

    if (input_mode != NameSelect.input_mode && NameSelect.area != NAME_AREA_TABS) {
        int old_cursor;
        int row;
        int column;

        switch (NameSelect.input_mode) {
            case NAME_INPUT_HIRAGANA:
            case NAME_INPUT_SYMBOL:
                if (input_mode == NAME_INPUT_ALPHABET) {
                    old_cursor = NameSelect.cursor;
                    row = old_cursor / 13;
                    column = old_cursor % 13;

                    if (NameSelect.input_mode == NAME_INPUT_HIRAGANA && column == 12) {
                        NameSelect.side_row = row + 1;
                    } else {
                        if (column >= 5) {
                            column--;
                        }

                        if (NameSelect.input_mode == NAME_INPUT_SYMBOL) {
                            while (column >= 10) {
                                column--;
                            }

                            if (NameSelect.language > LANG_JAPANESE) {
                                while (row >= 4) {
                                    row--;
                                }
                            }
                        }

                        NameSelect.cursor = column + row * 10;
                    }
                }

                break;
            case NAME_INPUT_ALPHABET: {
                old_cursor = NameSelect.cursor;
                row = old_cursor / 10;
                column = old_cursor % 10;

                if (NameSelect.side_row > 0) {
                    NameSelect.cursor = NameSelect.side_row * 13 - 1;
                } else {
                    if (column >= 5) {
                        column++;
                    }

                    if (row >= 4) {
                        row = 4;
                    }

                    NameSelect.cursor = column + row * 13;
                }

                break;
            }
        }
    }

    s8 rows;

    switch (NameSelect.input_mode) {
        case NAME_INPUT_HIRAGANA:
        case NAME_INPUT_KATAKANA:
            break;
        case NAME_INPUT_SYMBOL:
            rows = 3;

            while (NameSelect.cursor >= 40) {
                NameSelect.cursor -= 10;
            }

            break;
        case NAME_INPUT_ALPHABET:
            rows = 4;

            if (NameSelect.language > LANG_ENGLISH_UK) {
                rows += euro_code_linelimmit[NameSelect.language - 2];
            }

            while (NameSelect.cursor >= rows * 13) {
                NameSelect.cursor -= 13;
            }

            break;
    }

    sound = -1;

    if (cursor != NameSelect.cursor || side_row != NameSelect.side_row) {
        sound = MENU_SOUND_CURSOR;
    }

    if (name_pos != NameSelect.name_pos || input_mode != NameSelect.input_mode) {
        sound = MENU_SOUND_CONFIRM;
    }

    if (GamePad.Down(PAD_CROSS)) {
        sound = MENU_SOUND_CONFIRM;
    }

    if (GamePad.Down(PAD_CIRCLE) || action == NAME_ACTION_DELETE || action == NAME_ACTION_BACKSPACE) {
        sound = MENU_SOUND_REFUSE;
    }

    if (CommonMenuMes3.mes_made != 0) {
        CommonMenuMes3.MakeMesWin(0);
    }

    if (CommonMenuMes2.mes_made != mes_no) {
        CommonMenuMes2.MakeMesWin(mes_no);
    }

    ComMenuSePlay(sound);
    return 0;
}

/**
 * Gives one party member their default name for the chosen language.
 *
 * @mangled NameDefaultSet__Fi
 * @address 0x23C770
 * @size 0x110
 */
void NameDefaultSet(int chara_no) {
    int language = GetMenuLangFlag();
    s16 default_names[7][6][11] = {
        {{20, 1, 46},          {12, 47, 5},          {60, 43, 222},        {41, 77, 222},        {3, 46, 56, 56},                {5, 63, 35, 46, 70}           },
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
        {{181, 202, 188, 201}, {185, 196, 188, 202}, {168, 202, 205, 202}, {179, 208, 189, 212}, {182, 201, 194, 188, 194, 188}, {176, 206, 200, 202, 201, 191}},
    };
    s16 *name = SaveData->GetCharaName(chara_no);
    int  length;

    for (length = 0; default_names[language][chara_no][length] != 0 && length < 10; length++) {
        name[length] = default_names[language][chara_no][length];
    }

    NameSelect.name_pos = length;

    for (; length < 32; length++) {
        name[length] = 0;
    }
}

// clang-format off
/** The extra spacing to the left and right of each character from code 0xA2 upwards. */
s8 AlphabetEtcOffset[163][2] = {
    {1, 0}, {0, 1}, {1, 1}, {0, 1}, {1, 0}, {2, 2}, {0, 1}, {1, 2},
    {2, 4}, {4, 4}, {2, 2}, {0, 2}, {0, 1}, {0, 2}, {2, 2}, {1, 1},
    {0, 0}, {1, 1}, {1, 1}, {0, 0}, {2, 1}, {0, 2}, {0, 0}, {0, 2},
    {2, 2}, {1, 2}, {2, 1}, {1, 2}, {1, 1}, {2, 1}, {2, 2}, {2, 2},
    {1, 1}, {1, 3}, {3, 3}, {4, 2}, {1, 3}, {2, 4}, {0, 0}, {1, 1},
    {2, 1}, {1, 2}, {2, 1}, {1, 3}, {1, 2}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 2}, {1, 2}, {1, 1}, {0, 8}, {1, 1}, {-1, 4}, {3, 4},
    {2, 2}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 2}, {0, 1}, {0, 0},
    {0, 1}, {6, 0}, {-4, 7}, {0, 0}, {0, 0}, {1, 1}, {1, 0}, {0, 1},
    {3, 2}, {2, 3}, {4, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 3}, {3, 4},
    {1, 2}, {4, 6}, {1, 2}, {2, 2}, {1, 2}, {2, 1}, {2, 2}, {3, 1},
    {1, 1}, {1, 2}, {4, 6}, {2, 8}, {2, 8}, {2, 2}, {1, 3}, {1, 4},
    {1, 2}, {0, 0}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 2},
    {1, 1}, {1, 1}, {1, 1},
};
// clang-format on

/**
 * Gives the kerning between two name characters.
 *
 * @mangled GetFontLRTumeW__Fiii
 * @address 0x23C880
 * @size 0x78
 */
static int GetFontLRTumeW(int position, int left_code, int char_code) {
    int kerning = 0;

    if (char_code >= 0xA2 && char_code < 0x100) {
        kerning += AlphabetEtcOffset[char_code - 0xA2][0];
    }

    if (position - 1 >= 0 && char_code >= 0xA2 && char_code < 0x100) {
        kerning += AlphabetEtcOffset[char_code - 0xA2][1];
    }

    return kerning;
}

/**
 * Draws a party member's name on the character-select page.
 *
 * @mangled CharaSelectNameDraw2__FiiPsPP8CTexturei
 * @address 0x23C900
 * @size 0x250
 */
void CharaSelectNameDraw2(int x, int y, short *name, CTexture **textures, int blend_mode) {
    if (name == NULL) {
        return;
    }

    int condensed;
    int draw_x;
    CTexture *texture;
    int last;

    last = 9;

    while (name[last] == 0 && last > 0) {
        last--;
    }

    condensed = 0;
    int width = (last + 1) * 0x16;

    if (width >= 0xB0) {
        condensed = 1;
    }

    int kerning = 0;

    for (int slot = last; slot >= 0; slot--) {
        kerning += GetFontLRTumeW(slot, name[slot - 1], name[slot]);
    }

    if (condensed) {
        width -= kerning;
    }

    draw_x = x + 0x44 + (width >> 1) - ((width / (last + 1)) >> 1);

    for (; last >= 0; last--) {
        int cell_x;
        int cell_y;
        texture = GetNameTextureInfo(textures, name[last], cell_x, cell_y);
        CRect_i_ cell_rect(cell_x, cell_y, 0x16, 0x17);

        if (texture != NULL) {
            DrawMenu2DSprite(texture, CRect_i_(draw_x + 2, y + 2, 0x16, 0x15), cell_rect, 10, 10, 10, blend_mode);
            DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 0x16, 0x15), cell_rect, blend_mode);
        }

        int kerning_step = 0;

        if (last >= 0 && condensed == 1) {
            kerning_step = GetFontLRTumeW(last, name[last - 1], name[last]);
        }

        draw_x -= 0x14 - kerning_step;
    }
}

/**
 * Draws a party member's name centred on the save board, with a shadow and a top-to-bottom gradient.
 *
 * @mangled DrawSaveBoardCharaName2__FiiPsPP8CTexture6spRGBA6spRGBA
 * @address 0x23CB50
 * @size 0x284
 */
void DrawSaveBoardCharaName2(int x, int y, s16 *name, CTexture **textures, spRGBA top_color, spRGBA bottom_color) {
    spRGBA    shadow_top = {10, 10, 10, top_color.a};
    spRGBA    shadow_bottom = {10, 10, 10, bottom_color.a};
    int       condensed;
    int       draw_x;
    CTexture *texture;
    int       last;

    last = 9;

    while (name[last] == 0 && last > 0) {
        last--;
    }

    condensed = 0;
    int width = (last + 1) * 0x16;

    if (width >= 0xB0) {
        condensed = 1;
    }

    int kerning = 0;

    for (int slot = last; slot >= 0; slot--) {
        kerning += GetFontLRTumeW(slot, name[slot - 1], name[slot]);
    }

    if (condensed == 1) {
        width -= kerning;
    }

    draw_x = x + 0x36 + (width >> 1) - ((width / (last + 1)) >> 1);

    for (; last >= 0; last--) {
        int cell_x;
        int cell_y;
        texture = GetNameTextureInfo(textures, name[last], cell_x, cell_y);
        CRect_i_ cell_rect(cell_x, cell_y, 0x16, 0x17);

        DrawMenu2DSprite(texture, CRect_i_(draw_x + 2, y + 2, 0x16, 0x15), cell_rect, &shadow_top, &shadow_top, &shadow_bottom, &shadow_bottom);
        DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 0x16, 0x15), cell_rect, &top_color, &top_color, &bottom_color, &bottom_color);

        int kerning_step = 0;

        if (last >= 0 && condensed == 1) {
            kerning_step = GetFontLRTumeW(last, name[last - 1], name[last]);
        }

        draw_x -= 0x14 - kerning_step;
    }
}

/**
 * Gives how wide a party member's name draws.
 *
 * @mangled GetMsgLengthCharaName__Fi
 * @address 0x23CDE0
 * @size 0x78
 */
int GetMsgLengthCharaName(int chara_no) {
    if (chara_no < 0 || chara_no > 5) {
        return 0;
    }

    s16 *name = SaveData->GetCharaName(chara_no);
    int  length = 0;

    while (*name != 0 && length < 10) {
        name++;
        length++;
    }

    return length;
}

/** State of the storybook that plays before the game begins. */
OPENING_BOOK OpenBook;

/** The work area the storybook hands to the name-entry screen once its pages are read. */
u_long128 *OpeningReadBuf;

/**
 * Opens the storybook that begins the game.
 *
 * @mangled InitOpeningBook__FP1Pi
 * @address 0x23CE60
 * @size 0xB0
 */
void InitOpeningBook(u_long128 *buffer, int *tex_blocks) {
    u_long128 *load_buffer = buffer;

    if (load_buffer == NULL) {
        load_buffer = (u_long128 *) read_buffer;
    }

    load_buffer = MenuCalcBufAlignment(load_buffer);
    StartReadBG();
    LoadFileBGMenuData("openbook.pak", load_buffer);
    OpenBook.tex_block = tex_blocks[0];
    OpenBook.name_tex_block = tex_blocks[1];
    OpenBook.open = false;
    OpenBook.step = OPENING_BOOK_LOAD;
    OpenBook.page = 0;
    OpenBook.fade = 128;
    OpenBook.text_alpha = 0;
}

/**
 * Steps the storybook that plays before the game begins: reads its pages, fades them in and out on the pad, then hands over to naming the hero.
 *
 * @mangled OpeningBookKey__Fv
 * @address 0x23CF10
 * @size 0x664
 */
int OpeningBookKey() {
    int result;

    ReadBG();
    result = 0;

    switch (OpenBook.step) {
        case OPENING_BOOK_LOAD:
            if (OpenBook.open == 0 && ReadBGSync() == 0) {
                LOADTEXTURE_INFO2 info[3] = {
                    {"#frame_image#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                    {NULL,                                       0, 0},
                    {NULL,                                       0, 0}
                };

                info[0].block_no = OpenBook.tex_block;
                info[1].block_no = OpenBook.tex_block;
                BG_READ_INFO *read_file = GetReadBGFile(0);
                info[1].name = (char *) GetPackFile((u_int *) read_file->buffer, "opentex.img", NULL);
                TexManager.DeleteTextureBlock(OpenBook.tex_block);
                TexManager.CleanUpTextureList();
                TexManager.LoadTextureBlockEX(-1, info);
                OpeningReadBuf = read_file->buffer + (read_file->size >> 4) + 1;
                OpeningReadBuf = MenuCalcBufAlignment(OpeningReadBuf);
                short *messages = (short *) GetPackFile((u_int *) read_file->buffer, "opmes.bin", NULL);

                CommonMenuMes2.text_columns = MES_WIN_COLUMNS;
                CommonMenuMes2.text_rows = 10;
                CommonMenuMes2.text_len = 0;
                CommonMenuMes2.text_width = 0;
                CommonMenuMes2.text_height = 0;
                CommonMenuMes2.fade = 0.0f;
                CommonMenuMes2.fade_in = true;
                CommonMenuMes2.text_rate = CommonMenuMes2.text_rate_set;
                CommonMenuMes2.waiting = false;
                CommonMenuMes2.text_at = 0.0f;
                CommonMenuMes2.text_no = 0;
                CommonMenuMes2.text_from = 0;
                CommonMenuMes2.page_from = 0;
                CommonMenuMes2.InitMesWinTbl();
                CommonMenuMes2.clut_now = CommonMenuMes2.clut_default;
                CommonMenuMes2.wait = 0;
                CommonMenuMes2.blink = 0;
                CommonMenuMes2.auto_page_wait = 0;
                CommonMenuMes2.mes_made = -1;
                CommonMenuMes2.edge_alpha = 0x80;

                for (int i = 0; i < 10; i++) {
                    CommonMenuMes2.mes_no[i] = -1;
                }

                for (int i = 0; i < 8; i++) {
                    CommonMenuMes2.values[i] = 0;
                }

                CommonMenuMes2.value = 0;
                CommonMenuMes2.value_signed = false;
                CommonMenuMes2.value_show = true;
                CommonMenuMes2.value_narrow = false;
                CommonMenuMes2.space_width = -1;
                CommonMenuMes2.space_area = -1;
                CommonMenuMes2.cursor_row = -1;
                CommonMenuMes2.cursor_y = 0;
                CommonMenuMes2.cursor_lit = false;

                for (int i = 0; i < 10; i++) {
                    CommonMenuMes2.line_pos[i].x = -1;
                    CommonMenuMes2.line_pos[i].y = -1;
                }

                CommonMenuMes2.SetMesFukidashi(MES_FUKIDASHI_LARGE_TABLE_EDGE);
                CommonMenuMes2.text_rate = 0.0f;
                CommonMenuMes2.text_rate_set = 0.0f;
                CommonMenuMes2.page_arrow = true;
                CommonMenuMes2.centre_rows = true;
                CommonMenuMes2.columns = 40;
                CommonMenuMes2.rows = 3;
                int language = GetMenuLangFlag();

                if (language > LANG_JAPANESE) {
                    CommonMenuMes2.char_width--;
                }

                CommonMenuMes2.tex_block = 0x1A;
                CommonMenuMes2.tex_buff = MesWinTexBuff_02;
                CommonMenuMes2.SetBuff(messages);
                CommonMenuMes2.edge_alpha = OpenBook.text_alpha;
                OpenBook.open = true;
                OpenBook.fade = 0x80;
                s8 page_text_y[7] = {20, 16, 16, 16, 16, 16, 16};
                CommonMenuMes2.text_y = page_text_y[language] + 300;
                CommonMenuMes2.mes_made = -1;
                CommonMenuMes2.MakeMesWin(100);
            }

            if (OpenBook.open != 0) {
                OpenBook.fade--;

                if (OpenBook.fade <= 0) {
                    OpenBook.fade = 0;
                }
            }

            if (OpenBook.fade <= 0) {
                OpenBook.step = OPENING_BOOK_TEXT_IN;
                OpenBook.text_alpha = 0;
            }

            break;
        case OPENING_BOOK_TEXT_IN:
            OpenBook.text_alpha += 2;

            if (OpenBook.text_alpha >= 0x80) {
                OpenBook.text_alpha = 0x80;
                OpenBook.step = OPENING_BOOK_WAIT;
                OpenBook.fade = 0;
            }

            break;
        case OPENING_BOOK_TEXT_OUT:
            OpenBook.text_alpha -= 2;

            if (OpenBook.text_alpha <= 0) {
                OpenBook.text_alpha = 0;
                OpenBook.page++;
                CommonMenuMes2.MakeMesWin(OpenBook.page + 100);
                OpenBook.step = OPENING_BOOK_TEXT_IN;
            }

            break;
        case OPENING_BOOK_WAIT:
            if (GamePad.Down(PAD_CROSS)) {
                OpenBook.step = OPENING_BOOK_TEXT_OUT;

                if (OpenBook.page >= 11) {
                    OpenBook.step = OPENING_BOOK_FINISH;
                }
            }

            if (DebugMode && GamePad.Down2(PAD_CROSS)) {
                OpenBook.step = OPENING_BOOK_FINISH;
            }

            break;
        case OPENING_BOOK_FINISH:
            OpenBook.text_alpha--;

            if (OpenBook.text_alpha <= 0 && OpenBook.step == OPENING_BOOK_FINISH && OpenBook.text_alpha <= 0) {
                CommonMenuMes2.page_arrow = false;
                InitNameRegist(0, OpenBook.name_tex_block, OpeningReadBuf);
                OpenBook.step = OPENING_BOOK_NAME_ENTRY;
            }

            break;
        case OPENING_BOOK_NAME_ENTRY:
            result = NameEnterKey();
            break;
    }

    return result;
}

/**
 * Draws the storybook page by page.
 *
 * @mangled OpeningBookDraw__Fv
 * @address 0x23D580
 * @size 0x134
 */
void OpeningBookDraw() {
    setbilinear(0);
    AllFadeForMenu(0x80);

    if (OpenBook.open == 0) {
        return;
    }

    MenuTextureReload(OpenBook.tex_block);
    DrawFullSizePicture(TexManager.GetTexture("openbook", -1), 0, 0, 0x80);

    switch (OpenBook.step) {
        case OPENING_BOOK_LOAD:
        case OPENING_BOOK_TEXT_IN:
        case OPENING_BOOK_WAIT:
        case OPENING_BOOK_TEXT_OUT:
        case OPENING_BOOK_FINISH:
            MenuTextureReload(CommonMenuMes2.tex_block);
            CommonMenuMes2.edge_alpha = OpenBook.text_alpha;
            GetMenuCommonPutXY(&CommonMenuMes2, 0x14C - CommonMenuMes2.char_width);
            CommonMenuMes2.Step();
            CommonMenuMes2.DrawMesWin();
            AllFadeForMenu(OpenBook.fade);
            break;
        case OPENING_BOOK_NAME_ENTRY:
        case OPENING_BOOK_UNK_6:
            NameEnterDraw();
            break;
    }
}
