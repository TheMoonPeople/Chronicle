#include "menu_manual.hpp"

#include <array>
#include <format>
#include <string>
#include <string_view>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace {

constexpr int ENTRIES_PER_CATEGORY = 6;

/** Number of page images in each manual entry, by category and entry. */
constexpr std::array<s8, 24> ManualTgaNum = {2, 6, 4, 1, 2, 0, 1, 5, 3, 3, 3, 0, 3, 4, 3, 1, 0, 0, 2, 1, 5, 0, 0, 0};

/** Texture name prefixes of each category's page images. */
constexpr std::array<std::string_view, 4> ManualImagePrefixes = {"a_", "b_", "c_", "d_"};

/**
 * Empties a message window and puts its layout back to the defaults.
 */
void ResetManualMessage(ClsMes *message) {
    message->text_columns = MES_WIN_COLUMNS;
    message->text_rows = 10;
    message->text_len = 0;
    message->text_width = 0;
    message->text_height = 0;
    message->fade = 0.0f;
    message->fade_in = true;
    message->text_rate = message->text_rate_set;
    message->waiting = false;
    message->text_at = 0.0f;
    message->text_no = 0;
    message->text_from = 0;
    message->page_from = 0;
    message->InitMesWinTbl();
    message->clut_now = message->clut_default;
    message->wait = 0;
    message->blink = 0;
    message->auto_page_wait = 0;
    message->mes_made = -1;
    message->edge_alpha = 0x80;

    for (s32 &message_no : message->mes_no) {
        message_no = -1;
    }

    for (s32 &value : message->values) {
        value = 0;
    }

    message->value = 0;
    message->value_signed = false;
    message->value_show = true;
    message->value_narrow = false;
    message->space_width = -1;
    message->space_area = -1;
    message->cursor_row = -1;
    message->cursor_y = 0;
    message->cursor_lit = false;

    for (MES_LINE_POS &position : message->line_pos) {
        position.x = -1;
        position.y = -1;
    }
}

} // namespace

MANUAL_MENU_STATE ManualMenu;

CTexture *ManualMenuTex[6];

int GetNowManualPartTgaNum() {
    return ManualTgaNum[(ManualMenu.category * ENTRIES_PER_CATEGORY) + ManualMenu.entry];
}

int GetGameFlagForManualMenu() {
    CDngStatusData *status = SaveData->GetDngStatus();

    if (status == nullptr) {
        return 1;
    }

    int result = 0;

    if (status->SearchItemIndexNo(ITEM_MANUAL) >= 0) {
        result++;
    }

    return result;
}

s16 ManualImgEnter() {
    if (ManualMenu.images_ready == 0 && ReadBGSync() == 0) {
        LOADTEXTURE_INFO2 image_table[] = {
            {.name = manual_frame_image, .block_no = 0, .mipmap = 0},
            {.name = nullptr,            .block_no = 0, .mipmap = 0},
            {.name = nullptr,            .block_no = 0, .mipmap = 0},
        };
        image_table[0].block_no = ManualMenu.image_texture_block;
        image_table[1].block_no = ManualMenu.image_texture_block;
        BG_READ_INFO *primary = GetReadBGFile(0);
        std::string   pack_name = std::format("{}_{}.img", ManualMenu.category + 1, ManualMenu.entry + 1);
        image_table[1].name =
            reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(primary->buffer), pack_name.data(), nullptr));
        TexManager.DeleteTextureBlock(ManualMenu.image_texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, image_table);

        BG_READ_INFO *extra = GetReadBGFile(1);

        if (extra != nullptr) {
            LOADTEXTURE_INFO2 extra_table[] = {
                {.name = "#frame_image1#640#" SCREEN_HEIGHT_STR "#4", .block_no = 0, .mipmap = 0},
                {.name = nullptr,                                     .block_no = 0, .mipmap = 0},
                {.name = nullptr,                                     .block_no = 0, .mipmap = 0},
            };
            extra_table[0].block_no = ManualMenu.extra_texture_block;
            extra_table[1].block_no = ManualMenu.extra_texture_block;
            std::string extra_name = std::format("{}_{}b.img", ManualMenu.category + 1, ManualMenu.entry + 1);
            extra_table[1].name =
                reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(extra->buffer), extra_name.data(), nullptr));
            TexManager.DeleteTextureBlock(ManualMenu.extra_texture_block);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, extra_table);
        }

        int image_count = GetNowManualPartTgaNum();

        for (int image = 0; image < image_count; image++) {
            std::string texture_name =
                std::format("{}{}_{}", ManualImagePrefixes[ManualMenu.category], ManualMenu.entry + 1, image + 1);
            ManualMenuTex[image] = TexManager.GetTexture(texture_name.data(), -1);
        }

        ManualMenu.message_page = 0;
        ManualMenu.image_page = 0;
        ManualMenu.images_ready = true;
    }

    return ManualMenu.images_ready;
}

void DrawPrevNextCursor() {
    CTexture *texture = TexManager.GetTexture("mncursor", -1);

    if (texture == nullptr) {
        return;
    }

    CRect_i_                     texel(0x40, 0, 0x20, 0x20);
    constexpr std::array<int, 2> x = {0x40, 0x222};
    int                          image_count = GetNowManualPartTgaNum();

    for (int cursor = 0; cursor < 2; cursor++, texel.y += 0x20) {
        if (cursor == 0 && ManualMenu.image_page == 0) {
            continue;
        }

        if (cursor == 1 && ManualMenu.image_page == image_count - 1) {
            return;
        }

        CRect_i_ screen(x[cursor], 0x106, texel.width, texel.height);
        DrawMenu2DSprite(texture, screen, texel, 0x80);
    }
}

int InitMenuManual(int *texture_blocks, u_long128 *load_buffer) {
    ManualMenu.load_buffer = load_buffer;
    ManualMenu.load_buffer = MenuCalcBufAlignment(ManualMenu.load_buffer);
    u_long128 *buffer = ManualMenu.load_buffer;
    StartReadBG();
    LoadFileBGMenuData("manual/mndata.pak", buffer);
    ReadBG();
    ManualMenu.messages_ready = false;
    ManualMenu.images_ready = false;
    ManualMenu.common_texture_block = texture_blocks[0];
    ManualMenu.image_texture_block = texture_blocks[1];
    ManualMenu.extra_texture_block = texture_blocks[2];
    ManualMenu.mode = MANUAL_MODE_LOADING;
    ManualMenu.category = 0;
    ManualMenu.selection_level = -2;
    ManualMenu.entry = 0;
    ManualMenu.transition_frame = 0;
    ManualMsg = &EastKingMsgCls;
    ManualMsg->edge_alpha = 0;
    ManualMsg->unk_02C = 0x10;
    ManualMsg->init_030 = 0x10;
    ManualMenu.common_message_buffer = CommonMenuMes3.buff;
    ManualMenu.menu_message_buffer = CommonMenuMes1.buff;
    ManualMenu.char_width = CommonMenuMes3.char_width;
    return 1;
}

int SetManualMsgBuffer() {
    if (ManualMenu.messages_ready == 0 && ReadBGSync() == 0) {
        BG_READ_INFO     *archive = GetReadBGFile(0);
        LOADTEXTURE_INFO2 font_table[] = {
            {.name = manual_frame_image, .block_no = 0, .mipmap = 0},
            {.name = nullptr,            .block_no = 0, .mipmap = 0},
            {.name = nullptr,            .block_no = 0, .mipmap = 0},
        };
        font_table[0].block_no = ManualMenu.common_texture_block;
        font_table[1].block_no = ManualMenu.common_texture_block;
        char font_name[] = "mncursor.img";
        font_table[1].name =
            reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), font_name, nullptr));
        TexManager.DeleteTextureBlock(ManualMenu.common_texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, font_table);
        ManualMenu.load_buffer = archive->buffer + (archive->size >> 4) + 1;

        ResetManualMessage(ManualMsg);
        ManualMsg->Preset(MES_PRESET_NAME);
        ManualMsg->char_width = GetMenuCommonFontW(GetMenuLangFlag(), -1);
        ManualMsg->char_height = 0x16;
        ManualMsg->tex_block = 0x1A;
        ManualMsg->tex_buff = CommonMenuMes1.tex_buff;

        for (s32 &message_no : ManualMsg->mes_no) {
            message_no = -1;
        }

        char messages_name[] = "manual.bin";
        s16 *messages =
            reinterpret_cast<s16 *>(GetPackFile(reinterpret_cast<u_int *>(archive->buffer), messages_name, nullptr));
        ManualMsg->SetBuff(messages);
        ManualMsg->mes_made = -1;
        ManualMsg->stay_frame = true;
        ManualMsg->MakeMesWin(100);

        ResetManualMessage(&CommonMenuMes3);
        CommonMenuMes3.Preset(MES_PRESET_NAME);
        CommonMenuMes3.centre_rows = false;
        CommonMenuMes3.SetBuff(messages);
        CommonMenuMes3.rows = 3;
        CommonMenuMes3.char_width = ManualMsg->char_width;

        for (s32 &message_no : CommonMenuMes3.mes_no) {
            message_no = -1;
        }

        ManualMenu.messages_ready = true;
    }

    return ManualMenu.messages_ready;
}

void ExitManualMenu() {
    int texture_blocks[] = {ManualMenu.common_texture_block, ManualMenu.image_texture_block,
                            ManualMenu.extra_texture_block, -1};
    MenuTextureDelete(texture_blocks);
    CommonMenuMes3.centre_rows = true;
    CommonMenuMes3.cursor_row = -1;
    CommonMenuMes3.stay_frame = false;
    ManualMsg->cursor_row = -1;
    ManualMsg->stay_frame = false;
}

int GetNowManualMenuMode() {
    return ManualMenu.mode;
}
