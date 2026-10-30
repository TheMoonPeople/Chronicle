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
#include "fishing/host.hpp"
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
#include "ui/framework/widgets.hpp"
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

// The autosave's board is labelled in place of a number, and a save naming items no loaded pack
// provides has a "!" after its number.
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
    if (std::ranges::contains(SaveSlots.missing, info->file_no)) {
        static GameText mark;
        mark.Set("!");
        mark.Draw(draw_x + 2, draw_y - 2, start_alpha);
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

s32 MiniEventTextureBlock;

namespace {

// The event item board: the items an event takes, fishing's bait or the rods, in slots five to a row
// behind a window two rows high, with the item's description below. It is retail's board, drawn with
// the UI framework's widgets: the same pictures in the same places, the same motion and sounds.

// Where the board's parts are from its top left corner.
constexpr int kSlotsX = 0x1C;      // the slots' window
constexpr int kSlotsY = 0x38;
constexpr int kSlotsWidth = 0xD2;
constexpr int kSlotsHeight = 0x50;
constexpr int kIconX = 0x20;       // an icon in its slot
constexpr int kIconY = 4;
constexpr int kHandX = 6;          // the hand beside its slot
constexpr int kHandY = 0x3C;
constexpr int kBarY = 0x3C;        // the scroll bar's top
constexpr int kBarTravel = 68;     // how far it goes
constexpr int kTextX = 20;         // the description
constexpr int kTextY = 146;

// The slot's backing in the board's picture, and the scroll bar's.
constexpr Rect kSlotPicture(0x116, 0, 0x28, 0x28);
constexpr Rect kBarPicture(0, 0xE4, 8, 0xC);

struct ItemBoard {
    float       left = 0.0f;
    float       top = 0.0f;
    SlotGrid    grid;
    MenuHand    hand;
    float       bar_y = 0.0f;
    ITEM_PACK  *pack = nullptr;
    CTexture   *event_picture = nullptr; // the board for an event's items
    CTexture   *bait_picture = nullptr;  // the board for bait
    bool        ready = false;           // its pictures are loaded
    MenuPointer pointer;
    bool        active = false;          // still open after its last tick
};

ItemBoard g_board;

bool Listed(int item) {
    return EventItemListed(item, MiniMenu.fish_mode);
}

// The listed items in the pack's order, and where the n-th sits in the pack.
int ListedSlot(int listed) {
    for (int slot = 0, seen = 0; slot < g_board.pack->num; slot++) {
        if (Listed(g_board.pack->item[slot]) && seen++ == listed) {
            return slot;
        }
    }
    return -1;
}

int BoardRows() {
    return std::max(g_board.pack->num / 5, 1);
}

} // namespace

bool EventItemListed(int item, int fish_mode) {
    if (item < ITEM_DUNGEON_START) {
        return false;
    }
    switch (fish_mode) {
        case EVENT_ITEM_MODE_BAIT:
            return fishing::BaitIndex(item) >= 0;
        case EVENT_ITEM_MODE_ROD:
            return fishing::IsRod(item);
        default:
            return true;
    }
}

void InitEventItemSelect(int block, int *usable, ITEM_PACK *pack, int x, int y, int vanish, int fish_mode) {
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    MiniMenu.fish_mode = fish_mode;
    MiniEventTextureBlock = block;
    MiniMenu.lang = GetMenuLangFlag();
    s8 lang_y[7] = {0, -0x10, -0x10, -0x10, -0x10, -0x10, -0x10};
    g_board.left = x;
    g_board.top = static_cast<float>(y) + static_cast<float>(lang_y[MiniMenu.lang]) + 20.0f;
    g_board.pack = pack;
    MiniMenu.event_item_num = 0;

    for (int i = 0; i < pack->num; i++) {
        if (Listed(pack->item[i])) {
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

    const int left = static_cast<int>(g_board.left);
    const int top = static_cast<int>(g_board.top);
    g_board.grid = SlotGrid(SlotGridLayout{.window = Rect(left + kSlotsX, top + kSlotsY, kSlotsWidth, kSlotsHeight)});
    g_board.grid.Reset(pack->num);
    g_board.bar_y = static_cast<float>(kBarY) + g_board.top;
    const Rect at = g_board.grid.CursorRect();
    g_board.hand.SnapTo(static_cast<float>(at.x - (kSlotsX - kHandX)), static_cast<float>(at.y + (kHandY - kSlotsY)));
    MiniMenu.state = MINI_MENU_FADE_IN;
    g_board.ready = false;
}

namespace {

void LoadBoardPictures() {
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
    g_board.event_picture = TexManager.GetTexture("eventmnu", -1);
    g_board.bait_picture = TexManager.GetTexture("fishmnu", -1);
    ItemIcon = TexManager.GetTexture("itemicon", -1);
    InitMenuMesSet(MENU_MES_SET_EVENT_ITEM, reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "eventuse.bin", nullptr)));
    CommonMenuMes2.MakeMesWin(0);
    CommonMenuMes2.Step();
    g_board.ready = true;
}

// The pick: the item under the cursor, taken if the board accepts it. Fishing's boards take whatever
// they list (every bait the fishing framework knows, more than usable holds; every rod); an event's
// takes what it asked for, and is answered with any other.
int Pick(int *result) {
    const int  slot = g_board.grid.Cursor() < MiniMenu.event_item_num ? ListedSlot(g_board.grid.Cursor()) : -1;
    ITEM_PACK *pack = g_board.pack;

    if (slot < 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 0;
    }

    *result = pack->item[slot];
    bool accepted = MiniMenu.fish_mode != 0 && Listed(*result);

    for (int i = 0; !accepted && MiniMenu.usable[i] >= ITEM_DUNGEON_START && i < 13; i++) {
        accepted = MiniMenu.usable[i] == *result;
    }

    if (accepted && MiniMenu.vanish != 0) {
        pack->item[slot] = 0;
    }

    ComMenuSePlay(MENU_SOUND_CONFIRM);

    if (accepted) {
        return 1;
    }

    std::println("Miss\tselected itemNo = {}", *result);

    if (MiniMenu.fish_mode != 0) {
        MiniMenu.state = MINI_MENU_REFUSED;
        return 0;
    }

    return 1;
}

// The mouse, ahead of the pad's read of this tick: a hover puts the cursor on the slot, the left
// button takes it, the right backs out (or closes a refusal), the wheel moves a row.
void BoardPointer() {
    if (!g_board.pointer.IsOpen()) {
        g_board.pointer.Open();
    }
    const MenuPointerTake take = g_board.pointer.Take(g_board.hand.X(), g_board.hand.Y());
    MenuPointerShow(&g_board.pointer);
    if (!g_board.ready) {
        return;
    }
    if (MiniMenu.state == MINI_MENU_REFUSED) {
        if ((take.clicked & 3) != 0) {
            MenuPointerPress(kInputCircle);
        }
        return;
    }
    if (MiniMenu.state != MINI_MENU_CHOOSING) {
        return;
    }
    if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
        return;
    }
    const int hit = take.pointing ? g_board.grid.Hit(g_board.pointer.x, g_board.pointer.y, MiniMenu.event_item_num) : -1;
    if (hit >= 0 && take.moved && take.wheel == 0.0f) {
        g_board.grid.SetCursor(hit);
        if ((take.clicked & 1) != 0) {
            MenuPointerPress(kInputCross);
        }
    }
    if (take.pointing && take.wheel != 0.0f) {
        MenuPointerPress(take.wheel > 0.0f ? kInputUp : kInputDown);
    }
}

int EventItemSelectKey(int *result) {
    if (!g_board.ready) {
        if (ReadBGSync() != 0) {
            return 0;
        }
        LoadBoardPictures();
    }

    int done = 0;

    switch (MiniMenu.state) {
        case MINI_MENU_REFUSED:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                MiniMenu.state = MINI_MENU_CHOOSING;
            }

            break;
        case MINI_MENU_CHOOSING: {
            const bool moved = g_board.grid.Move((GamePad.Down(PAD_RIGHT) != 0) - (GamePad.Down(PAD_LEFT) != 0),
                                                 (GamePad.Down(PAD_DOWN) != 0) - (GamePad.Down(PAD_UP) != 0));

            if (moved) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_SQUARE) != 0) {
                SeitonItemBoard(g_board.pack);
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                break;
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                done = Pick(result);
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                *result = -1;
                done = 1;
            }

            break;
        }
        default:
            break;
    }

    const int slot = g_board.grid.Cursor() < MiniMenu.event_item_num ? ListedSlot(g_board.grid.Cursor()) : -1;
    const int item = slot >= 0 ? g_board.pack->item[slot] : 0;
    const int message = 0 < item ? item + 500 : 0;

    if (CommonMenuMes2.mes_made != message) {
        CommonMenuMes2.MakeMesWin(message);
    }

    return done;
}

// One of the board's columns: its top, its middle stretched by the language's extra height, and its
// foot, from the picture's column at u.
void DrawBoardColumn(CTexture *picture, int x, int y, int width, int extra_height, int u, int picture_width, int alpha) {
    Picture::Draw(picture, Rect(x, y, width, 0x94), Rect(u, 0, picture_width, 0x94), alpha);
    y += 0x94;
    Picture::Draw(picture, Rect(x, y, width, extra_height + 0x32), Rect(u, 0x94, picture_width, 0x14), alpha);
    y += extra_height + 0x32;
    Picture::Draw(picture, Rect(x, y, width, 0x1E), Rect(u, 0xC6, picture_width, 0x1E), alpha);
}

// The board's frame over the slots, wider and taller in the languages with longer words, and its
// scroll bar.
void DrawBoardFrame(CTexture *picture, int x, int y, int alpha, int lang) {
    s8  kakudai_tate_lang[7] = {0, 16, 16, 16, 16, 16, 16};
    s8  kakudai_yoko_lang[7] = {0, 10, 10, 20, 10, 16, 14};
    int extra_height = kakudai_tate_lang[lang];
    int edge = kakudai_yoko_lang[lang] + 6;

    DrawBoardColumn(picture, x + 0x1C, y, 0xD2, extra_height, 0x1C, 0xD2, alpha);
    DrawBoardColumn(picture, x + 0x1C - edge, y, edge, extra_height, 0x14, 6, alpha);
    DrawBoardColumn(picture, x + 8 - edge, y, 0x14, extra_height, 0, 0x14, alpha);
    DrawBoardColumn(picture, x + 0xEE, y, edge, extra_height, 0xEC, 6, alpha);
    DrawBoardColumn(picture, x + 0xEE + edge, y, 0x20, extra_height, 0xF4, 0x20, alpha);

    const int   rows = BoardRows();
    const float bar_height = std::min(136.0f / rows, 68.0f);
    g_board.bar_y += (static_cast<int>((y + kBarY) + (static_cast<float>(kBarTravel) * g_board.grid.TopRow()) / rows) - g_board.bar_y) / 4.0f;
    Picture::Draw(picture, Rect(x + 0xF6 + edge, static_cast<int>(g_board.bar_y), kBarPicture.w, static_cast<int>(bar_height)), kBarPicture, alpha);
}

// A row of slot backings, cut to the window: retail drew each row a pixel low, which the rows below
// hid but which left the window's first line uncovered, a dark line of the town above the top row.
void DrawSlotRow(CTexture *picture, int row, int alpha) {
    const Rect window = g_board.grid.Layout().window;
    const Rect first = g_board.grid.SlotRect(row * 5);
    int        y = first.y;
    int        v = kSlotPicture.y;
    int        height = kSlotPicture.h;

    if (y + height <= window.Top() || window.Bottom() <= y) {
        return;
    }

    MenuTextureClip(y, v, height, window.Top(), window.Bottom());

    for (int column = 0; column < 5; column++) {
        Picture::Draw(picture, Rect(first.x + column * g_board.grid.Layout().stride_x, y, kSlotPicture.w, height),
                      Rect(kSlotPicture.x, v, kSlotPicture.w, height), alpha);
    }
}

void EventItemSelectDraw() {
    if (!g_board.ready) {
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

    const int  left = static_cast<int>(g_board.left);
    const int  top = static_cast<int>(g_board.top);
    CTexture  *picture = (MiniMenu.fish_mode == EVENT_ITEM_MODE_BAIT) ? g_board.bait_picture : g_board.event_picture;
    const Rect window = g_board.grid.Layout().window;
    g_board.grid.Ease();

    for (int row = 0; row < g_board.pack->num / 5; row++) {
        DrawSlotRow(picture, row, alpha);
    }

    // Every slot of the pack's, the listed items first: an empty one draws nothing.
    std::array<s16, 100> items{};
    int                  item_count = 0;

    for (int j = 0; j < g_board.pack->num; j++) {
        if (Listed(g_board.pack->item[j])) {
            items[item_count++] = g_board.pack->item[j];
        }
    }

    // Retail stops at the first slot after one drawn below the window.
    for (int i = 0, y = top; i < static_cast<int>(items.size()) && y <= window.Bottom(); i++) {
        const Rect slot = g_board.grid.SlotRect(i);
        y = slot.y + kIconY;
        ItemIcon::DrawInSlot(items[i], slot.x + (kIconX - kSlotsX), y, window.Top(), window.Bottom(), alpha);
    }

    DrawBoardFrame(picture, left, top, alpha, MiniMenu.lang);
    const Rect at = g_board.grid.CursorRect();
    StayFrame::Draw(Rect(at.x - 6, at.y - 5, 0x24, 0x24), alpha);
    g_board.hand.Ease(static_cast<float>(at.x - (kSlotsX - kHandX)), static_cast<float>(at.y + (kHandY - kSlotsY)));
    g_board.hand.Draw(0x40);
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
    DrawMenuClsMes(&CommonMenuMes2, kTextX + left + message_pos[MiniMenu.lang][0], kTextY + top + message_pos[MiniMenu.lang][1]);

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

/**
 * Frees the event item selection menu's textures and releases the pad.
 */
void EventItemSelectExit() {
    TexManager.DeleteTextureBlock(MiniEventTextureBlock);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

} // namespace

int EventItemSelectLoop(int *result) {
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

    g_board.active = done == 0;
    return done;
}

bool EventItemPointerUpdate() {
    const bool active = std::exchange(g_board.active, false);
    if (!active) {
        return !g_board.pointer.Close();
    }
    BoardPointer();
    return true;
}

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
