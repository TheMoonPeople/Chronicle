#include "battle_globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "rect.hpp"
#include "runscript.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

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
enum class NameSelectArea : s16 {
    Keyboard    = 4, /**< The keyboard's keys and voicing column. */
    Tabs        = 5, /**< The tabs above the keyboard. */
    Help        = 6, /**< The help message. */
    Confirm     = 7, /**< The prompt confirming the name. */
    EmptyNotice = 8, /**< The notice that an empty name became the default name. */
};

// clang-format on

/**
 * What the name-entry screen is doing, as NAME_SELECT::state holds it.
 */
// clang-format off
enum class NameSelectState : s16 {
    Idle     = 0, /**< Taking input. */
    FadeIn   = 1, /**< Fading in. */
    FadeOut  = 2, /**< Fading out. */
    Exit     = 3, /**< Closing. */
    TabFlash = 6, /**< Flashing the tab that was pressed. */
    Help     = 8, /**< Showing the help message. */
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
enum class NameEnterAction : s32 {
    None          = -1,  /**< Nothing. */
    Decide        = 49,  /**< Checks the name and asks to confirm it. */
    Finish        = 50,  /**< Accepts the name and fades out. */
    Input         = 100, /**< Enters the key under the cursor. */
    Delete        = 200, /**< Deletes the character under the name's cursor. */
    Backspace     = 250, /**< Blanks the character before the name's cursor. */
    Insert        = 300, /**< Inserts a space under the name's cursor. */
    NextMode      = 400, /**< Shows the next keyboard. */
    PrevMode      = 450, /**< Shows the previous keyboard. */
    Left          = 500, /**< Moves the name's cursor left. */
    Right         = 600, /**< Moves the name's cursor right. */
    DefaultName   = 700, /**< Restores the default name. */
    Help          = 800, /**< Shows the help message. */
    ToggleVoicing = 900, /**< Toggles the voicing or case of the character under the name's cursor; never requested. */
};

// clang-format on

/**
 * What CheckName reports.
 */
// clang-format off
enum class NameCheckResult : s32 {
    Rejected = 0, /**< Rejected and cleared. */
    Ok       = 1, /**< Accepted. */
    Empty    = 2, /**< Blank; the default name is restored. */
};

// clang-format on

/**
 * Steps of the opening book, as OPENING_BOOK::step holds them.
 */
// clang-format off
enum class OpeningBookStep : s16 {
    Load      = 0, /**< Loading and fading the cover. */
    TextIn    = 1, /**< Text fading in. */
    Wait      = 2, /**< Waiting for cross. */
    TextOut   = 3, /**< Text fading out. */
    Finish    = 4, /**< Fading out before name entry. */
    NameEntry = 5, /**< Name entry. */
    Unk6      = 6, /**< Drawn but never set. */
};

// clang-format on

/**
 * Holds the state of the name-entry screen.
 */
struct NAME_SELECT {
    s16             chara_no;      /**< The party member being named. */
    NameSelectArea  area;          /**< The part of the screen the cursor is in. */
    s16             name_pos;      /**< The position in the name the next character goes to. */
    s16             side_row;      /**< The row of the voicing column the cursor is on, or 0 when it is on the keyboard. */
    s16             pushed_tab;    /**< The tab that was last pressed. @see NameSelectTab. */
    s16             input_mode;    /**< The keyboard being shown. @see NameInputMode. */
    s32             cursor;        /**< The key or tab the cursor is on. @see NameSelectTab. */
    NameSelectState state;         /**< What the screen is doing: fading, flashing a tab or leaving. */
    s32             state_count;   /**< How many frames the state has lasted. */
    float           cursor_x;      /**< Where the hand cursor is drawn across the screen. */
    float           cursor_y;      /**< Where the hand cursor is drawn down the screen. */
    s32             frame;         /**< How many frames the screen has been drawn, for the cursor's sway. */
    s16             language;      /**< The language the menus are in. */
    s16             texture_block; /**< The texture block the screen's textures are read into. */
    s16             loaded;        /**< Whether the screen's textures have been read. */
};

STATIC_ASSERT(sizeof(NAME_SELECT) == 0x2C);

/** The state of the name-entry screen. */
NAME_SELECT NameSelect;

/** The name-entry screen's frame, tabs and cursor. */
CTexture *NameTemp;

namespace {

/**
 * State of the storybook that plays before the game begins.
 */
struct OPENING_BOOK {
    s16             open;           /**< Whether the storybook's pages have been read and it draws. */
    s16             tex_block;      /**< Texture block the storybook's pages are loaded into. */
    s16             name_tex_block; /**< Texture block handed to the name-entry screen that follows the storybook. */
    s16             page;           /**< Index of the page being shown; its message is page plus 100. */
    OpeningBookStep step;           /**< Which part of the opening the storybook is showing. */
    s16             fade;           /**< Strength of the full-screen fade drawn over the storybook. */
    s16             text_alpha;     /**< Alpha the page text fades in and out through. */
};

STATIC_ASSERT(sizeof(OPENING_BOOK) == 0xE);

s16 *CharaName;

CTexture *CharaFace;
CTexture *HiraTex;
CTexture *KataTex;
CTexture *AlphaTex;
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

char NameEntryImageDescriptor[] = "#frame_image_name#640#" SCREEN_HEIGHT_STR "#4";
char NameEntryTextureFile[] = "nameregi.img";
char NameEntryTempTexture[] = "nametemp";
char NameEntryHiraganaTexture[] = "hira";
char NameEntryKatakanaTexture[] = "kata";
char NameEntryAlphabetTexture[] = "alphabet";
char NameEntryFaceTexture[] = "charaface";
char NameEntryMessageFile[] = "nameregi.bin";
char NameEntryMessageFile2[] = "nameregi2.bin";
char NameEntryFrameTexture[] = "frame_image";

} // namespace

void GlobalNameInit() {
    for (int chara_no = 0; chara_no < 6; chara_no++) {
        NameDefaultSet(chara_no);
    }
}

void InitNameRegist(int chara_no, int texture_block, u_long128 *buffer) {
    StartReadBG();

    if (buffer == nullptr) {
        buffer = reinterpret_cast<u_long128 *>(read_buffer);
    }

    LoadFileBGMenuData("nameregi.pak", MenuCalcBufAlignment(buffer));
    NameSelect.language = GetMenuLangFlag();
    GamePad.MenuModeOn(0x78);
    GamePad.SetAutoRepeat(PAD_DPAD, 30, 9);

    NameSelect.chara_no = chara_no;
    NameSelect.area = NameSelectArea::Keyboard;
    NameSelect.cursor = 0;

    if (NameSelect.language == LANG_JAPANESE) {
        NameSelect.input_mode = NAME_INPUT_KATAKANA;
    } else {
        NameSelect.input_mode = NAME_INPUT_ALPHABET;
    }

    NameSelect.side_row = 0;
    NameSelect.state = NameSelectState::FadeIn;
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

void ExitNameEnterFunc() {
    int texture_blocks[2] = {NameSelect.texture_block, -1};
    MenuTextureDelete(texture_blocks);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

namespace {

/**
 * Gives the texture and cell one name character draws from.
 */
CTexture *GetNameTextureInfo(CTexture **textures, int char_code, int &cell_x, int &cell_y) {
    CTexture *texture = nullptr;

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
 * Draws the name being entered.
 */
void DrawCharaName(int x, int y, int brightness, int blend_mode) {
    int       draw_x = x;
    CTexture *textures[4] = {AlphaTex, KataTex, HiraTex, EuroTex};

    for (int slot = 0; slot < 10; slot++) {
        int       cell_x = 0;
        int       cell_y = 0;
        CTexture *texture = GetNameTextureInfo(textures, CharaName[slot], cell_x, cell_y);
        DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 22, 22), CRect_i_(cell_x, cell_y, 22, 22), brightness, brightness, brightness, blend_mode);
        draw_x += 22;
    }
}

/**
 * Draws the four corners of the frame around the name being entered, pulling them inward as the frame counter cycles.
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
        DrawMenu2DSprite(NameTemp, CRect_i_(corner[corner_no][0], corner[corner_no][1], 12, 12), CRect_i_(cell[corner_no][0], cell[corner_no][1], 16, 16), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);
    }
}

/**
 * Draws the top of the name-entry screen: the party member's face and title, the name being entered and the cursor over it.
 */
void DrawCharaNameUp(int x, int y, int brightness, int blend_mode) {
    int chara_no = NameSelect.chara_no;
    int left = x - 2;
    int top = y + 14;
    int face_x = 0;
    int face_y = chara_no * 106;
    int name_length = 0;

    if (chara_no > 2) {
        face_x = 106;
        face_y = (chara_no - 3) * 106;
    }

    DrawMenu2DSprite(CharaFace, CRect_i_(left, top, 88, 88), CRect_i_(face_x, face_y, 106, 106), blend_mode);
    DrawMenuHelpWindow(TexManager.GetTexture("window", -1), -1, left + 102, top + 16, 8.6f, 1.0f, blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 97, top + 16, 26, 23), CRect_i_(0, 256, 26, 24), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 123, top + 16, 210, 23), CRect_i_(26, 256, 172, 24), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);
    DrawMenu2DSprite(NameTemp, CRect_i_(left + 333, top + 16, 26, 23), CRect_i_(198, 256, 26, 24), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);

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

    if (NameSelect.area < NameSelectArea::Help) {
        CRect_i_ destination(left + 89, top + 25, 24, 24);
        CRect_i_ source(24, 472, 24, 24);
        DrawMenu2DSprite(NameTemp, destination, source, blend_mode);
        source.x += 25;
        destination.x = left + 331;
        DrawMenu2DSprite(NameTemp, destination, source, blend_mode);
    }

    left = x + 120;
    top = y + 58;
    DrawCharaName(left, top, brightness, blend_mode);

    left = x + 122;
    top = y + 82;

    if (NameSelect.area == NameSelectArea::Confirm) {
        for (name_length = 10; name_length > 0; name_length--) {
            if (CharaName[name_length - 1] != 0 && CharaName[name_length - 1] != 0xE6) {
                break;
            }
        }
    }

    for (int slot = 0; slot < 10; slot++) {
        int underline_brightness = brightness;

        if (NameSelect.area == NameSelectArea::Confirm && name_length <= slot) {
            underline_brightness = 0x38;
        }

        DrawMenu2DSprite(NameTemp, CRect_i_(left, top, 18, 2), CRect_i_(470, 328, 18, 2), static_cast<u8>(underline_brightness), static_cast<u8>(underline_brightness), static_cast<u8>(underline_brightness), blend_mode);
        left += 22;
    }

    int cursor_slot = NameSelect.name_pos;

    if (cursor_slot >= 10) {
        cursor_slot = 9;
    }

    left = x + 110 + cursor_slot * 22;
    top = y + 48;

    switch (NameSelect.state) {
        case NameSelectState::FadeIn:
        case NameSelectState::FadeOut:
            break;
        default:
            if (NameSelect.area < NameSelectArea::Help) {
                DrawNameRegiWaku(left, top, 30, brightness, blend_mode);
                int bob_frame = NameSelect.frame - 15;

                if (NameSelect.area >= NameSelectArea::Help) {
                    bob_frame = 0;
                }

                top = (top - 20) + 4.0f * sinf(0.20943952f * (bob_frame % 31));
                DrawMenu2DSprite(NameTemp, CRect_i_(left + 20, top, 24, 24), CRect_i_(488, 328, 24, 24), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);
            }

            break;
    }
}

} // namespace

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

namespace {

/**
 * Draws the accented-character rows of the alphabet keyboard for one European language. PAL only.
 */
void DrawEuroSpecialFont(int x, int y, int language, int brightness, int blend_mode) {
    EuroTex = TexManager.GetTexture("euro", -1);

    for (int row = 0; row < 2; row++) {
        for (int column = 0; column < 13; column++) {
            int key = column + row * 13;

            if (key >= 26) {
                break;
            }

            int char_code = menu_euro_codetbl[language][0][key] - 0x104;

            if (char_code == -2) {
                break;
            }

            if (char_code >= 0) {
                int cell_x = (char_code % 10) * 22;
                int cell_y = (char_code / 10) * 22;
                int key_x = x + column * 34;
                int key_y = y + row * 26;
                DrawMenu2DSprite(EuroTex, CRect_i_(key_x + 1, key_y + 1, 22, 22), CRect_i_(cell_x, cell_y, 22, 23), 0, 0, 0, (blend_mode * 80) >> 7);
                DrawMenu2DSprite(EuroTex, CRect_i_(key_x, key_y, 22, 22), CRect_i_(cell_x, cell_y, 22, 23), static_cast<u8>(brightness), static_cast<u8>(brightness), static_cast<u8>(brightness), blend_mode);
            }
        }
    }
}

/**
 * Gives where a keyboard tab is drawn across the name-entry frame for a language. PAL only.
 */
s16 Get_NameTemp_PutX(int language, int tab) {
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

/**
 * Draws the character keyboard the name is entered from.
 */
void DrawNameTemplete(int x, int y, int brightness, int blend_mode) {
    int input_mode = NameSelect.input_mode;
    int pushed_tab = NameSelect.pushed_tab;
    int draw_x = 0;
    int draw_y = 0;
    int tex_u = 0;
    int tex_v = 0;
    int i = 0;
    int key_x = 0;
    int key_y = 0;
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

    if (NameSelect.state == NameSelectState::TabFlash) {
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
    CTexture *texture = nullptr;

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

                            [[fallthrough]];
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
        default:
            break;
    }
}

/**
 * Reports whether two names differ: 0 when all ten characters match, 1 otherwise.
 */
int NameCompare(const short *first, const short *second) {
    int matching = 0;

    for (int i = 0; i < 10; i++) {
        if (first[i] == second[i]) {
            matching++;
        }
    }

    if (matching >= 10) {
        return 0;
    }

    return 1;
}

/**
 * Reports whether the entered name may be used.
 */
NameCheckResult CheckName() {
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
        return NameCheckResult::Empty;
    }

    for (const auto &blank_name : blank_names) {
        if (NameCompare(CharaName, blank_name) == 0) {
            return NameCheckResult::Rejected;
        }
    }

    for (int other_chara_no = NameSelect.chara_no - 1; 0 <= other_chara_no; other_chara_no--) {
        if (NameCompare(CharaName, SaveData->GetCharaName(other_chara_no)) == 0) {
            return NameCheckResult::Rejected;
        }
    }

    return NameCheckResult::Ok;
}

} // namespace

void NameEnterDraw() {
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;

    setbilinear(0);

    if (NameSelect.chara_no != 0) {
        AllFadeForMenu(0x80);
    }

    if (NameSelect.loaded == 0) {
        ReadBG();

        if (ReadBGSync() == 0) {
            LOADTEXTURE_INFO2 info[3] = {
                {.name = NameEntryImageDescriptor, .block_no = NameSelect.texture_block, .mipmap = 0},
                {.name = nullptr,                  .block_no = NameSelect.texture_block, .mipmap = 0},
                {.name = nullptr,                  .block_no = 0,                        .mipmap = 0},
            };

            BG_READ_INFO *read_file = GetReadBGFile(0);
            info[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(read_file->buffer), NameEntryTextureFile, nullptr));
            TexManager.DeleteTextureBlock(NameSelect.texture_block);
            TexManager.LoadTextureBlockEX(-1, info);
            NameTemp = TexManager.GetTexture(NameEntryTempTexture, -1);
            HiraTex = TexManager.GetTexture(NameEntryHiraganaTexture, -1);
            KataTex = TexManager.GetTexture(NameEntryKatakanaTexture, -1);
            AlphaTex = TexManager.GetTexture(NameEntryAlphabetTexture, -1);
            EuroTex = TexManager.GetTexture("euro", -1);
            printf("euro tex is %p\n", static_cast<void *>(EuroTex));
            CharaFace = TexManager.GetTexture(NameEntryFaceTexture, -1);
            NameSelect.loaded = true;

            auto *pack = reinterpret_cast<u_int *>(read_file->buffer);
            auto *messages = reinterpret_cast<short *>(GetPackFile(pack, NameEntryMessageFile, nullptr));
            auto *messages2 = reinterpret_cast<short *>(GetPackFile(pack, NameEntryMessageFile2, nullptr));
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

    int language = NameSelect.language;
    MenuTextureReload(NameSelect.texture_block);
    int chara_no = NameSelect.chara_no;
    int fade = 0x80;
    int brightness = 0x80;

    if (NameSelect.state == NameSelectState::FadeIn) {
        fade = NameSelect.state_count * 5;

        if (fade > 0x80) {
            fade = 0x80;
            NameSelect.state = NameSelectState::Idle;
        }
    }

    if (NameSelect.area >= NameSelectArea::Help) {
        brightness = 0x38;
    }

    CTexture *frame_texture = TexManager.GetTexture(NameEntryFrameTexture, -1);

    if ((chara_no != 0 || NameSelect.loaded == 0) && frame_texture != nullptr) {
        FrameImageDraw(0x80, 0x80);
    }

    if (NameSelect.loaded != 0) {
        MenuTextureReload(NameSelect.texture_block);
        DrawMenu2DSprite(NameTemp, CRect_i_(0x30, 0x21, 0x3C, 0x27), CRect_i_(0x1C0, 0x100, 0x3C, 0x28), fade);
        DrawMenu2DSprite(NameTemp, CRect_i_(0x6E, 0x24, 0xAE, 0x1F), CRect_i_(0xC0, 0x178, 0xAE, 0x20), fade);

        int name_color = brightness;

        if (NameSelect.area >= NameSelectArea::Help) {
            name_color = 0x80;
        }

        DrawCharaNameUp(0x3E, 0x36, name_color, fade);
        DrawNameTemplete(0x42, 0x9E, brightness, fade);

        switch (NameSelect.area) {
            case NameSelectArea::Tabs:
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
            case NameSelectArea::Keyboard: {
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
            default:
                break;
        }

        if (NameSelect.area < NameSelectArea::Help) {
            if (abs(static_cast<int>(cursor_x - NameSelect.cursor_x)) < 0x144) {
                NameSelect.cursor_x += (cursor_x - NameSelect.cursor_x) / 4.0f;
                NameSelect.cursor_y += (cursor_y - NameSelect.cursor_y) / 4.0f;
            } else {
                NameSelect.cursor_x = cursor_x;
            }
        }

        switch (NameSelect.state) {
            case NameSelectState::FadeIn:
            case NameSelectState::FadeOut:
                break;
            default: {
                if (NameSelect.area == NameSelectArea::Keyboard && 230.0f <= NameSelect.cursor_y) {
                    DrawNameRegiWaku(static_cast<int>(16.0f + cursor_x), static_cast<int>(cursor_y - 6.0f), 0x1C, brightness, fade);
                }

                // The hand sways on its own counter, which holds still while a tab flashes.
                static int sway_frame = 0;

                if (NameSelect.state != NameSelectState::TabFlash) {
                    sway_frame++;
                }

                if (sway_frame > 100000) {
                    sway_frame = 0;
                }

                cursor_x = NameSelect.cursor_x + 5.0f * cosf(0.07853981852531433f * static_cast<float>(sway_frame));
                cursor_y = NameSelect.cursor_y + 3.0f * sinf(0.13089969754219055f * static_cast<float>(sway_frame));

                if (NameSelect.area < NameSelectArea::EmptyNotice) {
                    CRect_i_ hand_rect(0x1C0, 0x128, 0x20, 0x20);

                    DrawMenu2DSprite(NameTemp, CRect_i_(static_cast<int>(2.0f + cursor_x), static_cast<int>(2.0f + cursor_y), 0x20, 0x20), hand_rect, 0, 0, 0, (fade * 0x50) >> 7);
                    DrawMenu2DSprite(NameTemp, CRect_i_(static_cast<int>(cursor_x), static_cast<int>(cursor_y), 0x20, 0x20), hand_rect, brightness, brightness, brightness, fade);
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

        if (NameSelect.area >= NameSelectArea::Help) {
            int win_x = 0xAE;
            int win_y = 0xAA;
            int win_width = 0x12C;

            switch (NameSelect.area) {
                case NameSelectArea::Confirm:
                    win_x = 0xD8;
                    win_y = 0xB8;
                    win_width = 0xDA;
                    break;
                case NameSelectArea::EmptyNotice:
                    win_x = 0xBA;
                    win_y = 0xB8;
                    win_width = 0x116;
                    break;
                default:
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
            case NameSelectArea::Help:
            case NameSelectArea::Confirm:
            case NameSelectArea::EmptyNotice:
                CommonMenuMes2.edge_alpha = fade;
                CommonMenuMes2.Step();
                CommonMenuMes2.DrawMesWin();
                break;
            default:
                break;
        }

        setbilinear(0);

        switch (NameSelect.state) {
            case NameSelectState::FadeIn:
                fade = std::max(0x80 - NameSelect.state_count * 6, 0);

                if (NameSelect.chara_no != 0) {
                    FrameImageDraw(0x80, fade);
                }

                break;
            case NameSelectState::Exit:
                ExitNameEnterFunc();
                [[fallthrough]];
            case NameSelectState::FadeOut:
                fade = std::min(NameSelect.state_count * 3, 0x80);

                AllFadeForMenu(fade);
                break;
            default:
                break;
        }

        if (NameSelect.state != NameSelectState::Idle) {
            NameSelect.state_count++;
        }

        setbilinear(1);
    }
}

s32 NameEnterKey() {
    if (NameSelect.loaded == 0) {
        return 0;
    }

    int language = NameSelect.language;
    int chara_no = NameSelect.chara_no;

    switch (NameSelect.state) {
        case NameSelectState::FadeOut:
            if (NameSelect.state_count >= 84) {
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                NameSelect.state = NameSelectState::Exit;
                return 1;
            }

            return 0;
        case NameSelectState::TabFlash:
            if (NameSelect.state_count > 8) {
                NameSelect.state = NameSelectState::Idle;
            }

            break;
        default:
            break;
    }

    int             cursor = NameSelect.cursor;
    int             side_row = NameSelect.side_row;
    int             name_pos = NameSelect.name_pos;
    NameEnterAction action = NameEnterAction::None;
    int             mes_no = -1;

    switch (NameSelect.area) {
        case NameSelectArea::Help:
            mes_no = chara_no + 100;

            if (GamePad.Down(PAD_CROSS) || GamePad.Down(PAD_CIRCLE)) {
                NameSelect.area = NameSelectArea::Tabs;
                NameSelect.state = NameSelectState::Idle;
            }

            break;
        case NameSelectArea::Confirm:
        case NameSelectArea::EmptyNotice:
            if (NameSelect.area == NameSelectArea::Confirm) {
                mes_no = 91;
            }

            if (NameSelect.area == NameSelectArea::EmptyNotice) {
                mes_no = 95;
            }

            if (GamePad.Down(PAD_CROSS)) {
                NameSelect.state = NameSelectState::Idle;
                action = NameEnterAction::Finish;
            } else if (GamePad.Down(PAD_CIRCLE)) {
                NameSelect.area = NameSelectArea::Keyboard;
                NameSelect.state = NameSelectState::Idle;
            }

            break;
        case NameSelectArea::Keyboard:
            if (NameSelect.input_mode > NAME_INPUT_HIRAGANA) {
                NameSelect.side_row = 0;
            }

            if (NameSelect.side_row == 0) {
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
                        default:
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
                        NameSelect.area = NameSelectArea::Tabs;

                        if (NameSelect.cursor < 4) {
                            NameSelect.cursor += 6;
                        } else if (NameSelect.cursor >= 4 && NameSelect.cursor < 7) {
                            NameSelect.cursor = NAME_TAB_HELP;
                        } else {
                            NameSelect.cursor = NAME_TAB_OK;
                        }
                    }
                }
            } else {
                int column = -1;

                if (GamePad.Down(PAD_UP)) {
                    if (NameSelect.side_row == 1) {
                        NameSelect.side_row = 0;
                        NameSelect.area = NameSelectArea::Tabs;
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
            }

            if (GamePad.Down(PAD_CROSS)) {
                action = NameEnterAction::Input;
            } else if (GamePad.Down(PAD_CIRCLE)) {
                action = NameEnterAction::Backspace;
            }

            break;
        case NameSelectArea::Tabs: {
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
                    NameSelect.area = NameSelectArea::Keyboard;
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
                                    NameSelect.area = NameSelectArea::Keyboard;
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
                                    NameSelect.area = NameSelectArea::Keyboard;
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
                action = NameEnterAction::Backspace;
            } else if (GamePad.Down(PAD_CROSS)) {
                NameSelect.pushed_tab = NameSelect.cursor;

                if (NameSelect.cursor < NAME_TAB_DEFAULT_NAME) {
                    if (NameSelect.input_mode != NameSelect.cursor) {
                        NameSelect.input_mode = NameSelect.cursor;
                    }
                } else {
                    NameSelect.state = NameSelectState::TabFlash;
                    NameSelect.state_count = 0;
                    NameEnterAction tab_action[7] = {NameEnterAction::DefaultName, NameEnterAction::Decide, NameEnterAction::Left, NameEnterAction::Right, NameEnterAction::Delete, NameEnterAction::Insert, NameEnterAction::Help};
                    action = tab_action[NameSelect.cursor - NAME_TAB_DEFAULT_NAME];
                }
            }

            break;
        }
    }

    if (GamePad.Down(PAD_START) && NameSelect.area < NameSelectArea::Help) {
        action = NameEnterAction::Decide;
    }

    GamePad.Down(PAD_TRIANGLE);

    if (GamePad.Down(PAD_L1)) {
        action = NameEnterAction::Left;
    }

    if (GamePad.Down(PAD_R1)) {
        action = NameEnterAction::Right;
    }

    if (NameSelect.area < NameSelectArea::Help) {
        if (GamePad.Down(PAD_R2)) {
            action = NameEnterAction::NextMode;
        }

        if (GamePad.Down(PAD_L2)) {
            action = NameEnterAction::PrevMode;
        }
    }

    int input_mode = NameSelect.input_mode;

    switch (action) {
        case NameEnterAction::None:
            break;
        case NameEnterAction::Input: {
            int code_base = 0;
            int code = 0;
            int key = NameSelect.cursor;
            int slot = NameSelect.name_pos;

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
                                        [[fallthrough]];
                                    case 47:
                                        key--;
                                        [[fallthrough]];
                                    case 45:
                                    case 39:
                                        key--;
                                        [[fallthrough]];
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
                default:
                    break;
            }

            break;
        }
        case NameEnterAction::Delete:
            for (int i = NameSelect.name_pos + 1; i <= 11; i++) {
                CharaName[i - 1] = CharaName[i];
            }

            break;
        case NameEnterAction::Backspace:
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
        case NameEnterAction::Insert: {
            int slot = NameSelect.name_pos;
            int i = 9;

            for (; i >= slot; i--) {
                CharaName[i + 1] = CharaName[i];
            }

            CharaName[i + 1] = 230;

            for (i = 10; i < 32; i++) {
                CharaName[i] = 230;
            }

            break;
        }
        case NameEnterAction::NextMode:
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
        case NameEnterAction::PrevMode:
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
        case NameEnterAction::Left:
            if (0 < NameSelect.name_pos) {
                if (NameSelect.name_pos >= 10) {
                    NameSelect.name_pos--;
                }

                NameSelect.name_pos--;
            }

            break;
        case NameEnterAction::Right:
            if (NameSelect.name_pos < 10) {
                NameSelect.name_pos++;
            }

            break;
        case NameEnterAction::Help:
            NameSelect.area = NameSelectArea::Help;
            NameSelect.state = NameSelectState::Help;
            break;
        case NameEnterAction::DefaultName:
            NameDefaultSet(NameSelect.chara_no);
            break;
        case NameEnterAction::Decide:
            switch (CheckName()) {
                case NameCheckResult::Ok: {
                    NameSelect.area = NameSelectArea::Confirm;
                    int length = 10;

                    for (; length > 0; length--) {
                        if (CharaName[length - 1] != 0 && CharaName[length - 1] != 230) {
                            break;
                        }
                    }

                    for (int i = 0; i < length; i++) {
                        if (CharaName[i] == 0) {
                            CharaName[i] = 230;
                        }
                    }

                    for (; length < 32; length++) {
                        CharaName[length] = 0;
                    }

                    break;
                }
                case NameCheckResult::Rejected:
                    for (int i = 0; i < 10; i++) {
                        CharaName[i] = 0;
                    }

                    NameSelect.name_pos = 0;
                    break;
                case NameCheckResult::Empty:
                    NameDefaultSet(NameSelect.chara_no);
                    mes_no = 95;
                    NameSelect.area = NameSelectArea::EmptyNotice;
                    break;
            }

            break;
        case NameEnterAction::Finish:
            for (int i = 0; i < 10; i++) {
                printf("CharaName[%d][%d] = %d\n", chara_no, i, CharaName[i]);
            }

            GamePad.AutoRepeatOff();
            GamePad.MenuModeOff();
            NameSelect.area = NameSelectArea::Keyboard;
            NameSelect.state = NameSelectState::FadeOut;
            NameSelect.state_count = 0;
            break;
        case NameEnterAction::ToggleVoicing:
            break;
    }

    if (input_mode != NameSelect.input_mode && NameSelect.area != NameSelectArea::Tabs) {
        int old_cursor = 0;
        int row = 0;
        int column = 0;

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

                    row = std::min(row, 4);

                    NameSelect.cursor = column + row * 13;
                }

                break;
            }
            default:
                break;
        }
    }

    switch (NameSelect.input_mode) {
        case NAME_INPUT_SYMBOL:
            while (NameSelect.cursor >= 40) {
                NameSelect.cursor -= 10;
            }

            break;
        case NAME_INPUT_ALPHABET: {
            int rows = 4;

            if (NameSelect.language > LANG_ENGLISH_UK) {
                rows += euro_code_linelimmit[NameSelect.language - 2];
            }

            while (NameSelect.cursor >= rows * 13) {
                NameSelect.cursor -= 13;
            }

            break;
        }
        default:
            break;
    }

    int sound = -1;

    if (cursor != NameSelect.cursor || side_row != NameSelect.side_row) {
        sound = MENU_SOUND_CURSOR;
    }

    if (name_pos != NameSelect.name_pos || input_mode != NameSelect.input_mode) {
        sound = MENU_SOUND_CONFIRM;
    }

    if (GamePad.Down(PAD_CROSS)) {
        sound = MENU_SOUND_CONFIRM;
    }

    if (GamePad.Down(PAD_CIRCLE) || action == NameEnterAction::Delete || action == NameEnterAction::Backspace) {
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
    int  length = 0;

    for (; default_names[language][chara_no][length] != 0 && length < 10; length++) {
        name[length] = default_names[language][chara_no][length];
    }

    NameSelect.name_pos = length;

    for (; length < 32; length++) {
        name[length] = 0;
    }
}

namespace {

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
 */
int GetFontLRTumeW(int position, int char_code) {
    int kerning = 0;

    if (char_code >= 0xA2 && char_code < 0x100) {
        kerning += AlphabetEtcOffset[char_code - 0xA2][0];
    }

    if (position - 1 >= 0 && char_code >= 0xA2 && char_code < 0x100) {
        kerning += AlphabetEtcOffset[char_code - 0xA2][1];
    }

    return kerning;
}

} // namespace

void CharaSelectNameDraw2(int x, int y, short *name, CTexture **textures, int blend_mode) {
    if (name == nullptr) {
        return;
    }

    int last = 9;

    while (name[last] == 0 && last > 0) {
        last--;
    }

    int condensed = 0;
    int width = (last + 1) * 0x16;

    if (width >= 0xB0) {
        condensed = 1;
    }

    int kerning = 0;

    for (int slot = last; slot >= 0; slot--) {
        kerning += GetFontLRTumeW(slot, name[slot]);
    }

    if (condensed) {
        width -= kerning;
    }

    int draw_x = x + 0x44 + (width >> 1) - ((width / (last + 1)) >> 1);

    for (; last >= 0; last--) {
        int       cell_x = 0;
        int       cell_y = 0;
        CTexture *texture = GetNameTextureInfo(textures, name[last], cell_x, cell_y);
        CRect_i_  cell_rect(cell_x, cell_y, 0x16, 0x17);

        if (texture != nullptr) {
            DrawMenu2DSprite(texture, CRect_i_(draw_x + 2, y + 2, 0x16, 0x15), cell_rect, 10, 10, 10, blend_mode);
            DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 0x16, 0x15), cell_rect, blend_mode);
        }

        int kerning_step = 0;

        if (last >= 0 && condensed == 1) {
            kerning_step = GetFontLRTumeW(last, name[last]);
        }

        draw_x -= 0x14 - kerning_step;
    }
}

void DrawSaveBoardCharaName2(int x, int y, s16 *name, CTexture **textures, spRGBA top_color, spRGBA bottom_color) {
    spRGBA shadow_top = {.r = 10, .g = 10, .b = 10, .a = top_color.a};
    spRGBA shadow_bottom = {.r = 10, .g = 10, .b = 10, .a = bottom_color.a};
    int    last = 9;

    while (name[last] == 0 && last > 0) {
        last--;
    }

    int condensed = 0;
    int width = (last + 1) * 0x16;

    if (width >= 0xB0) {
        condensed = 1;
    }

    int kerning = 0;

    for (int slot = last; slot >= 0; slot--) {
        kerning += GetFontLRTumeW(slot, name[slot]);
    }

    if (condensed == 1) {
        width -= kerning;
    }

    int draw_x = x + 0x36 + (width >> 1) - ((width / (last + 1)) >> 1);

    for (; last >= 0; last--) {
        int       cell_x = 0;
        int       cell_y = 0;
        CTexture *texture = GetNameTextureInfo(textures, name[last], cell_x, cell_y);
        CRect_i_  cell_rect(cell_x, cell_y, 0x16, 0x17);

        DrawMenu2DSprite(texture, CRect_i_(draw_x + 2, y + 2, 0x16, 0x15), cell_rect, &shadow_top, &shadow_top, &shadow_bottom, &shadow_bottom);
        DrawMenu2DSprite(texture, CRect_i_(draw_x, y, 0x16, 0x15), cell_rect, &top_color, &top_color, &bottom_color, &bottom_color);

        int kerning_step = 0;

        if (last >= 0 && condensed == 1) {
            kerning_step = GetFontLRTumeW(last, name[last]);
        }

        draw_x -= 0x14 - kerning_step;
    }
}

int GetMsgLengthCharaName(int chara_no) {
    if (chara_no < 0 || chara_no > 5) {
        return 0;
    }

    const s16 *name = SaveData->GetCharaName(chara_no);
    int        length = 0;

    while (name[length] != 0 && length < 10) {
        length++;
    }

    return length;
}

namespace {

/** State of the storybook that plays before the game begins. */
OPENING_BOOK OpenBook;

/** The work area the storybook hands to the name-entry screen once its pages are read. */
u_long128 *OpeningReadBuf;

} // namespace

void InitOpeningBook(u_long128 *buffer, int *tex_blocks) {
    u_long128 *load_buffer = buffer;

    if (load_buffer == nullptr) {
        load_buffer = reinterpret_cast<u_long128 *>(read_buffer);
    }

    load_buffer = MenuCalcBufAlignment(load_buffer);
    StartReadBG();
    LoadFileBGMenuData("openbook.pak", load_buffer);
    OpenBook.tex_block = tex_blocks[0];
    OpenBook.name_tex_block = tex_blocks[1];
    OpenBook.open = false;
    OpenBook.step = OpeningBookStep::Load;
    OpenBook.page = 0;
    OpenBook.fade = 128;
    OpenBook.text_alpha = 0;
}

int OpeningBookKey() {
    ReadBG();
    int result = 0;

    switch (OpenBook.step) {
        case OpeningBookStep::Load:
            if (OpenBook.open == 0 && ReadBGSync() == 0) {
                LOADTEXTURE_INFO2 info[3] = {
                    {.name = "#frame_image#640#" SCREEN_HEIGHT_STR "#4", .block_no = OpenBook.tex_block, .mipmap = 0},
                    {.name = nullptr,                                    .block_no = OpenBook.tex_block, .mipmap = 0},
                    {.name = nullptr,                                    .block_no = 0,                  .mipmap = 0},
                };

                BG_READ_INFO *read_file = GetReadBGFile(0);
                info[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(read_file->buffer), "opentex.img", nullptr));
                TexManager.DeleteTextureBlock(OpenBook.tex_block);
                TexManager.CleanUpTextureList();
                TexManager.LoadTextureBlockEX(-1, info);
                OpeningReadBuf = read_file->buffer + (read_file->size >> 4) + 1;
                OpeningReadBuf = MenuCalcBufAlignment(OpeningReadBuf);
                auto *messages = reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(read_file->buffer), "opmes.bin", nullptr));

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

                for (auto &mes_no : CommonMenuMes2.mes_no) {
                    mes_no = -1;
                }

                for (auto &value : CommonMenuMes2.values) {
                    value = 0;
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

                for (auto &line_pos : CommonMenuMes2.line_pos) {
                    line_pos.x = -1;
                    line_pos.y = -1;
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
                OpenBook.fade = std::max<s16>(OpenBook.fade, 0);
            }

            if (OpenBook.fade <= 0) {
                OpenBook.step = OpeningBookStep::TextIn;
                OpenBook.text_alpha = 0;
            }

            break;
        case OpeningBookStep::TextIn:
            OpenBook.text_alpha += 2;

            if (OpenBook.text_alpha >= 0x80) {
                OpenBook.text_alpha = 0x80;
                OpenBook.step = OpeningBookStep::Wait;
                OpenBook.fade = 0;
            }

            break;
        case OpeningBookStep::TextOut:
            OpenBook.text_alpha -= 2;

            if (OpenBook.text_alpha <= 0) {
                OpenBook.text_alpha = 0;
                OpenBook.page++;
                CommonMenuMes2.MakeMesWin(OpenBook.page + 100);
                OpenBook.step = OpeningBookStep::TextIn;
            }

            break;
        case OpeningBookStep::Wait:
            if (GamePad.Down(PAD_CROSS)) {
                OpenBook.step = OpeningBookStep::TextOut;

                if (OpenBook.page >= 11) {
                    OpenBook.step = OpeningBookStep::Finish;
                }
            }

            if (DebugMode && GamePad.Down2(PAD_CROSS)) {
                OpenBook.step = OpeningBookStep::Finish;
            }

            break;
        case OpeningBookStep::Finish:
            OpenBook.text_alpha--;

            if (OpenBook.text_alpha <= 0) {
                CommonMenuMes2.page_arrow = false;
                InitNameRegist(0, OpenBook.name_tex_block, OpeningReadBuf);
                OpenBook.step = OpeningBookStep::NameEntry;
            }

            break;
        case OpeningBookStep::NameEntry:
            result = NameEnterKey();
            break;
        case OpeningBookStep::Unk6:
            break;
    }

    return result;
}

void OpeningBookDraw() {
    setbilinear(0);
    AllFadeForMenu(0x80);

    if (OpenBook.open == 0) {
        return;
    }

    MenuTextureReload(OpenBook.tex_block);
    DrawFullSizePicture(TexManager.GetTexture("openbook", -1), 0, 0, 0x80);

    switch (OpenBook.step) {
        case OpeningBookStep::Load:
        case OpeningBookStep::TextIn:
        case OpeningBookStep::Wait:
        case OpeningBookStep::TextOut:
        case OpeningBookStep::Finish:
            MenuTextureReload(CommonMenuMes2.tex_block);
            CommonMenuMes2.edge_alpha = OpenBook.text_alpha;
            GetMenuCommonPutXY(&CommonMenuMes2, 0x14C - CommonMenuMes2.char_width);
            CommonMenuMes2.Step();
            CommonMenuMes2.DrawMesWin();
            AllFadeForMenu(OpenBook.fade);
            break;
        case OpeningBookStep::NameEntry:
        case OpeningBookStep::Unk6:
            NameEnterDraw();
            break;
    }
}

} // namespace dc::game
