#include "menu_save.hpp"

#include <array>
#include <filesystem>
#include <system_error>
#include <utility>

#include "battle_globals.hpp"
#include "gametext.hpp"
#include "localize.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "quest_host.hpp"
#include "rect.hpp"
#include "save_slots.hpp"
#include "texture.hpp"

int PlayerAllItemCheck(int item) {
    return QuestHostPlayerAllItemCheck(item);
}

namespace {

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

// Alpha at screen row y: ramps up over the top 128 rows and down over the bottom 128.
int SaveBoardEdgeAlpha(int y, int alpha) {
    if (y < 0) {
        return 0;
    }
    if (y < 0x81) {
        return (y * alpha) >> 7;
    }
    if (y < SCREEN_HEIGHT - 0x7F) {
        return (alpha * 0x80) >> 7;
    }
    if (y < SCREEN_HEIGHT + 1) {
        return ((SCREEN_HEIGHT - y) * alpha) >> 7;
    }
    return 0;
}

void GetSaveBoardAlphaInfo(int y, int height, int &start_alpha, int &end_alpha, int alpha) {
    start_alpha = SaveBoardEdgeAlpha(y, alpha);
    end_alpha = SaveBoardEdgeAlpha(y + height, alpha);
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

// Message number for the save screens, with no message that names the memory card. Card
// operations finish within the frame after the one that starts them and show nothing; the save
// after the ending asks "Want to save?"; alerts only a card raised report a failed save or load.
int GetSaveMenuMsgNo() {
    return static_cast<int>(SaveMenuMessageFor());
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
