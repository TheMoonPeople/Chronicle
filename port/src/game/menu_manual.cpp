#include "menu_manual.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

s8 ManualTgaNum[24] = {2, 6, 4, 1, 2, 0, 1, 5, 3, 3, 3, 0, 3, 4, 3, 1, 0, 0, 2, 1, 5, 0, 0, 0};

MANUAL_MENU_STATE ManualMenu;

CTexture *ManualMenuTex[6];

/**
 * Returns the texture page for the selected manual category and entry.
 */
int GetNowManualPartTgaNum() {
    return *(ManualMenu.entry + (ManualTgaNum + (ManualMenu.category * 6)));
}

/**
 * Reports whether the party has the item that unlocks the manual.
 */
int GetGameFlagForManualMenu() {
    CDngStatusData *status = SaveData->GetDngStatus();

    if (status == NULL) {
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
            {manual_frame_image, 0, 0},
            {NULL,               0, 0},
            {NULL,               0, 0},
        };
        image_table[0].block_no = ManualMenu.image_texture_block;
        image_table[1].block_no = ManualMenu.image_texture_block;
        BG_READ_INFO *primary = GetReadBGFile(0);
        char          pack_name[32] = "%d_%d.img";
        sprintf(pack_name, pack_name, ManualMenu.category + 1, ManualMenu.entry + 1);
        image_table[1].name = (char *) GetPackFile((u_int *) primary->buffer, pack_name, NULL);
        TexManager.DeleteTextureBlock(ManualMenu.image_texture_block);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, image_table);

        BG_READ_INFO *extra = GetReadBGFile(1);

        if (extra != NULL) {
            LOADTEXTURE_INFO2 extra_table[] = {
                {"#frame_image1#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                {NULL,                                        0, 0},
                {NULL,                                        0, 0},
            };
            extra_table[0].block_no = ManualMenu.extra_texture_block;
            extra_table[1].block_no = ManualMenu.extra_texture_block;
            char extra_name[32] = "%d_%db.img";
            sprintf(extra_name, extra_name, ManualMenu.category + 1, ManualMenu.entry + 1);
            extra_table[1].name = (char *) GetPackFile((u_int *) extra->buffer, extra_name, NULL);
            TexManager.DeleteTextureBlock(ManualMenu.extra_texture_block);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, extra_table);
        }

        int image_count = GetNowManualPartTgaNum();

        for (int image = 0; image < image_count; image++) {
            char *prefix[] = {"a_", "b_", "c_", "d_"};
            char  texture_name[32];
            strcpy(texture_name, prefix[ManualMenu.category]);
            strcat(texture_name, "%d_%d");
            sprintf(texture_name, texture_name, ManualMenu.entry + 1, image + 1);
            ManualMenuTex[image] = TexManager.GetTexture(texture_name, -1);
        }

        ManualMenu.message_page = 0;
        ManualMenu.image_page = 0;
        ManualMenu.images_ready = true;
    }

    return ManualMenu.images_ready;
}

void DrawPrevNextCursor() {
    CTexture *texture = TexManager.GetTexture("mncursor", -1);

    if (texture == NULL) {
        return;
    }

    CRect_i_ texel(0x40, 0, 0x20, 0x20);
    int      x[] = {0x40, 0x222};
    int      image_count = GetNowManualPartTgaNum();

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

/**
 * Empties a message window and puts its layout back to the defaults.
 */
static inline void ResetManualMessage(ClsMes *message) {
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

    for (int slot = 0; slot < 10; slot++) {
        message->mes_no[slot] = -1;
    }

    for (int value = 0; value < 8; value++) {
        message->values[value] = 0;
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

    for (int line = 0; line < 10; line++) {
        message->line_pos[line].x = -1;
        message->line_pos[line].y = -1;
    }
}

int SetManualMsgBuffer() {
    if (ManualMenu.messages_ready == 0 && ReadBGSync() == 0) {
        BG_READ_INFO     *archive = GetReadBGFile(0);
        LOADTEXTURE_INFO2 font_table[] = {
            {manual_frame_image, 0, 0},
            {NULL,               0, 0},
            {NULL,               0, 0},
        };
        font_table[0].block_no = ManualMenu.common_texture_block;
        font_table[1].block_no = ManualMenu.common_texture_block;
        char font_name[32] = "mncursor.img";
        font_table[1].name = (char *) GetPackFile((u_int *) archive->buffer, font_name, NULL);
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

        for (int slot = 0; slot < 10; slot++) {
            ClsMes *window = ManualMsg;
            window->mes_no[slot] = -1;
        }

        s16 *messages = (s16 *) GetPackFile((u_int *) archive->buffer, "manual.bin", NULL);
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

        for (int slot = 0; slot < 10; slot++) {
            CommonMenuMes3.mes_no[slot] = -1;
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
    ClsMes *msg = ManualMsg;
    msg->cursor_row = -1;
    ManualMsg->stay_frame = false;
}

int GetNowManualMenuMode() {
    return ManualMenu.mode;
}

/**
 * Puts a message window's choice cursor on a line, or takes it away for a negative line.
 */
static inline void SetCursorRow(ClsMes *message, int row) {
    message->cursor_row = row;
}

int MenuManualKey();

void MenuManualDraw();

