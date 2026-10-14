#include "menu_save.hpp"

#include <filesystem>
#include <system_error>

#include "battle_globals.hpp"
#include "gametext.hpp"
#include "localize.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "save_slots.hpp"
#include "texture.hpp"

// Message numbers of allmenu.mes the save screens show.
enum SaveMenuMessage {
    SAVE_MES_NONE = 0,
    SAVE_MES_WANT_TO_SAVE = 260,  /**< "Want to save?" */
    SAVE_MES_SAVED = 262,         /**< "Saving completed." */
    SAVE_MES_REPLACE = 264,       /**< "Data exist on file N. Replace it?" */
    SAVE_MES_SAVE_FAILED = 266,   /**< "Saving failed." */
    SAVE_MES_WANT_TO_LOAD = 270,  /**< "Want to load?" */
    SAVE_MES_LOAD_FAILED = 274,   /**< "Loading failed." */
    SAVE_MES_FORMAT_FAILED = 282, /**< "Formatting failed." */
    SAVE_MES_DELETE = 290,        /**< "Delete file?" */
    SAVE_MES_COPY = 292,          /**< "Want to copy file?" */
    SAVE_MES_OTHER_VERSION = 299, /**< "Data not loaded due to change in data format. Deleting No. N data." */
};

// Retail's, with no message that names the memory card. The card operations finish within the
// frame after the one that starts them and show nothing; the save after the ending asks "Want to
// save?" where retail asked to save the cleared data to the card; the alerts only a card raised
// say the save or the load failed.
PORT_OVERRIDE int GetSaveMenuMsgNo() {
    if (McAccess.GetFuncNo() != MC_OPERATION_IDLE) {
        return SAVE_MES_NONE;
    }

    switch (SaveMenu.key_no) {
        case SAVE_KEY_SAVE_DECIDE:
            return SAVE_MES_REPLACE;
        case SAVE_KEY_LOAD_DECIDE:
            return SAVE_MES_WANT_TO_LOAD;
        case SAVE_KEY_ALERT:
            switch (SaveMenu.alert_no) {
                case SAVE_ALERT_NONE:
                case SAVE_ALERT_UNK_3:
                case SAVE_ALERT_UNK_4:
                case SAVE_ALERT_UNK_5:
                    return SAVE_MES_NONE;
                case SAVE_ALERT_FORMAT_FAILED:
                    return SAVE_MES_FORMAT_FAILED;
                case SAVE_ALERT_SAVE_FAILED:
                    return SAVE_MES_SAVE_FAILED;
                case SAVE_ALERT_LOAD_FAILED:
                    return SAVE_MES_LOAD_FAILED;
                default:
                    return SaveMenu.access_kind == SAVE_ACCESS_LOAD ? SAVE_MES_LOAD_FAILED : SAVE_MES_SAVE_FAILED;
            }
        case SAVE_KEY_DELETE:
            return SAVE_MES_DELETE;
        case SAVE_KEY_COPY:
            return SAVE_MES_COPY;
        case SAVE_KEY_DIF_VERSION:
            return SAVE_MES_OTHER_VERSION;
        case SAVE_KEY_AFTER_ENDING:
            return SAVE_MES_WANT_TO_SAVE;
        case SAVE_KEY_END_SAVE:
        case SAVE_KEY_END_SAVE_ENDING:
            return SAVE_MES_SAVED;
        default:
            return SAVE_MES_NONE;
    }
}

// Retail's looked for a card holding the save directory and its configuration. Data exists once
// state.json or a save folder does.
PORT_OVERRIDE int InitExistData() {
    if (McAccess.InitForMC() != 0) {
        return 0;
    }

    McAccess.port = 0;
    McAccess.LoadSysConfig();
    std::error_code error;
    return std::filesystem::exists(SaveStatePath(), error) || !SaveSlotFiles().empty();
}

namespace {

void GetSaveBoardAlphaInfo(int x, int width, int &start_alpha, int &end_alpha, int alpha) {
    if (x < 0) {
        start_alpha = 0;
    } else if ((0 <= x) && (x < 0x81)) {
        start_alpha = (x * alpha) >> 7;
    } else if ((x > 0x80) && (x < SCREEN_HEIGHT - 0x7F)) {
        start_alpha = (alpha * 0x80) >> 7;
    } else if ((x > SCREEN_HEIGHT - 0x80) && (x < SCREEN_HEIGHT + 1)) {
        start_alpha = ((SCREEN_HEIGHT - x) * alpha) >> 7;
    } else {
        start_alpha = 0;
    }

    x += width;

    if (x < 0) {
        end_alpha = 0;
    } else if ((0 <= x) && (x < 0x81)) {
        end_alpha = (x * alpha) >> 7;
    } else if ((x > 0x80) && (x < SCREEN_HEIGHT - 0x7F)) {
        end_alpha = (alpha * 0x80) >> 7;
    } else if ((x > SCREEN_HEIGHT - 0x80) && (x < SCREEN_HEIGHT + 1)) {
        end_alpha = ((SCREEN_HEIGHT - x) * alpha) >> 7;
    } else {
        end_alpha = 0;
    }
}

} // namespace

// Retail's, with the autosave's board labelled in place of a number.
PORT_OVERRIDE void DrawSaveBoard(SAVEDATA_INFO *info, CTexture **name_texture, int x, int y, int unused, int alpha) {
    int i;
    int draw_x;
    int draw_y;
#ifndef PAL
    int time[3];
#endif

    if (info == NULL) {
        return;
    }

    spRGBA top = {0x80, 0x80, 0x80, 0};
    top.a = alpha;
    spRGBA bottom = {0x80, 0x80, 0x80, 0};
    bottom.a = alpha;
    int start_alpha;
    int end_alpha;
    GetSaveBoardAlphaInfo(y, 0x88, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    DrawMenu2DSprite(SaveBoard, CRect_i_(x, y, 0x180, 0x87), CRect_i_(0, 0, 0x180, 0x88), &top, &top, &bottom, &bottom);

    draw_x = x + 0x54;
#ifdef PAL
    s8 number_shift[] = {0, 0, 0, 10, 0, 4, 13};
    draw_x += number_shift[GetMenuLangFlag()];
    int time[3];
#endif
    draw_y = y + 8;
    RECT number_rect = {0x88, 0xCA, 0xC, 0x10};
    GetSaveBoardAlphaInfo(y, 0x10, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;

    if (info->file_no <= 0) {
        static GameText label;
        label.Set(LocalizeText("save.autosave", "Auto"));
        label.Draw(draw_x - label.Width(), draw_y - 2, start_alpha);
    } else {
        int digits;
        int number = info->file_no;
        int clip_y;
        int src_y;
        int height;
        int digit;
        int width;
        int src_x;

        for (digits = GetNumberKeta(number); 0 < digits; digits--) {
            clip_y = draw_y;
            digit = number % 10;
            width = number_rect.width;
            draw_x -= width - 1;
            src_x = number_rect.x + width * digit;
            src_y = number_rect.y;
            height = number_rect.height;
            MenuTextureClip(clip_y, src_y, height, 0, SCREEN_HEIGHT);
            DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, clip_y, width, height - 1), CRect_i_(src_x, src_y, width, height), &top, &top, &bottom, &bottom);
            number /= 10;
        }
    }

    draw_x = x + 0x1A;
    draw_y = y + 0x1C;
    GetSaveBoardAlphaInfo(y, 0x16, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    int name_x = draw_x + 8;

    DrawSaveBoardCharaName2(name_x, draw_y, (short *) info->name, name_texture, top, bottom);

    // Play time, as hours, minutes and seconds.
    RECT time_rect = {0x88, 0xB8, 0xC, 0x12};
    int  column = x + 0x92;
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
    time[0] = frames / 3600;
    time[1] = (frames / 60 - time[0] * 60) % 60;
    time[2] = frames % 60;

    for (int part = 2; part >= 0; part--) {
        int value = time[part];
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, time_rect.width, time_rect.height), CRect_i_(time_rect.x + time_rect.width * (value / 10), time_rect.y, time_rect.width, time_rect.height), &top, &top, &bottom, &bottom);
        DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x + time_rect.width, draw_y, time_rect.width, time_rect.height), CRect_i_(time_rect.x + time_rect.width * (value % 10), time_rect.y, time_rect.width, time_rect.height), &top, &top, &bottom, &bottom);
        draw_x -= 0x1E;
    }

    CRect_i_ colon(0x100, 0xB8, 0xC, 0x12);
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
    {
        int digits;
        int number = info->quest_total;
        int clip_y;
        int src_y;
        int height;
        int digit;
        int width;
        int src_x;

        for (digits = GetNumberKeta(number); 0 < digits; digits--) {
            clip_y = draw_y;
            digit = number % 10;
            width = number_rect.width;
            draw_x -= width - 1;
            src_x = number_rect.x + width * digit;
            src_y = number_rect.y;
            height = number_rect.height;
            MenuTextureClip(clip_y, src_y, height, 0, SCREEN_HEIGHT);
            DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, clip_y, width, height - 1), CRect_i_(src_x, src_y, width, height), &top, &top, &bottom, &bottom);
            number /= 10;
        }
    }

    // Row of each map's name on the board texture; a map from 14 on is in the second block.
    int map = info->map_no;
    s16 map_name[62] = {0, 1, 2, 3, 6, 0, 0, 0, 0, 0, 0, 1, 1, 1, 9, 9, 9, 9, 9, 2, 2,
                        4, 10, 4, 13, 13, 13, 10, 9, 4, 4, 4, 4, 1, 10, 12, 10, 5, 13, 12, 13, 4,
                        10, 2, 2, 2, 13, 4, 13, 13, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14};
    int name_u;
    int name_v;

    if (map_name[map] < 0xE) {
        name_u = (map_name[map] / 7) * 0x88;
        name_v = (map_name[map] % 7) * 0x14 + 0xB8;
    } else {
        map_name[map] -= 0xE;
        name_u = (map_name[map] / 7) * 0x88;
        name_v = (map_name[map] % 7) * 0x14 + 0x144;
    }

    draw_x = x + 0x23;
    draw_y = y + 0x63;
    GetSaveBoardAlphaInfo(draw_y, 0x14, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;
    DrawMenu2DSprite(SaveBoard, CRect_i_(draw_x, draw_y, 0x88, 0x15), CRect_i_(name_u, name_v, 0x88, 0x14), &top, &top, &bottom, &bottom);

    // A face for each member of the party, three to a row.
    int face_x = x + 0xCF;
    draw_x = face_x;
    draw_y = y + 0x1A;
    GetSaveBoardAlphaInfo(draw_y, 0x30, start_alpha, end_alpha, alpha);
    top.a = start_alpha;
    bottom.a = end_alpha;

    for (i = 0; i < info->party_size; i++) {
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
