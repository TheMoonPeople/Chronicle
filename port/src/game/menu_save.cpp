#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <system_error>
#include <utility>

#include "battle_globals.hpp"
#include "camera.hpp"
#include "clsmes.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "editatra.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "mainitemmodel.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "platform/input.hpp"
#include "quest/host.hpp"
#include "rect.hpp"
#include "save/save_slots.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "text/gametext.hpp"
#include "texture.hpp"
#include "ui/menu_pointer.hpp"
#include "userstatus.hpp"

namespace dc::game {

using namespace dc::localize;
using namespace dc::quest;
using namespace dc::save;
using namespace dc::text;
using namespace dc::ui;

SAVE_MENU_STATE SaveMenu;
CTexture       *SaveMenuMojiTextbl[4];

/** State of the event item selection menu. */
MINI_MENU_INFO MiniMenu;

namespace {

/**
 * Draws a section of the event item board, clipped to a range.
 */
void DrawEventItemBoard(int x, int y, int top, int bottom, int alpha, CTexture *texture);

/**
 * Handles pad input for the event item selection menu.
 */
int EventItemSelectKey(int *result);

/**
 * Draws the event item selection menu.
 */
void EventItemSelectDraw();

} // namespace

/**
 * Checks the memory card before a save and picks the save menu's next step.
 */
int SaveMenuKeySaveCheck() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (McCheckMCPs2(card) == 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
        return 1;
    }

    if ((card->result < 0) || (card->format_change != 0)) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
        return 1;
    }

    if (card->formatted == 0) {
        SaveMenu.key_no = SAVE_KEY_FORMAT;
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
        return 1;
    }

    if (card->dir_exists == 0) {
        SaveMenu.key_no = SAVE_KEY_NEW_DIR_SELECT;
        return 1;
    }

    if (McAccess.CheckFileNo(SaveMenu.file_no) != 0) {
        SaveMenu.key_no = SAVE_KEY_SAVE_DECIDE;
    } else {
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
        SaveMenu.key_no = SAVE_KEY_SAVE;
    }

    return 1;
}

/**
 * Handles the save menu's prompt to confirm a save.
 */
int SaveMenuKeySaveDecide() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
        int file_no = SaveMenu.file_no;
        McAccess.file_no = file_no;
        static_cast<s32 *>(SaveData->GetConfigData())[17] = file_no;
        SaveMenu.key_no = SAVE_KEY_SAVE;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        SaveMenu.step_time = 0;
        return 1;
    }

    return 1;
}

/**
 * Steps the save menu while the game is written to the memory card.
 */
int SaveMenuKeySave() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (McCheckMCPs2(card) == 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
        return 1;
    }

    if (card->result < 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
        return 1;
    }

    if ((card->free_size < 0x50) && (McAccess.CheckFileNo(SaveMenu.file_no) == 0)) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_SPACE_FILE;
        return 1;
    }

    McAccess.SetFuncNo(MC_OPERATION_SAVE);
    McAccess.file_no = SaveMenu.file_no;
    SaveMenu.key_no = SAVE_KEY_END_SAVE;
    return 1;
}

/**
 * Waits for a button press once a save has finished.
 */
int SaveMenuKeyEndSave() {
    if ((GamePad.Down(PAD_CROSS) != 0) || (GamePad.Down(PAD_CIRCLE) != 0)) {
        McAccess.SetFuncNo(MC_OPERATION_IDLE);
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
    }

    return 1;
}

namespace {

/**
 * Confirms loading the selected save file and advances the menu state.
 */
[[maybe_unused]] int SaveMenuKeyLoadDecide() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        int file_no = SaveMenu.file_no;

        SAVEDATA_INFO *file_info = &McAccess.file_info[file_no];

        if (file_info->state != 0) {
            McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
            McAccess.file_no = SaveMenu.file_no;
            SaveMenu.key_no = SAVE_KEY_LOAD;
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        } else {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }

        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    return 1;
}

} // namespace

/**
 * Steps the save menu while a save is read from the memory card.
 */
int SaveMenuKeyLoad() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (McCheckMCPs2(card) == 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
        return 1;
    }

    if (card->result < 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
        return 1;
    }

    McAccess.SetFuncNo(MC_OPERATION_LOAD);
    McAccess.file_no = SaveMenu.file_no;
    SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
    return 1;
}

/**
 * Waits for a button press while the save menu shows an alert.
 */
int SaveMenuKeyArart() {
    if (SaveMenu.alert_no != SAVE_ALERT_NONE && GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        ComMenuSePlay(MENU_SOUND_REFUSE);
    }

    return 1;
}

/**
 * Handles the save menu's prompt to create new save data.
 */
int SaveMenuKeyNewDirSelect() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
        SaveMenu.key_no = SAVE_KEY_NEW_DIR;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        SaveMenu.step_time = 0;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    return 1;
}

/**
 * Steps the save menu while new save data is created on the memory card.
 */
int SaveMenuKeyNewDir() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (McCheckMCPs2(card) == 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
        return 1;
    }

    if ((card->result < 0) || (card->formatted == 0)) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
        return 1;
    }

    if (card->free_size < 0x190) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_SPACE_DIR;
        return 1;
    }

    McAccess.SetFuncNo(MC_OPERATION_MAKE_DIR);
    SaveMenu.key_no = SAVE_KEY_SAVE;
    return 1;
}

/**
 * Handles the save menu's memory card format step.
 */
int SaveMenuKeyFormat() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (McCheckMCPs2(card) == 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
        return 1;
    }

    if (card->result < 0) {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
        return 1;
    }

    if (GamePad.Down(PAD_CROSS) != 0) {
        McAccess.SetFuncNo(MC_OPERATION_FORMAT);
        SaveMenu.key_no = SAVE_KEY_NEW_DIR;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    return 1;
}

/**
 * Handles the menu response after checking an unformatted memory card.
 */
int SaveMenuKeyUnFormat() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (card->formatted != 0) {
        McAccess.SetFuncNo(MC_OPERATION_UNFORMAT);
    } else {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
    }

    return 1;
}

/**
 * Waits for a button press when the save data is of a different version.
 */
int SaveMenuKeyDifVersion() {
    if (GamePad.Down(PAD_TRIANGLE | PAD_CIRCLE | PAD_CROSS | PAD_SQUARE) != 0) {
        McAccess.SetFuncNo(MC_OPERATION_DELETE);
    }

    return 1;
}

/**
 * Steps the delete choice of the save menu.
 */
s32 SaveMenuKeyDelete() {
    return 1;
}

/**
 * Steps the copy choice of the save menu.
 */
s32 SaveMenuKeyCopy() {
    return 1;
}

/**
 * Handles key input on the save menu offered after the ending.
 */
int SaveMenuKeyAfterEnding() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
    } else if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_FADE_OUT;
        ExitSaveSelect();
    }

    return 1;
}

/**
 * Handles the prompt to confirm a save after the ending.
 */
int SaveMenuKeySaveDecideEnding() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        SaveMenu.key_no = SAVE_KEY_SAVE_ENDING;
    } else if (GamePad.Down(PAD_CIRCLE) != 0) {
        SaveMenu.key_no = SAVE_KEY_FADE_OUT;
        ExitSaveSelect();
    }

    return 1;
}

namespace {

/**
 * Steps the save menu while saving after the ending.
 */
[[maybe_unused]] int SaveMenuKeySaveEnding() {
    SaveMenu.key_no = SAVE_KEY_END_SAVE_ENDING;
    McAccess.SetFuncNo(MC_OPERATION_SAVE_CONFIG);
    return 1;
}

/**
 * Waits for a button press once the save after the ending has finished.
 */
[[maybe_unused]] int SaveMenuKeyEndSaveEnding() {
    if (GamePad.Down(PAD_CROSS) != 0) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        McAccess.SetFuncNo(MC_OPERATION_IDLE);
    }

    return 1;
}

/**
 * Message numbers of allmenu.mes that the save screens show.
 */
enum class SaveMenuMessage : int {
    NONE = 0,
    WANT_TO_SAVE = 260,  /**< "Want to save?" */
    SAVED = 262,         /**< "Saving completed." */
    REPLACE = 264,       /**< "Data exist on file N. Replace it?" */
    SAVE_FAILED = 266,   /**< "Saving failed." */
    WANT_TO_LOAD = 270,  /**< "Want to load?" */
    LOAD_FAILED = 274,   /**< "Loading failed." */
    FORMAT_FAILED = 282, /**< "Formatting failed." */
    DELETE = 290,        /**< "Delete file?" */
    COPY = 292,          /**< "Want to copy file?" */
    OTHER_VERSION = 299, /**< "Data not loaded due to change in data format. Deleting No. N data." */
};

SaveMenuMessage SaveMenuMessageFor() {
    if (McAccess.GetFuncNo() != MC_OPERATION_IDLE) {
        return SaveMenuMessage::NONE;
    }

    switch (SaveMenu.key_no) {
        case SAVE_KEY_SAVE_DECIDE:
            return SaveMenuMessage::REPLACE;
        case SAVE_KEY_LOAD_DECIDE:
            return SaveMenuMessage::WANT_TO_LOAD;
        case SAVE_KEY_ALERT:
            switch (SaveMenu.alert_no) {
                case SAVE_ALERT_NONE:
                case SAVE_ALERT_UNK_3:
                case SAVE_ALERT_UNK_4:
                case SAVE_ALERT_UNK_5:
                    return SaveMenuMessage::NONE;
                case SAVE_ALERT_FORMAT_FAILED:
                    return SaveMenuMessage::FORMAT_FAILED;
                case SAVE_ALERT_SAVE_FAILED:
                    return SaveMenuMessage::SAVE_FAILED;
                case SAVE_ALERT_LOAD_FAILED:
                    return SaveMenuMessage::LOAD_FAILED;
                default:
                    return SaveMenu.access_kind == SAVE_ACCESS_LOAD ? SaveMenuMessage::LOAD_FAILED : SaveMenuMessage::SAVE_FAILED;
            }
        case SAVE_KEY_DELETE:
            return SaveMenuMessage::DELETE;
        case SAVE_KEY_COPY:
            return SaveMenuMessage::COPY;
        case SAVE_KEY_DIF_VERSION:
            return SaveMenuMessage::OTHER_VERSION;
        case SAVE_KEY_AFTER_ENDING:
            return SaveMenuMessage::WANT_TO_SAVE;
        case SAVE_KEY_END_SAVE:
        case SAVE_KEY_END_SAVE_ENDING:
            return SaveMenuMessage::SAVED;
        default:
            return SaveMenuMessage::NONE;
    }
}

} // namespace

// Message number for the save screens, with no message that names the memory card. Card
// operations finish within the frame after the one that starts them and show nothing; the save
// after the ending asks "Want to save?"; alerts only a card raised report a failed save or load.
int GetSaveMenuMsgNo() {
    return static_cast<int>(SaveMenuMessageFor());
}

int SaveMenuTextureEnter() {
    ReadBG();

    if (ReadBGSync() != 0) {
        return 0;
    }

    LOADTEXTURE_INFO2 tex[3] = {
        {"#frame_image_save#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {nullptr,                                         0, 0},
        {nullptr,                                         0, 0}
    };

    tex[0].block_no = SaveMenu.block_no;
    tex[1].block_no = SaveMenu.block_no;
    BG_READ_INFO *bg = GetReadBGFile(0);
    u_int        *pack = GetPackFile(reinterpret_cast<u_int *>(bg->buffer), "saveimg.img", nullptr);
    tex[1].name = reinterpret_cast<char *>(pack);
    TexManager.DeleteTextureBlock(SaveMenu.block_no);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, tex);
    SaveBoard = TexManager.GetTexture("save", -1);

    char *moji_names[4] = {"alphabet", "kata", "hira", "alphabet"};

    for (int i = 0; i < 4; i++) {
        SaveMenuMojiTextbl[i] = TexManager.GetTexture(moji_names[i], -1);
    }

    if (SaveMenu.mode == SAVE_MENU_MODE_ENDING || SaveMenu.mode == SAVE_MENU_MODE_LOAD) {
        InitMenuMesSet(MENU_MES_SET_ALLMENU, reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(bg->buffer), "allmenu.mes", nullptr)));
    }

    CommonMenuMes2.stay_frame = true;
    CommonMenuMes2.value_show = true;
    CommonMenuMes2.value_signed = false;
    CommonMenuMes2.cursor_lit = true;

    char        *save_buffer = reinterpret_cast<char *>(pack) + bg->size;
    MC_ICON_DATA icon = {
        {"dkicon.ico",   nullptr, 0},
        {"dkicon_c.ico", nullptr, 0},
        {"dkicon_d.ico", nullptr, 0}
    };

    for (MC_ICON_FILE *file : std::array{&icon.view, &icon.copy, &icon.del}) {
        file->data = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(bg->buffer), file->name, &file->size));
    }

    McAccess.SetBuff(save_buffer);
    McAccess.SetIconData(&icon);
    McAccess.MakeMcIconSysInfo();
    return 1;
}

int SaveMenuEffectFadeOut() {
    if (SaveMenu.key_no == SAVE_KEY_FADE_OUT) {
        return 1;
    }

    return 0;
}

namespace {

/**
 * Fades the save board toward the top and bottom of the screen: the alpha at
 * screen row y, ramping over the 0x80 rows nearest each edge.
 */
int SaveBoardEdgeAlpha(int y, int alpha) {
    if ((0 <= y) && (y < 0x81)) {
        return (y * alpha) >> 7;
    }

    if ((y > 0x80) && (y < SCREEN_HEIGHT - 0x7F)) {
        return (alpha * 0x80) >> 7;
    }

    if ((y > SCREEN_HEIGHT - 0x80) && (y < SCREEN_HEIGHT + 1)) {
        return ((SCREEN_HEIGHT - y) * alpha) >> 7;
    }

    return 0;
}

/**
 * Computes the save board's alpha ramp at a position and at that position
 * offset by the second argument.
 */
void GetSaveBoardAlphaInfo(int x, int width, int &start_alpha, int &end_alpha, int alpha) {
    start_alpha = SaveBoardEdgeAlpha(x, alpha);
    end_alpha = SaveBoardEdgeAlpha(x + width, alpha);
}

constexpr RECT number_rect = {.x = 0x88, .y = 0xCA, .width = 0xC, .height = 0x10};

// Draws number right to left, its last digit ending at right_x.
void DrawSaveBoardNumber(int number, int right_x, int draw_y, spRGBA &top, spRGBA &bottom) {
    for (int digits = GetNumberKeta(number); digits > 0; digits--) {
        int       clip_y = draw_y;
        const int digit = number % 10;
        const int width = number_rect.width;
        right_x -= width - 1;
        const int src_x = number_rect.x + width * digit;
        int       src_y = number_rect.y;
        int       height = number_rect.height;
        MenuTextureClip(clip_y, src_y, height, 0, SCREEN_HEIGHT);
        DrawMenu2DSprite(SaveBoard, CRect_i_(right_x, clip_y, width, height - 1), CRect_i_(src_x, src_y, width, height), &top, &top, &bottom, &bottom);
        number /= 10;
    }
}

} // namespace

// The autosave's board is labelled in place of a number.
void DrawSaveBoard(SAVEDATA_INFO *info, CTexture **name_texture, int x, int y, int /*unused*/, int alpha) {
    if (info == nullptr) {
        return;
    }

    spRGBA top = {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0};
    spRGBA bottom = {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0};
    int    start_alpha = 0;
    int    end_alpha = 0;
    GetSaveBoardAlphaInfo(y, 0x88, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    DrawMenu2DSprite(SaveBoard, CRect_i_(x, y, 0x180, 0x87), CRect_i_(0, 0, 0x180, 0x88), &top, &top, &bottom, &bottom);

    int draw_x = x + 0x54;
#ifdef PAL
    constexpr s8 number_shift[] = {0, 0, 0, 10, 0, 4, 13};
    draw_x += number_shift[GetMenuLangFlag()];
#endif
    int draw_y = y + 8;
    GetSaveBoardAlphaInfo(y, 0x10, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;

    if (info->file_no <= 0) {
        static GameText label;
        label.Set(LocalizeText("save.autosave", "Auto"));
        label.Draw(draw_x - label.Width(), draw_y - 2, start_alpha);
    } else {
        DrawSaveBoardNumber(info->file_no, draw_x, draw_y, top, bottom);
    }

    draw_x = x + 0x1A;
    draw_y = y + 0x1C;
    GetSaveBoardAlphaInfo(y, 0x16, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    DrawSaveBoardCharaName2(draw_x + 8, draw_y, reinterpret_cast<short *>(info->name), name_texture, top, bottom);

    // Play time, as hours, minutes and seconds.
    constexpr RECT time_rect = {.x = 0x88, .y = 0xB8, .width = 0xC, .height = 0x12};
    const int      column = x + 0x92;
    draw_x = column + 0xA;
    draw_y = y + 0x36;
    GetSaveBoardAlphaInfo(draw_y, 0x12, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    int frames = info->play_time;

    if (frames >= 0x1499700) {
        frames = 0x14996C4;
    }

    frames /= 60;
    const int hours = frames / 3600;
    const int minutes = (frames / 60 - hours * 60) % 60;
    const int seconds = frames % 60;

    const std::array<int, 3> parts = {hours, minutes, seconds};
    for (int part = 2; part >= 0; part--) {
        const int value = parts[part];
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, time_rect.width, time_rect.height), CRect_i_(time_rect.x + time_rect.width * (value / 10), time_rect.y, time_rect.width, time_rect.height), &top, &top, &bottom, &bottom);
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x + time_rect.width, draw_y, time_rect.width, time_rect.height), CRect_i_(time_rect.x + time_rect.width * (value % 10), time_rect.y, time_rect.width, time_rect.height), &top, &top, &bottom, &bottom);
        draw_x -= 0x1E;
    }

    const CRect_i_ colon(0x100, 0xB8, 0xC, 0x12);
    GetSaveBoardAlphaInfo(draw_y, 0x12, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    draw_x = column + 2;
    DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, 0xC, 0x12), colon, &top, &top, &bottom, &bottom);
    DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x - 0x1E, draw_y, 0xC, 0x12), colon, &top, &top, &bottom, &bottom);

    draw_x = x + 0x88;
    draw_y = y + 0x52;
    GetSaveBoardAlphaInfo(draw_y, 0x12, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    DrawSaveBoardNumber(info->quest_total, draw_x, draw_y, top, bottom);

    // Row of each map's name on the board texture; a map from 14 on is in the second block.
    const int                            map = info->map_no;
    static constexpr std::array<s16, 62> map_name = {
        0,
        1,
        2,
        3,
        6,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        1,
        9,
        9,
        9,
        9,
        9,
        2,
        2,
        4,
        10,
        4,
        13,
        13,
        13,
        10,
        9,
        4,
        4,
        4,
        4,
        1,
        10,
        12,
        10,
        5,
        13,
        12,
        13,
        4,
        10,
        2,
        2,
        2,
        13,
        4,
        13,
        13,
        13,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        14,
        14,
    };
    if (map >= 0 && std::cmp_less(map, map_name.size())) {
        const int  name_index = map_name[map];
        const bool second_block = name_index >= 0xE;
        const int  block_index = second_block ? name_index - 0xE : name_index;
        const int  name_u = (block_index / 7) * 0x88;
        const int  name_v = (block_index % 7) * 0x14 + (second_block ? 0x144 : 0xB8);

        draw_x = x + 0x23;
        draw_y = y + 0x63;
        GetSaveBoardAlphaInfo(draw_y, 0x14, start_alpha, end_alpha, alpha);
        top.a = start_alpha;
        bottom.a = end_alpha;
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, 0x88, 0x15), CRect_i_(name_u, name_v, 0x88, 0x14), &top, &top, &bottom, &bottom);
    }

    // A face for each member of the party, three to a row.
    const int face_x = x + 0xCF;
    draw_x = face_x;
    draw_y = y + 0x1A;
    GetSaveBoardAlphaInfo(draw_y, 0x30, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;

    for (int i = 0; i < info->party_size; i++) {
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, 0x30, 0x31), CRect_i_(i * 0x30, 0x88, 0x30, 0x30), &top, &top, &bottom, &bottom);
        draw_x += 0x38;

        if (i == 2) {
            draw_x = face_x;
            draw_y += 0x34;
            GetSaveBoardAlphaInfo(draw_y, 0x30, start_alpha, end_alpha, alpha);
            top.a = start_alpha;
            bottom.a = end_alpha;
        }
    }
}

void DrawNewFileTemplete(int x, int y, int alpha) {
    int start_alpha = 0;
    int end_alpha = 0;

    if (y % 2 != 0) {
        y++;
    }

    spRGBA top = {0x80, 0x80, 0x80, 0};
    top.a = alpha;
    spRGBA bottom = {0x80, 0x80, 0x80, 0};
    bottom.a = alpha;

    // Top edge: two corners and the run between them.
    GetSaveBoardAlphaInfo(y, 0x10, start_alpha, end_alpha, 0x80);
    top.a = start_alpha;
    bottom.a = end_alpha;
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x, y, 0x10, 0xF), CRect_i_(0x120, 0x88, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x + 0x170, y, 0x10, 0xF), CRect_i_(0x140, 0x88, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x + 0x10, y, 0x160, 0xF), CRect_i_(0x130, 0x88, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);

    // Bottom edge.
    GetSaveBoardAlphaInfo(y + 0x78, 0x10, start_alpha, end_alpha, 0x80);
    top.a = start_alpha;
    bottom.a = end_alpha;
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x, y + 0x78, 0x10, 0xF), CRect_i_(0x120, 0xA8, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x + 0x170, y + 0x78, 0x10, 0xF), CRect_i_(0x140, 0xA8, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x + 0x10, y + 0x78, 0x160, 0xF), CRect_i_(0x130, 0xA8, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);

    // Sides.
    GetSaveBoardAlphaInfo(y + 0x10, 0x68, start_alpha, end_alpha, 0x80);
    top.a = start_alpha;
    bottom.a = end_alpha;
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x, y + 0xF, 0x10, 0x69), CRect_i_(0x120, 0x98, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, SaveBoard, CRect_i_(x + 0x170, y + 0xF, 0x10, 0x69), CRect_i_(0x140, 0x98, 0x10, 0x10), &top, &top, &bottom, &bottom, 1);

    // "New file".
    GetSaveBoardAlphaInfo(y + 0x35, 0x1E, start_alpha, end_alpha, 0x80);
    top.a = start_alpha;
    bottom.a = end_alpha;
    s8  label_inset[] = {64, 64, 64, 64, 38, 64, 64};
    int label_x = 0xC0 - label_inset[GetMenuLangFlag()];
    set2DSprite(GetVif1Packet(), SaveBoard, CRect_i_(x + label_x, y + 0x35, 0x74, 0x1E), CRect_i_(0x10C, 0xB8, 0x74, 0x1E), &top, &top, &bottom, &bottom, 1);
}

// Data exists once state.json or a save folder does.
int InitExistData() {
    if (McAccess.InitForMC() != 0) {
        return 0;
    }

    McAccess.port = 0;
    McAccess.LoadSysConfig();
    std::error_code error;
    return static_cast<int>(std::filesystem::exists(SaveStatePath(), error) || !SaveSlotFiles().empty());
}

int SaveEnableCheck() {
    if (McAccess.InitForMC() != 0) {
        return SAVE_ENABLE_NO_CARD;
    }

    int found = 0;
    int free_size = 0;

    for (int port = 0; port < 2; port++) {
        McAccess.port = port;
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);

        int result = 0;

        do {
            result = McAccess.Step();
        } while (result == 0);

        if (result < 0) {
            continue;
        }

        MC_CARD_INFO *card = &McAccess.card[McAccess.port];

        if (McCheckMCPs2(card) == 0) {
            continue;
        }

        found = 1;

        if (card->formatted == 0) {
            return found;
        }

        McAccess.SetFuncNo(MC_OPERATION_GET_DIR);

        do {
            result = McAccess.Step();
        } while (result == 0);

        if (result < 0) {
            continue;
        }

        if (card->dir_exists != 0) {
            return SAVE_ENABLE_OK;
        }

        free_size = std::max(free_size, card->free_size);
    }

    if (found == 0) {
        return SAVE_ENABLE_NO_CARD;
    }

    if (free_size < 0x190) {
        return SAVE_ENABLE_NO_SPACE;
    }

    return SAVE_ENABLE_OK;
}

/**
 * Screen position of the event item selection board's top left corner.
 */
float EventBoardPos[2];

/**
 * Screen y of the event item board's first row, eased toward the scroll row.
 */
int EventItemMoveY;

s32 MiniEventTextureBlock;

/**
 * Nonzero once the event item selection menu's textures have loaded.
 */
int MiniEventTexReadFlag;

/**
 * Screen position of the event item cursor's highlight, eased toward the cursor.
 */
float MiniCur[2];

namespace {

/**
 * Screen y of the event item board's scroll bar, eased toward its target.
 */
float EventBarY;

/**
 * Board texture of the event item selection menu.
 */
CTexture *MiniEventBoard;

/**
 * Board texture of the fish food selection menu.
 */
CTexture *FishFoodBoard;

/**
 * The item pack the event item selection menu lists.
 */
ITEM_PACK *EventItemPackPt;

} // namespace

void InitEventItemSelect(int block, int *usable, ITEM_PACK *pack, int x, int y, int vanish, int fish_mode) {
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    MiniMenu.fish_mode = fish_mode;
    MiniEventTextureBlock = block;
    MiniMenu.lang = GetMenuLangFlag();
    s8 lang_y[7] = {0, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10};
    EventBoardPos[0] = x;
    EventBoardPos[1] = static_cast<float>(y) + static_cast<float>(lang_y[MiniMenu.lang]) + 20.0f;
    EventItemPackPt = pack;
    MiniMenu.event_item_num = 0;

    for (int i = 0; i < pack->num; i++) {
        if (pack->item[i] >= ITEM_DUNGEON_START) {
            MiniMenu.event_item_num++;
        }
    }

    for (MiniMenu.usable_num = 0; MiniMenu.usable_num < 13; MiniMenu.usable_num++) {
        MiniMenu.usable[MiniMenu.usable_num] = usable[MiniMenu.usable_num];
        std::println("itemno = {}", usable[MiniMenu.usable_num]);

        if (usable[MiniMenu.usable_num] < 0) {
            break;
        }
    }

    MiniMenu.selected = -1;
    MiniMenu.vanish = vanish;

    if (MiniMenu.vanish != 0) {
        std::println("vanish after use !");
    } else {
        std::println("exist after use ");
    }

    LOADTEXTURE_INFO2 texture[2] = {
        {"#frame_image#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {nullptr,                                    0, 0}
    };
    texture[0].block_no = MiniEventTextureBlock;
    TexManager.DeleteTextureBlock(MiniEventTextureBlock);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, texture);
    StartReadBG();
    auto *buffer = reinterpret_cast<u_long128 *>(read_buffer);
    buffer = MenuCalcBufAlignment(buffer);
    LoadFileBGMenuData("eventmnu2.pak", buffer);
    ReadBG();
    MiniMenu.cursor = 0;
    MiniMenu.scroll_row = 0;
    EventItemMoveY = 56.0f + EventBoardPos[1] - MiniMenu.scroll_row * 0x28;
    int rows = EventItemPackPt->num / 5;

    if (rows <= 0) {
        rows = 1;
    }

    EventBarY = 60.0f + EventBoardPos[1] + static_cast<int>(68.0f * MiniMenu.scroll_row) / rows;
    MiniCur[0] = 6.0f + EventBoardPos[0] + ((MiniMenu.cursor + 5) % 5) * 0x2A;
    MiniCur[1] = 60.0f + EventBoardPos[1] + ((MiniMenu.cursor - MiniMenu.scroll_row * 5) / 5) * 0x28;
    MiniMenu.state = MINI_MENU_FADE_IN;
    MiniEventTexReadFlag = 0;
}

namespace {

/**
 * Frees the event item selection menu's textures and releases the pad.
 */
void EventItemSelectExit() {
    TexManager.DeleteTextureBlock(MiniEventTextureBlock);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

/**
 * Runs one frame of the original item picker selection and drawing logic.
 */
int RetailEventItemSelectLoop(int *result) {
    ReadBG();
    int alpha = 0x40;

    switch (MiniMenu.state) {
        case MINI_MENU_CHOOSING:
            alpha = 0x40;
            break;
        case MINI_MENU_FADE_IN:
            alpha = std::max(0x80 - MiniMenu.state_time * 4, 0x40);

            break;
        case MINI_MENU_FADE_OUT:
            alpha = std::min(MiniMenu.state_time * 4 + 0x40, 0x80);

            break;
        default:
            break;
    }

    setbilinear(0);
    FrameImageDraw(alpha, 0x80);
    MenuTextureReload(MiniEventTextureBlock);
    const int done = EventItemSelectKey(result);
    EventItemSelectDraw();
    setbilinear(1);

    if (done != 0) {
        EventItemSelectExit();
    }

    return done;
}

} // namespace

namespace {

/** The pointer owned by the door/object item picker. */
MenuPointer event_pointer;
/** Whether the item picker remained active after its last update. */
bool event_active = false;

/**
 * Finds an occupied item cell within the animated board's visible clip.
 */
int ItemHit(float x, float y) {
    const float left = EventBoardPos[0] + 28.0f;
    const float top = EventBoardPos[1] + 56.0f;
    if (x < left || x >= left + 210.0f || y < top || y >= top + 80.0f) {
        return -1;
    }
    for (int index = 0; index < MiniMenu.event_item_num; ++index) {
        const int   column = index % 5;
        const int   row = index / 5;
        const float cell_left = left + column * 42.0f;
        const auto  cell_top = static_cast<float>(EventItemMoveY + row * 40);
        if (x >= cell_left && x < cell_left + 42.0f && y >= cell_top && y < cell_top + 40.0f) {
            return index;
        }
    }
    return -1;
}

} // namespace

/**
 * Tracks the item picker's lifetime while preserving its original item handling.
 */
int EventItemSelectLoop(int *result) {
    const int done = RetailEventItemSelectLoop(result);
    event_active = done == 0;
    return done;
}

bool EventItemPointerUpdate() {
    const bool active = event_active;
    event_active = false;
    if (!active) {
        return !event_pointer.Close();
    }
    if (!event_pointer.IsOpen()) {
        event_pointer.Open();
    }
    const MenuPointerTake take = event_pointer.Take(MiniCur[0], MiniCur[1]);
    MenuPointerShow(&event_pointer);
    if (!MiniEventTexReadFlag) {
        return true;
    }
    if (MiniMenu.state == MINI_MENU_REFUSED) {
        if ((take.clicked & 3) != 0) {
            MenuPointerPress(kInputCircle);
        }
        return true;
    }
    if (MiniMenu.state != MINI_MENU_CHOOSING) {
        return true;
    }
    if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
        return true;
    }
    const int hit = take.pointing ? ItemHit(event_pointer.x, event_pointer.y) : -1;
    if (hit >= 0 && take.moved && take.wheel == 0.0f) {
        MiniMenu.cursor = hit;
        if ((take.clicked & 1) != 0) {
            MenuPointerPress(kInputCross);
        }
    }
    if (take.pointing && take.wheel != 0.0f) {
        MenuPointerPress(take.wheel > 0.0f ? kInputUp : kInputDown);
    }
    return true;
}

namespace {

int EventItemSelectKey(int *result) {
    if (MiniEventTexReadFlag == 0) {
        if (ReadBGSync() != 0) {
            return 0;
        }

        LOADTEXTURE_INFO2 texture[4] = {
            {"#frame_image#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
            {nullptr,                                    0, 0},
            {nullptr,                                    0, 0},
            {nullptr,                                    0, 0}
        };
        texture[0].block_no = MiniEventTextureBlock;
        texture[1].block_no = MiniEventTextureBlock;
        texture[2].block_no = MiniEventTextureBlock;
        BG_READ_INFO *file = GetReadBGFile(0);
        texture[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "eventmnu.img", nullptr));
        texture[2].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "fishmnu.img", nullptr));
        TexManager.DeleteTextureBlock(MiniEventTextureBlock);
        TexManager.LoadTextureBlockEX(-1, texture);
        MiniEventBoard = TexManager.GetTexture("eventmnu", -1);
        FishFoodBoard = TexManager.GetTexture("fishmnu", -1);
        ItemIcon = TexManager.GetTexture("itemicon", -1);
        InitMenuMesSet(MENU_MES_SET_EVENT_ITEM, reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "eventuse.bin", nullptr)));
        CommonMenuMes2.MakeMesWin(0);
        CommonMenuMes2.Step();
        MiniEventTexReadFlag = 1;
    }

    int       done = 0;
    const int old_cursor = MiniMenu.cursor;
    const int slot_num = EventItemPackPt->num;

    switch (MiniMenu.state) {
        case MINI_MENU_REFUSED:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                MiniMenu.state = MINI_MENU_CHOOSING;
            }

            break;
        case MINI_MENU_CHOOSING:
            if (GamePad.Down(PAD_UP) != 0 && MiniMenu.cursor > 4) {
                MiniMenu.cursor -= 5;
            }

            if (GamePad.Down(PAD_DOWN) != 0 && MiniMenu.cursor < slot_num - 5) {
                MiniMenu.cursor += 5;
            }

            if (GamePad.Down(PAD_LEFT) != 0 && 0 < MiniMenu.cursor) {
                MiniMenu.cursor--;
            }

            if (GamePad.Down(PAD_RIGHT) != 0 && MiniMenu.cursor < slot_num - 1) {
                MiniMenu.cursor++;
            }

            if (MiniMenu.cursor < MiniMenu.scroll_row * 5) {
                MiniMenu.scroll_row--;
            }

            if ((MiniMenu.scroll_row + 2) * 5 - 1 < MiniMenu.cursor) {
                MiniMenu.scroll_row++;
            }

            if (old_cursor != MiniMenu.cursor) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_SQUARE) != 0) {
                SeitonItemBoard(EventItemPackPt);
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                break;
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                int  pack_index = -1;
                s16 *slot;

                if (MiniMenu.cursor < MiniMenu.event_item_num) {
                    int event_no = 0;
                    slot = EventItemPackPt->item;

                    for (pack_index = 0; pack_index < EventItemPackPt->num; pack_index++) {
                        if (*slot >= ITEM_DUNGEON_START) {
                            if (MiniMenu.cursor == event_no) {
                                *result = *slot;
                                break;
                            }

                            event_no++;
                        }

                        slot++;

                        if (pack_index == EventItemPackPt->num - 1) {
                            pack_index = EventItemPackPt->num;
                        }
                    }
                }

                int accepted = 0;

                for (int i = 0; slot != nullptr && MiniMenu.usable[i] >= ITEM_DUNGEON_START && i < 13; i++) {
                    if (MiniMenu.usable[i] == *result) {
                        accepted = 1;

                        if (MiniMenu.vanish != 0 && 0 <= pack_index && pack_index < EventItemPackPt->num) {
                            EventItemPackPt->item[pack_index] = 0;
                        }

                        break;
                    }
                }

                if (slot == nullptr) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else if (accepted != 0) {
                    done = 1;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else {
                    std::println("Miss\tselected itemNo = {}", *result);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    done = 1;

                    if (MiniMenu.fish_mode != 0) {
                        done = 0;
                        MiniMenu.state = MINI_MENU_REFUSED;
                    }
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                *result = -1;
                done = 1;
            }

            break;
        default:
            break;
    }

    s16 *item = nullptr;

    if (MiniMenu.cursor < MiniMenu.event_item_num) {
        int event_no = 0;
        item = EventItemPackPt->item;

        for (int j = 0; j < EventItemPackPt->num; j++) {
            if (*item >= ITEM_DUNGEON_START) {
                if (MiniMenu.cursor == event_no) {
                    break;
                }

                event_no++;
            }

            item++;
        }
    }

    const int message = (item != nullptr && 0 < *item) ? *item + 500 : 0;

    if (CommonMenuMes2.mes_made != message) {
        CommonMenuMes2.MakeMesWin(message);
    }

    return done;
}

/**
 * Draws one vertical section of the board the event and fishing menus share.
 */
void DrawEventAndFishMenuBoard_Ver(CTexture *texture, CRect_i_ rect, int src_u, int src_width, int /*lang*/, int alpha) {
    int y = rect.y;

    DrawMenu2DSprite(texture, CRect_i_(rect.x, y, rect.width, 0x94), CRect_i_(src_u, 0, src_width, 0x94), alpha);
    y += 0x94;
    DrawMenu2DSprite(texture, CRect_i_(rect.x, y, rect.width, rect.height + 0x32), CRect_i_(src_u, 0x94, src_width, 0x14), alpha);
    y += rect.height + 0x32;
    DrawMenu2DSprite(texture, CRect_i_(rect.x, y, rect.width, 0x1E), CRect_i_(src_u, 0xC6, src_width, 0x1E), alpha);
}

/**
 * Draws the board the event and fishing menus share.
 */
void DrawEventAndFishMenuBoard(CTexture *texture, int x, int y, int alpha, int lang) {
    s8  kakudai_tate_lang[7] = {0, 16, 16, 16, 16, 16, 16};
    s8  kakudai_yoko_lang[7] = {0, 10, 10, 20, 10, 16, 14};
    int extra_height = kakudai_tate_lang[lang];
    int extra_width = kakudai_yoko_lang[lang];

    CRect_i_ center(x + 0x1C, y, 0xD2, extra_height);
    DrawEventAndFishMenuBoard_Ver(texture, center, 0x1C, 0xD2, lang, alpha);
    int      edge = extra_width + 6;
    CRect_i_ left_inner(x + 0x1C - edge, y, edge, extra_height);
    CRect_i_ left_outer(x + 8 - edge, y, 0x14, extra_height);
    DrawEventAndFishMenuBoard_Ver(texture, left_inner, 0x14, 6, lang, alpha);
    DrawEventAndFishMenuBoard_Ver(texture, left_outer, 0, 0x14, lang, alpha);
    CRect_i_ right_inner(x + 0xEE, y, edge, extra_height);
    CRect_i_ right_outer(x + 0xEE + edge, y, 0x20, extra_height);
    DrawEventAndFishMenuBoard_Ver(texture, right_inner, 0xEC, 6, lang, alpha);
    DrawEventAndFishMenuBoard_Ver(texture, right_outer, 0xF4, 0x20, lang, alpha);

    int rows = EventItemPackPt->num / 5;

    if (rows <= 0) {
        rows = 1;
    }

    const float bar_height = std::min(136.0f / rows, 68.0f);

    int bar_x = x + 0xF6 + edge;
    EventBarY += (static_cast<int>((y + 0x3C) + (68.0f * MiniMenu.scroll_row) / rows) - EventBarY) / 4.0f;
    int bar_y = EventBarY;
    DrawMenu2DSprite(texture, CRect_i_(bar_x, bar_y, 8, static_cast<int>(bar_height)), CRect_i_(0, 0xE4, 8, 0xC), alpha);
}

void EventItemSelectDraw() {
    if (MiniEventTexReadFlag == 0) {
        return;
    }

    int alpha = 0x80;

    switch (MiniMenu.state) {
        case MINI_MENU_FADE_IN:
            alpha = MiniMenu.state_time * 8;

            if (alpha > 0x80) {
                alpha = 0x80;
                MiniMenu.state = MINI_MENU_CHOOSING;
            }

            break;
        case MINI_MENU_FADE_OUT:
            alpha = std::max(0x80 - MiniMenu.state_time * 8, 0);

            break;
        default:
            break;
    }

    const float left = EventBoardPos[0];
    const float top = EventBoardPos[1];
    int         y = top;
    CTexture   *board = (MiniMenu.fish_mode != 0) ? FishFoodBoard : MiniEventBoard;
    const float row_top = 56.0f + top;
    const int   clip_top = row_top;
    const int   clip_bottom = 136.0f + top;
    const float icon_x = 32.0f + left;
    int         x = icon_x;
    const int   board_x = 28.0f + left;
    int         row_y = row_top - MiniMenu.scroll_row * 0x28;
    EventItemMoveY += (row_y - EventItemMoveY) >> 2;

    if (std::abs(EventItemMoveY - row_y) < 4) {
        EventItemMoveY = row_y;
    }

    row_y = EventItemMoveY;

    for (int row = 0; row < EventItemPackPt->num / 5; row++) {
        DrawEventItemBoard(board_x, row_y, clip_top, clip_bottom, alpha, board);
        row_y += 0x28;
    }

    std::array<s16, 100> items{};
    int                  item_count = 0;

    for (int j = 0; j < EventItemPackPt->num; j++) {
        if (EventItemPackPt->item[j] >= ITEM_DUNGEON_START) {
            items[item_count++] = EventItemPackPt->item[j];
        }
    }

    row_y = EventItemMoveY;

    for (int i = 0; i < 100; i++) {
        if (clip_bottom < y) {
            break;
        }

        y = row_y + 4;
        DrawIconParts(items[i], x, y, clip_top, clip_bottom, alpha, 0);
        x += 0x2A;

        if (i % 5 == 4) {
            x = icon_x;
            row_y += 0x28;
        }
    }

    DrawEventAndFishMenuBoard(board, left, top, alpha, MiniMenu.lang);
    y = MiniMenu.cursor;
    x = 6.0f + left + ((y + 5) % 5) * 0x2A;
    y = 60.0f + top + ((y - MiniMenu.scroll_row * 5) / 5) * 0x28;
    DrawMenuWaku(x + 0x10, y - 9, 0x24, 0x24, 0, StayTex, alpha);
    MiniCur[0] += (x - MiniCur[0]) / 4.0f;
    MiniCur[1] += (y - MiniCur[1]) / 4.0f;
    DrawMenuObjectVibe(MiniCur[0], MiniCur[1], 1, 0x40);
    CursorVibeCnt++;

    if (CursorVibeCnt >= 0x405F7E00) {
        CursorVibeCnt = 0;
    }

    CommonMenuMes2.edge_alpha = alpha;
    s8 message_pos[7][2] = {
        {0,  0},
        {-4, 0},
        {-4, 0},
        {-9, 0},
        {-4, 0},
        {-7, 0},
        {-6, 0}
    };
    DrawMenuClsMes(&CommonMenuMes2, 20.0f + left + message_pos[MiniMenu.lang][0], 146.0f + top + message_pos[MiniMenu.lang][1]);

    if (MiniMenu.state == MINI_MENU_REFUSED) {
        if (CommonMenuMes1.mes_made != 1) {
            CommonMenuMes1.MakeMesWin(1);
        }

        AllFadeForMenu(0);
        CommonMenuMes1.stay_frame = true;
        DrawMenuClsMes(&CommonMenuMes1, 0xDC, 0xA0);
    }

    if (MiniMenu.state != MINI_MENU_CHOOSING) {
        MiniMenu.state_time++;
    } else {
        MiniMenu.state_time = 0;
    }

    setbilinear(1);
}

void DrawEventItemBoard(int x, int y, int top, int bottom, int alpha, CTexture *texture) {
    if ((y < top - 0x27) || (bottom <= y)) {
        return;
    }

    int pos_x = x;
    int clip_y = y;
    int clip_v = 0;
    int clip_height = 0x28;
    MenuTextureClip(clip_y, clip_v, clip_height, top, bottom);

    for (int i = 0; i < 5; i++) {
        DrawMenu2DSprite(texture, CRect_i_(pos_x, clip_y + 1, 0x28, clip_height), CRect_i_(0x116, clip_v, 0x28, clip_height), alpha);
        pos_x += 0x2A;
    }
}

} // namespace

int PlayerAllItemCheck(int item) {
    return QuestHostPlayerAllItemCheck(item);
}

s32 GetAddAttachItem(s32 item_no) {
    return ((item_no >= ITEM_ATTACH_STAT_START) && (item_no < ITEM_ATTACH_GEM_START)) ? 1 : 0;
}

int TransWepNo(int weapon_no) {
    s32 item_no = weapon_no;

    if (item_no > 0) {
        if (item_no < 0x15) {
            item_no += ITEM_WEAPON_DAGGER_BROKEN - 1;
        } else if ((item_no >= 0x15) && (item_no < 0x21)) {
            item_no += ITEM_WEAPON_WOODENSLINGSHOT_BROKEN - 0x15;
        } else if ((item_no >= 0x21) && (item_no < 0x2E)) {
            item_no += ITEM_WEAPON_MALLET_BROKEN - 0x21;
        } else if ((item_no >= 0x2E) && (item_no < 0x3A)) {
            item_no += ITEM_WEAPON_GOLD_RING_BROKEN - 0x2E;
        } else if ((item_no >= 0x3A) && (item_no < 0x46)) {
            item_no += ITEM_WEAPON_FIGHTING_STICK_BROKEN - 0x3A;
        } else if ((item_no >= 0x46) && (item_no < ITEM_ATTACH_START)) {
            item_no += ITEM_WEAPON_MACHINE_GUN_BROKEN - 0x46;
        }
    }

    return item_no;
}

int TransWepNoNewToOld(int weapon_no) {
    s32 item_no = weapon_no;
    std::println("newitemno is {}", item_no);

    if (item_no >= ITEM_WEAPON_START) {
        if (item_no < ITEM_WEAPON_MARDAN_EINS) {
            item_no -= ITEM_WEAPON_DAGGER_BROKEN - 1;
        } else if ((item_no >= ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) && (item_no < ITEM_WEAPON_MATADOR)) {
            item_no -= ITEM_WEAPON_WOODENSLINGSHOT_BROKEN - 0x15;
        } else if ((item_no >= ITEM_WEAPON_MALLET_BROKEN) && (item_no < ITEM_WEAPON_PLATE_HAMMER)) {
            item_no -= ITEM_WEAPON_MALLET_BROKEN - 0x21;
        } else if ((item_no >= ITEM_WEAPON_GOLD_RING_BROKEN) && (item_no < ITEM_WEAPON_POCKLEKUL)) {
            item_no -= ITEM_WEAPON_GOLD_RING_BROKEN - 0x2E;
        } else if ((item_no >= ITEM_WEAPON_FIGHTING_STICK_BROKEN) && (item_no < ITEM_WEAPON_5_FOOT_NAIL)) {
            item_no -= ITEM_WEAPON_FIGHTING_STICK_BROKEN - 0x3A;
        } else if ((item_no >= ITEM_WEAPON_MACHINE_GUN_BROKEN) && (item_no < ITEM_WEAPON_SNAIL)) {
            item_no -= ITEM_WEAPON_MACHINE_GUN_BROKEN - 0x46;
        }
    }

    std::println("olditemno is {}", item_no);
    return item_no;
}

} // namespace dc::game
