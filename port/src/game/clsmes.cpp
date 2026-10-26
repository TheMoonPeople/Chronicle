#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 821

#include "clsmes.hpp"

#include <libpkt.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "character.hpp"
#include "editpartsdata.hpp"
#include "editpartsinfo.hpp"
#include "gameutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

/* The external-character code of every letter of each registered name. */
s16 NameRegistTbl[8][11];

/* Random tables the message window shakes its text with. */
float RandTbl[64];
float RandTbl2[64];

/* Every pass of the outline the message window draws behind its text. The
 * last entry's alpha ends the run. */
MES_FUCHI FuchiTbl_E[] = {
    {0,  -2, 0, 0, 0, 128},
    {0,  2,  0, 0, 0, 128},
    {2,  0,  0, 0, 0, 128},
    {-2, 0,  0, 0, 0, 128},
    {0,  0,  0, 0, 0, -1 },
};

/** What each of the eight words of a GaijiDataTbl entry says. */
enum {
    GAIJI_CODE,   /**< The external-character code the entry draws. */
    GAIJI_U,      /**< Distance of the character from the left of the texture. */
    GAIJI_V,      /**< Distance of the character from the top of the texture. */
    GAIJI_WIDTH,  /**< How wide the character is in the texture. */
    GAIJI_HEIGHT, /**< How tall the character is in the texture. */
    GAIJI_X_OFF,  /**< Extra distance the character draws to the right. */
    GAIJI_Y_OFF,  /**< Extra distance the character draws down. */
    GAIJI_CELLS,  /**< Font cells the character takes across. */
};

/**
 * The tag that opens a run of GS register writes, as the four words the GIF
 * reads it out of.
 */
struct GIFTAG {
    u32 word[4];
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(GIFTAG) == 0x10);

s16 ClsMes::GetGaijiW(int code) {
    if (code >= -0x300 && code < -0x251) {
        if (this->narrow_gaiji != 0) {
            if (code == -0x2BD) {
                return 5;
            }

            if (code == -0x2BA) {
                return 5;
            }

            if (code == -0x2AB) {
                return 5;
            }
        }

        EDIT_GAIJI *table = (EDIT_GAIJI *) &EditPartsData[0x491C];
        EDIT_GAIJI *gaiji = &EditGaijiTbl[code];
        s16         width = this->char_width;

        return width * gaiji->width;
    }

    return this->char_width;
}

int ClsMes::GetNameLen(int chara) {
    if (chara < 0) {
        return -1;
    }

    if (chara >= 6) {
        return -1;
    }

    short *name = (short *) SaveData->GetCharaName(chara);

    for (int i = 0; i < 11; i++) {
        if ((u16) name[i] == 0) {
            return i;
        }
    }

    return -1;
}

int ClsMes::GetNameWidth(int chara) {
    if (chara < 0) {
        return -1;
    }

    if (chara >= 6) {
        return -1;
    }

    u16 *name = (u16 *) SaveData->GetCharaName(chara);
    s16  width = 0;

    for (int i = 0; i < 11; i++) {
        if (name[i] == 0) {
            return width;
        }

        int code = NameRegistCodeJtoE(name[i] - 1);

        if (code >= -0x300 && code < -0x250) {
            width += this->GetGaijiW(code);
        } else {
            width += this->char_width;
        }
    }

    return -1;
}

void Myset2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &src, const CRect_i_ &dst, u8 r, u8 g, u8 b, u8 a);

void ClsMes::AutoSetSub(CCharacter *first, CCharacter *second, int *out_pos) {
    GetScrPosFromChar(first, out_pos);
    GetScrPosFromChar(second, &out_pos[2]);
}

void ClsMes::AutoSet(int *pos) {
    RECT  candidate[3][3];
    int   outside[3][3];
    float clearance[3][3];
    int   speaker_x;
    int   speaker_y;
    int   other_x;
    int   other_y;
    int   width;
    int   height;
    int   col;
    int   row;
    int   best_col;
    int   best_row;

    speaker_x = pos[0];
    speaker_y = pos[1];
    other_x = pos[2];
    other_y = pos[3];

    width = 0xCA;
    height = 0x8A;

    if (this->win_width > 0xCA) {
        width = this->win_width;
    }

    if (this->win_height > 0x8A) {
        height = this->win_height;
    }

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            candidate[row][col].height = height;

            if (col == 0) {
                candidate[row][col].x = 0;
            } else {
                candidate[row][col].x = col * (0x260 - width) >> 1;
                candidate[row][col].x += 0x10;
            }

            if (row == 0) {
                candidate[row][col].y = 0;
            } else {
                candidate[row][col].y = row * (0x1A0 - height) >> 1;
                candidate[row][col].y += 0x10;
            }

            if (col == 0 || col == 2) {
                candidate[row][col].width = width + 0x10;
            } else {
                candidate[row][col].width = width;
            }

            if (row == 0 || row == 2) {
                candidate[row][col].height = height + 0x10;
            } else {
                candidate[row][col].height = height;
            }
        }
    }

    best_col = 0;
    best_row = 0;

    if (this->auto_pos == MES_POS_AUTO) {
        for (row = 0; row < 3; row++) {
            for (col = 0; col < 3; col++) {
                RECT *rect = &candidate[row][col];

                outside[row][col] = CheckPosInOutForRect(rect, speaker_x, speaker_y);

                if (outside[row][col] == 0) {
                    outside[row][col] = CheckPosInOutForRect(rect, other_x, other_y);
                }
            }
        }

        for (row = 0; row < 3; row++) {
            for (col = 0; col < 3; col++) {
                if (outside[row][col] != 0) {
                    clearance[row][col] = -1.0f;
                }

                RECT *rect = &candidate[row][col];

                clearance[row][col] = GetDisPosToRect(rect, speaker_x, speaker_y);

                float other_clearance = GetDisPosToRect(rect, other_x, other_y);

                if (other_clearance < clearance[row][col]) {
                    clearance[row][col] = other_clearance;
                }
            }
        }

        float best_clearance = 0.0f;

        for (row = 0; row < 3; row++) {
            for (col = 0; col < 3; col++) {
                if (clearance[row][col] >= 0.0f && best_clearance < clearance[row][col]) {
                    best_col = col;
                    best_row = row;
                    best_clearance = clearance[row][col];
                }
            }
        }
    } else {
        best_col = (this->auto_pos - 1) % 3;
        best_row = (this->auto_pos - 1) / 3;
    }

    this->win_x = candidate[best_row][best_col].x;
    this->win_y = candidate[best_row][best_col].y;

    if (this->win_width < 0xCA) {
        this->win_x += (0xCA - this->win_width) >> 1;
    }

    if (this->win_height < 0x8A) {
        this->win_y += (0x8A - this->win_height) >> 1;
    }

    if (best_col == 0) {
        this->win_x += 0x10;
    }

    if (best_row == 0) {
        this->win_y += 0x10;
    }

    this->text_x = this->win_x + (int) (3.0f * this->char_width);
    this->text_y = this->win_y + (int) (1.5f * this->char_height);

    if (this->tail_on != 0) {
        this->tail_to_x = speaker_x;
        this->tail_to_y = speaker_y;
    }

    this->grow_x = this->win_x + (this->win_width >> 1);
    this->grow_y = this->win_y + (this->win_height >> 1);

    if (this->tail_on != 0) {
        if (this->tail_to_x <= this->win_x + (int) (this->win_width >> 2)) {
            this->tail_x = this->win_x + (int) (this->win_width >> 2);
        } else if (this->win_x + (int) (this->win_width * 3 >> 2) <= this->tail_to_x) {
            this->tail_x = this->win_x + (int) (this->win_width * 3 >> 2);
        } else {
            this->tail_x = this->tail_to_x;
        }

        if (this->tail_to_y <= this->win_y + (int) (this->win_height >> 2)) {
            this->tail_y = this->win_y + (int) (this->win_height >> 2);
        } else if (this->win_y + (int) (this->win_height * 3 >> 2) <= this->tail_to_y) {
            this->tail_y = this->win_y + (int) (this->win_height * 3 >> 2);
        } else {
            this->tail_y = this->tail_to_y;
        }
    }
}

void ClsMes::AbsFukidashiIn() {
    if (this->win_x < 0x10) {
        this->win_x = 0x10;
        this->text_x = this->win_x + (int) (3.0f * this->char_width);
    } else if (this->win_x + this->win_width >= 0x270) {
        this->win_x = 0x270 - this->win_width;
        this->text_x = this->win_x + (int) (3.0f * this->char_width);
    }

    if (this->win_y < 0x10) {
        this->win_y = 0x10;
        this->text_y = this->win_y + (int) (1.5f * this->char_height);
    } else if (this->win_y + this->win_height >= 0x1B0) {
        this->win_y = 0x1B0 - this->win_height;
        this->text_y = this->win_y + (int) (1.5f * this->char_height);
    }
}

ClsMes::ClsMes() {
    this->text_x = 0x64;
    this->text_y = 0x32;
    this->char_width = 0xB;
    this->char_height = 0x14;
    this->columns = MES_WIN_COLUMNS;
    this->rows = 0xA;
    this->text_columns = MES_WIN_COLUMNS;
    this->text_rows = 0xA;
    this->text_len = 0;
    this->text_width = 0;
    this->text_height = 0;
    this->unk_02C = 0x10;
    this->init_030 = 0x10;
    this->fukidashi = true;
    this->fukidashi_shape = 0;
    this->grow_x = 0x140;
    this->grow_y = SCREEN_HALF_HEIGHT;
    this->win_x = this->text_x;
    this->win_y = this->text_y;
    this->win_width = this->char_width * this->columns;
    this->win_height = this->char_height * this->rows;
    this->auto_pos = MES_POS_AUTO;
    this->tail_on = true;
    this->tail_x = 0x12C;
    this->tail_y = 0xC8;
    this->tail_to_x = 0x140;
    this->tail_to_y = 0xC8;
    this->tail_half_width = 8;
    this->tail_length = 0x40;
    this->stay_frame = false;
    this->stay_width = 0;
    this->stay_height = 0;
    this->text_rate = 0.5f;
    this->text_rate_set = 0.5f;
    this->waiting = false;
    this->auto_page = false;
    this->fade_speed = 0.01f;
    this->fade = 0.0f;
    this->fade_in = true;
    this->text_at = 0.0f;
    this->text_no = 0;
    this->text_from = 0;
    this->page_from = 0;
    this->clut_default = 1;
    this->clut_now = this->clut_default;
    this->wait = 0;
    this->blink = 0;
    this->auto_page_wait = 0;
    this->mes_made = -1;
    this->style = MES_EDGE_DOUBLE;
    this->page_arrow = true;
    this->end_mark = false;
    this->centre_rows = true;
    this->page_mark_style = true;
    this->edge_alpha = 0x80;
    this->narrow_gaiji_set = 0;
    this->narrow_gaiji = false;

    for (int i = 0; i < 10; i++) {
        this->mes_no[i] = -1;
    }

    for (int i = 0; i < 8; i++) {
        this->values[i] = 0;
    }

    this->value = 0;
    this->value_signed = false;
    this->value_show = true;
    this->value_narrow = false;
    this->space_width = -1;
    this->space_area = -1;
    this->cursor_row = -1;
    this->cursor_y = 0;
    this->cursor_lit = false;
    this->tex_block = 0;

    for (int i = 0; i < 10; i++) {
        this->line_pos[i].x = -1;
        this->line_pos[i].y = -1;
    }

    this->buff = NULL;
    this->buff_system = NULL;
    this->text = NULL;
    this->text_system = NULL;
    this->tex_buff = NULL;
    this->clut = MesWinClut;
}

void ClsMes::SetBuff(short *buff) {
    this->buff = buff;
    this->text = (char *) this->buff;
    this->text += (u16) this->buff[1];
}

void ClsMes::SetBuff_system(short *buff) {
    this->buff_system = buff;
    this->text_system = (char *) this->buff_system;
    this->text_system += (u16) this->buff_system[1];
}

void ClsMes::Preset(int style) {
    switch (style) {
        case MES_PRESET_SYSTEM:
            this->fukidashi = false;
            this->text_rate = 0.0f;
            this->text_rate_set = 0.0f;
            this->clut_default = Color2Clut(FONT_COLOR_WHITE) & 0xFF;
            this->clut_now = this->clut_default;
            this->style = MES_EDGE_BLACK;
            this->page_arrow = false;
            this->centre_rows = true;
            this->page_mark_style = false;
            break;
        case MES_PRESET_FUKIDASHI:
            this->fukidashi = true;
            this->clut_default = Color2Clut(FONT_COLOR_BROWN) & 0xFF;
            this->clut_now = this->clut_default;
            this->style = MES_EDGE_WHITE;
            this->page_arrow = true;
            this->centre_rows = true;
            this->page_mark_style = true;
            break;
        case MES_PRESET_AUTO_PAGE:
            this->style = MES_EDGE_NONE;
            this->page_arrow = false;
            this->centre_rows = true;
            this->auto_page = true;
            break;
        case MES_PRESET_NAME:
            this->fukidashi = false;
            this->text_rate = 0.0f;
            this->text_rate_set = 0.0f;
            this->clut_default = Color2Clut(FONT_COLOR_GOLD) & 0xFF;
            this->clut_now = this->clut_default;
            this->style = MES_EDGE_TABLE;
            this->page_arrow = false;
            this->page_mark_style = false;
            break;
        case MES_PRESET_NONE:
            break;
    }
}

void ClsMes::SetMesFukidashi(int style) {
    switch (style) {
        case MES_FUKIDASHI_LARGE_BLACK_EDGE:
            this->char_width = 0xD;
            this->char_height = 0x14;
            this->fukidashi = false;
            this->stay_frame = false;
            this->clut_default = 1;
            this->clut_now = this->clut_default;
            this->page_arrow = true;
            this->style = MES_EDGE_BLACK;
            this->page_mark_style = true;
            break;
        case MES_FUKIDASHI_LARGE_TABLE_EDGE:
            this->char_width = 0xD;
            this->char_height = 0x14;
            this->fukidashi = false;
            this->stay_frame = false;
            this->clut_default = Color2Clut(FONT_COLOR_WHITE) & 0xFF;
            this->clut_now = this->clut_default;
            this->page_arrow = true;
            this->style = MES_EDGE_TABLE;
            this->page_mark_style = true;
            break;
        case MES_FUKIDASHI_SMALL_FRAMED:
            this->char_width = 0xB;
            this->char_height = 0x14;
            this->fukidashi = false;
            this->stay_frame = true;
            this->clut_default = Color2Clut(FONT_COLOR_WHITE) & 0xFF;
            this->clut_now = this->clut_default;
            this->page_arrow = false;
            this->style = MES_EDGE_BLACK;
            this->page_mark_style = false;
            break;
        case MES_FUKIDASHI_SMALL_DOUBLE_EDGE:
            this->char_width = 0xB;
            this->char_height = 0x14;
            this->fukidashi = false;
            this->stay_frame = false;
            this->clut_default = Color2Clut(FONT_COLOR_WHITE) & 0xFF;
            this->clut_now = this->clut_default;
            this->page_arrow = true;
            this->style = MES_EDGE_DOUBLE;
            this->page_mark_style = true;
            break;
        case MES_FUKIDASHI_NO_BUBBLE:
            this->fukidashi = false;
            this->page_arrow = false;
            break;
        default:
            this->char_width = 0xB;
            this->char_height = 0x14;
            this->fukidashi = true;
            this->stay_frame = false;
            this->clut_default = 1;
            this->clut_now = this->clut_default;
            this->page_arrow = true;
            this->style = MES_EDGE_NONE;
            this->page_mark_style = true;
            break;
    }
}

void ClsMes::Step() {
    if (this->fukidashi == 0) {
        this->fade = 1.0f;
        return;
    }

    if (this->fade_in != 0) {
        if (this->fade < 1.0f) {
            this->fade = this->fade + this->fade_speed;
        }

        if (1.0f < this->fade) {
            this->fade = 1.0f;
        }
    } else {
        if (0.0f < this->fade) {
            this->fade = this->fade - this->fade_speed;
        }

        if (this->fade < 0.0f) {
            this->fade = 0.0f;
        }
    }

    if (this->tail_on == 0) {
        return;
    }

    float centre[2];
    float left[2];
    float left_out[2];
    float right[2];
    float right_out[2];

    centre[0] = this->tail_x;
    centre[1] = this->tail_y;

    float angle = atan2((float) (this->tail_to_x - this->tail_x), (float) (this->tail_to_y - this->tail_y));

    left[0] = this->tail_x - this->tail_half_width;
    left[1] = this->tail_y;
    RollPos(centre, left, angle, left_out);
    this->tail_left_x = (int) left_out[0];
    this->tail_left_y = (int) left_out[1];

    right[0] = this->tail_x + this->tail_half_width;
    right[1] = this->tail_y;
    RollPos(centre, right, angle, right_out);
    this->tail_right_x = (int) right_out[0];
    this->tail_right_y = (int) right_out[1];

    float dx = this->tail_to_x - this->tail_x;
    float dy = this->tail_to_y - this->tail_y;
    float length = sqrt(dx * dx + dy * dy);

    float px = this->tail_length * dx / length;
    float py = this->tail_length * dy / length;

    this->tail_tip_x = (int) px + this->tail_x;
    this->tail_tip_y = (int) py + this->tail_y;
}

int ClsMes::State() {
    if (this->fade <= 0.0f) {
        return CLSMES_CLOSED;
    }

    if (0.0f < this->fade && this->fade < 1.0f) {
        return this->fade_in != 0 ? CLSMES_FADE_IN : CLSMES_FADE_OUT;
    }

    if (this->waiting != 0) {
        return CLSMES_PAGE_WAIT;
    }

    if (this->text_no >= this->text_len) {
        return CLSMES_SHOWN;
    }

    return CLSMES_REVEALING;
}

void ClsMes::MyTextureMake_InitAll() {
    if (this->tex_buff != NULL) {
        memset(this->tex_buff, 0, 0x100);
    }
}

void ClsMes::GoNextPage() {
    if (this->waiting != 0) {
        this->waiting = false;
        MyTextureMake_InitAll();
        this->blink = 0;
        this->text_from = this->page_from;
    }
}

int ClsMes::MyTextureMake_sub() {
    int at = this->text_no;

    this->text_no = at + 1;

    short code = this->win_line[at].code;

    int unused;

    if (code >= 0x1000) {
        unused = code;
    }

    switch (code) {
        case MES_CODE_NEWLINE:
            this->text_at += 1.0f;
            return 0;
        case MES_CODE_PAGE:
            this->waiting = true;

            if (this->auto_page != 0 && this->blink >= this->auto_page_wait) {
                this->GoNextPage();
                this->auto_page_wait = 0;
            }

            this->page_from = at;
            return 1;
        case MES_CODE_END:
            this->text_at += 1.0f;
            return 2;
        case MES_CODE_SPACE:
            this->text_at += 1.0f;
            return 0;
    }

    if ((code >= -0x700 && code <= -0x601) || (code >= -0x800 && code <= -0x701) || (code >= -0x900 && code < -0x800)) {
        this->text_at += 1.0f;
        return 0;
    }

    this->wait = this->win_line[at].space;
    return 0;
}

void ClsMes::MyTextureMake() {
    if (this->waiting != 0) {
        if (this->auto_page != 0 && this->blink >= this->auto_page_wait) {
            this->GoNextPage();
            this->auto_page_wait = 0;
        }

        return;
    }

    if (this->wait > 0) {
        this->wait--;
    } else if (this->text_rate > 0.0f) {
        this->text_at += this->text_rate;
    }

    if (this->text_rate_set == 0.0f && this->text_no >= this->text_len) {
        return;
    }

    if (this->text_at > this->text_len) {
        this->text_at = this->text_len;
    }

    while (this->text_rate == 0.0f || this->text_at - this->text_no >= 1.0f) {
        int made = this->MyTextureMake_sub();

        if (made == 1 || made == 2) {
            if (this->text_rate == 0.0f) {
                this->text_rate = this->text_rate_set;
                this->text_at = this->text_no;
            }

            return;
        }

        if (this->text_no >= this->text_len) {
            this->text_rate = this->text_rate_set;
            this->text_at = this->text_no;
            return;
        }

        if (this->text_rate_set == 0.0f && this->text_no >= this->text_len) {
            return;
        }
    }
}

short *SetAndGetNameRegistTbl(int chara) {
    if (chara < 0) {
        return NULL;
    }

    if (chara >= 6) {
        return NULL;
    }

    for (int i = 0; i < 11; i++) {
        NameRegistTbl[chara][i] = MES_CODE_NEWLINE;
    }

    u16 *name = (u16 *) SaveData->GetCharaName(chara);

    for (int i = 0; i < 11; i++) {
        if (name[i] == 0) {
            NameRegistTbl[chara][i] = MES_CODE_NEWLINE;
        } else {
            NameRegistTbl[chara][i] = NameRegistCodeJtoE(name[i] - 1);
        }
    }

    return NameRegistTbl[chara];
}

void ClsMes::MakeMesWinTbl_value(int *x, int *y) {
    if (this->value_show == 0 && this->value == 0) {
        return;
    }

    char text[128];

    if (this->value_signed != 0 && this->value > 0) {
        sprintf(text, "+%d\n", this->value);
    } else {
        sprintf(text, "%d\n", this->value);
    }

    int len = strlen(text);

    for (int i = 0; i < len; i++) {
        int  code = -1;
        char digit = text[i];

        if (digit == '+') {
            code = -0x2A4;
        }

        if (digit == '-') {
            code = -0x2A3;
        }

        if (digit == '1') {
            code = -0x290;
        }

        if (digit == '2') {
            code = -0x28F;
        }

        if (digit == '3') {
            code = -0x28E;
        }

        if (digit == '4') {
            code = -0x28D;
        }

        if (digit == '5') {
            code = -0x28C;
        }

        if (digit == '6') {
            code = -0x28B;
        }

        if (digit == '7') {
            code = -0x28A;
        }

        if (digit == '8') {
            code = -0x289;
        }

        if (digit == '9') {
            code = -0x288;
        }

        if (digit == '0') {
            code = -0x291;
        }

        if (code == -1) {
            continue;
        }

        if (this->value_narrow != 0) {
            this->SetMesWinTbl(code, 0, *x, *y);
            *x = *x + (this->char_width >> 2) * 3;
        } else {
            this->SetMesWinTbl(code, 0, *x, *y);
            *x = *x + this->char_width;
        }
    }
}

void ClsMes::MakeMesWinTbl_value(int which, int *x, int *y) {
    if (this->value_show == 0 && this->values[which] == 0) {
        return;
    }

    char text[128];

    if (this->value_signed != 0 && this->values[which] > 0) {
        sprintf(text, "+%d\n", this->values[which]);
    } else {
        sprintf(text, "%d\n", this->values[which]);
    }

    int len = strlen(text);

    for (int i = 0; i < len; i++) {
        int  code = -1;
        char digit = text[i];

        if (digit == '+') {
            code = -0x2A4;
        }

        if (digit == '-') {
            code = -0x2A3;
        }

        if (digit == '1') {
            code = -0x290;
        }

        if (digit == '2') {
            code = -0x28F;
        }

        if (digit == '3') {
            code = -0x28E;
        }

        if (digit == '4') {
            code = -0x28D;
        }

        if (digit == '5') {
            code = -0x28C;
        }

        if (digit == '6') {
            code = -0x28B;
        }

        if (digit == '7') {
            code = -0x28A;
        }

        if (digit == '8') {
            code = -0x289;
        }

        if (digit == '9') {
            code = -0x288;
        }

        if (digit == '0') {
            code = -0x291;
        }

        if (code == -1) {
            continue;
        }

        if (this->value_narrow != 0) {
            this->SetMesWinTbl(code, 0, *x, *y);
            *x = *x + (this->char_width >> 2) * 3;
        } else {
            this->SetMesWinTbl(code, 0, *x, *y);
            *x = *x + this->char_width;
        }
    }
}

int ClsMes::MakeMesWinTbl_system(int code, int *x, int *y) {
    short *at;
    int    text_code;

    if (code >= -0x500 && code < -0x400) {
        code += 0x500;

        if (code == 0xFF) {
            return 0;
        }

        if (code == 0xFA) {
            return 0;
        }

        if (code == 0xF9) {
            return 0;
        }

        if (code == 0xF8) {
            return 0;
        }

        if (code == 0xF7) {
            return 0;
        }

        if (code == 0xF6) {
            return 0;
        }

        if (code == 0xF5) {
            return 0;
        }

        if (code == 0xF4) {
            return 0;
        }

        if (code == 0xF3) {
            return 0;
        }

        if (code == 0xFE) {
            code = this->mes_no[0];
        } else if (code == 0xFD) {
            code = this->mes_no[1];
        } else if (code == 0xFC) {
            code = this->mes_no[2];
        } else if (code == 0xFB) {
            code = this->mes_no[3];
        } else if (code == 0xF2) {
            code = this->mes_no[4];
        } else if (code == 0xF1) {
            code = this->mes_no[5];
        } else if (code == 0xF0) {
            code = this->mes_no[6];
        } else if (code == 0xEF) {
            code = this->mes_no[7];
        } else if (code == 0xEE) {
            code = this->mes_no[8];
        } else if (code == 0xED) {
            code = this->mes_no[9];
        }
    } else {
        return 0;
    }

    if (this->buff_system == 0) {
        return 0;
    }

    at = this->GetTextLineDataTop_system(code);

    if (at == 0) {
        return 0;
    }

    while (1) {
        text_code = *at;
        at++;

        if (text_code >= -0x506 && text_code < -0x500) {
            short *name = SetAndGetNameRegistTbl(text_code + 0x506);

            if (name != 0) {
                int name_code = *name;

                while (name_code != MES_CODE_NEWLINE && name_code != MES_CODE_END) {
                    this->SetMesWinTbl(name_code, 0, *x, *y);
                    *x += this->char_width;
                    name++;
                    name_code = *name;
                }
            }
        } else {
            switch (text_code) {
                case MES_CODE_END:
                    return 1;

                case MES_CODE_SPACE:
                    this->SetMesWinTbl(text_code, 0, *x, *y);

                    if (this->space_area >= 0 || this->space_width >= 0) {
                        *x += this->space_width;
                    } else {
                        *x += this->char_width;
                    }

                    break;

                case MES_CODE_NEWLINE:
                    this->SetMesWinTbl(text_code, 1, *x, *y);
                    *x = 0;
                    *y += this->char_height;
                    break;

                default:
                    if (text_code >= -0x300 && text_code < -0x250) {
                        this->SetMesWinTbl(text_code, 0, *x, *y);
                        *x += this->GetGaijiW(text_code);
                    } else {
                        this->SetMesWinTbl(text_code, 1, *x, *y);
                        *x += this->char_width;
                    }

                    break;
            }
        }
    }
}

int ClsMes::GetMesLen_system(int mes_no) {
    int no;

    if (mes_no >= -0x500 && mes_no < -0x400) {
        no = mes_no + 0x500;

        if (no == 0xFF) {
            return -1;
        }

        if (no == 0xFA) {
            return -1;
        }

        if (no == 0xF9) {
            return -1;
        }

        if (no == 0xF8) {
            return -1;
        }

        if (no == 0xF7) {
            return -1;
        }

        if (no == 0xF6) {
            return -1;
        }

        if (no == 0xF5) {
            return -1;
        }

        if (no == 0xF4) {
            return -1;
        }

        if (no == 0xF3) {
            return -1;
        }

        if (no == 0xFE) {
            no = this->mes_no[0];
        } else if (no == 0xFD) {
            no = this->mes_no[1];
        } else if (no == 0xFC) {
            no = this->mes_no[2];
        } else if (no == 0xFB) {
            no = this->mes_no[3];
        } else if (no == 0xF2) {
            no = this->mes_no[4];
        } else if (no == 0xF1) {
            no = this->mes_no[5];
        } else if (no == 0xF0) {
            no = this->mes_no[6];
        } else if (no == 0xEF) {
            no = this->mes_no[7];
        } else if (no == 0xEE) {
            no = this->mes_no[8];
        } else if (no == 0xED) {
            no = this->mes_no[9];
        }
    } else {
        return -1;
    }

    if (this->buff_system == NULL) {
        return -1;
    }

    short *at = this->GetTextLineDataTop_system(no);

    if (at == NULL) {
        return -1;
    }

    int len[2] = {0, 0};
    int line = 0;

    while (1) {
        short code = *at++;

        if (code >= -0x506 && code < -0x500) {
            len[line] += this->GetNameLen(code + 0x506);
            continue;
        }

        switch (code) {
            case MES_CODE_NEWLINE:
                line++;
                continue;
            case MES_CODE_END:
                if (len[0] > len[1]) {
                    return len[0];
                }

                return len[1];
        }

        len[line]++;
    }
}

int ClsMes::GetMesWidth_system(int mes_no) {
    if (this->buff_system == NULL) {
        return -1;
    }

    short *at = this->GetTextLineDataTop_system(mes_no);

    if (at == NULL) {
        return -1;
    }

    int width[2] = {0, 0};
    int line = 0;

    while (1) {
        short code = *at++;

        if (code >= -0x506 && code < -0x500) {
            width[line] += this->GetNameWidth(code + 0x506);
            continue;
        }

        switch (code) {
            case MES_CODE_NEWLINE:
                line++;
                continue;
            case MES_CODE_END:
                if (width[0] > width[1]) {
                    return width[0];
                }

                return width[1];
        }

        if (code >= -0x300 && code < -0x250) {
            width[line] += this->GetGaijiW(code);
        } else {
            width[line] += this->char_width;
        }
    }
}

short *ClsMes::GetTextLineDataTop_system(int line) {
    int    count = this->buff_system[0];
    short *body = &this->buff_system[1];

    for (int i = 0; i < count; i++) {
        if (line == body[i * 2 + 1]) {
            return &body[count] + body[i * 2 + 2];
        }
    }

    return NULL;
}

void ClsMes::InitMesWinTbl() {
    for (int i = 0; i < MES_WIN_LINE_MAX; i++) {
        this->win_line[i].code = 0;
        this->win_line[i].x = 0;
        this->win_line[i].y = 0;
        this->win_line[i].clut = 0;
        this->win_line[i].space = 0;
    }

    this->win_line_num = 0;
}

int ClsMes::CalcSpaceW(int width, int glyph_width, short *text) {
    short *at = text;
    int    used = 0;
    int    spaces = 0;

    while (1) {
        short code = *at++;

        switch (code) {
            case MES_CODE_NEWLINE:
            case MES_CODE_PAGE:
            case MES_CODE_END:
                if (spaces > 0) {
                    return (width - used) / spaces;
                }

                return -1;
            case MES_CODE_SPACE:
                spaces++;
                continue;
        }

        if (code >= -0x300 && code < -0x250) {
            used += this->GetGaijiW(code);
        } else if (code >= -0x700 && code < -0x600) {
            used += code + 0x700;
        } else {
            used += glyph_width;
        }
    }
}

int ClsMes::MakeMesWinTbl(int mes_no) {
    short *at;
    int    x;
    int    y;
    int    code;

    if (this->buff == 0) {
        return 0;
    }

    at = this->GetTextLineDataTop(mes_no);

    if (at == 0) {
        return 0;
    }

    this->InitMesWinTbl();

    x = 0;
    y = 0;

    while (1) {
        code = *at;
        at++;

        if (code == MES_CODE_PAGE || code == MES_CODE_NEWLINE || code == MES_CODE_END) {
            this->space_width = -1;
            this->space_area = -1;
        }

        switch (code) {
            case MES_CODE_END:
                this->SetMesWinTbl(code, 0, x, y);
                return 1;

            case MES_CODE_NEWLINE:
                this->SetMesWinTbl(code, 0, x, y);
                x = 0;
                y += this->char_height;
                break;

            case MES_CODE_PAGE:
                this->SetMesWinTbl(code, 0, x, y);
                x = 0;
                y = 0;
                break;

            case MES_CODE_SPACE:
                this->SetMesWinTbl(code, 0, x, y);

                if (this->space_area >= 0 || this->space_width >= 0) {
                    x += this->space_width;
                } else {
                    x += this->char_width;
                }

                break;

            default:
                if (code >= -0x300 && code < -0x250) {
                    this->SetMesWinTbl(code, 0, x, y);
                    x += this->GetGaijiW(code);
                } else if (code >= -0x900 && code < -0x800) {
                    this->space_area = (code + 0x900) * 4;
                    this->space_width = this->CalcSpaceW(this->space_area, this->char_width, at - 1);
                } else if (code >= -0x800 && code < -0x700) {
                    this->space_width = code + 0x800;
                } else if (code >= -0x700 && code < -0x600) {
                    x += code + 0x700;
                } else if (code >= -0x506 && code < -0x500) {
                    short *name = SetAndGetNameRegistTbl(code + 0x506);

                    if (name != 0) {
                        int name_code = *name;

                        while (name_code != MES_CODE_NEWLINE && name_code != MES_CODE_END) {
                            this->SetMesWinTbl(name_code, 0, x, y);
                            x += this->char_width;
                            name++;
                            name_code = *name;
                        }
                    }
                } else if (code == -0x401) {
                    this->MakeMesWinTbl_value(&x, &y);
                } else if (code >= -0x40D && code < -0x405) {
                    this->MakeMesWinTbl_value(-0x406 - code, &x, &y);
                } else if (this->MakeMesWinTbl_system(code, &x, &y) == 0) {
                    if (this->SetMesWinTbl(code, 0, x, y) != 0) {
                        x += this->char_width;
                    }
                }

                break;
        }
    }
}

void NeedMesWinWH_sub(int *len, int *max_len, int *width, int *max_width, int char_width, int count) {
    if (count <= 0) {
        return;
    }

    *len += count;

    if (*max_len < *len) {
        *max_len = *len;
    }

    *width += count * char_width;

    if (*max_width < *width) {
        *max_width = *width;
    }
}

void ClsMes::NeedMesWinWH(int mes_no, int *out) {
    short *at;
    int   *max_len;
    int   *max_lines;
    int   *max_width;
    int   *max_height;
    int    code;
    int    len;
    int    width;
    int    lines;
    int    height;
    int    count;

    if (this->buff == 0) {
        return;
    }

    at = this->GetTextLineDataTop(mes_no);

    if (at == 0) {
        return;
    }

    max_len = &out[0];
    max_lines = &out[1];
    max_width = &out[2];
    max_height = &out[3];

    *max_len = 0;
    *max_lines = 0;
    *max_width = 0;
    *max_height = 0;
    len = 0;
    lines = 0;
    width = 0;
    height = 0;
    count = 0;

    while (1) {
        count++;
        code = *at;
        at++;

        if (code == MES_CODE_PAGE || code == MES_CODE_NEWLINE || code == MES_CODE_END) {
            this->space_width = -1;
            this->space_area = -1;
        }

        switch (code) {
            case MES_CODE_END:
                return;

            case MES_CODE_SPACE:
                len++;

                if (*max_len < len) {
                    *max_len = len;
                }

                if (this->space_area >= 0 || this->space_width >= 0) {
                    width += this->space_width;
                } else {
                    width += this->char_width;
                }

                if (*max_width < width) {
                    *max_width = width;
                }

                break;

            case MES_CODE_NEWLINE:
                len = 0;
                lines++;

                if (*max_lines < lines) {
                    *max_lines = lines;
                }

                width = 0;
                height += this->char_height;

                if (*max_height < height) {
                    *max_height = height;
                }

                break;

            case MES_CODE_PAGE:
                len = 0;
                lines = 0;
                width = 0;
                height = 0;
                break;

            default:
                if (code >= -0x200 && code <= -0x101) {
                    continue;
                } else if (code >= -0x400 && code <= -0x301) {
                    continue;
                } else if (code >= -0xA00 && code <= -0x901) {
                    continue;
                } else if (code >= -0x900 && code < -0x800) {
                    this->space_area = (code + 0x900) * 4;
                    this->space_width = this->CalcSpaceW(this->space_area, this->char_width, at - 1);
                } else if (code >= -0x800 && code < -0x700) {
                    this->space_width = code + 0x800;
                } else if (code >= -0x700 && code < -0x600) {
                    len++;

                    if (*max_len < len) {
                        *max_len = len;
                    }

                    width += code + 0x700;

                    if (*max_width < width) {
                        *max_width = width;
                    }
                } else if (code == -0x401) {
                    if (this->value_show != 0 || this->value != 0) {
                        char text[128];

                        if (this->value_signed != 0 && this->value > 0) {
                            sprintf(text, "+%d\n", this->value);
                        } else {
                            sprintf(text, "%d\n", this->value);
                        }

                        if (this->value_narrow != 0) {
                            NeedMesWinWH_sub(&len, max_len, &width, max_width, (this->char_width >> 2) * 3, strlen(text) - 1);
                        } else {
                            NeedMesWinWH_sub(&len, max_len, &width, max_width, this->char_width, strlen(text) - 1);
                        }
                    }
                } else if (code >= -0x40D && code < -0x405) {
                    if (this->value_show != 0 || this->values[-0x406 - code] != 0) {
                        char text[128];

                        if (this->value_signed != 0 && this->values[-0x406 - code] > 0) {
                            sprintf(text, "+%d\n", this->values[-0x406 - code]);
                        } else {
                            sprintf(text, "%d\n", this->values[-0x406 - code]);
                        }

                        if (this->value_narrow != 0) {
                            NeedMesWinWH_sub(&len, max_len, &width, max_width, (this->char_width >> 2) * 3, strlen(text) - 1);
                        } else {
                            NeedMesWinWH_sub(&len, max_len, &width, max_width, this->char_width, strlen(text) - 1);
                        }
                    }
                } else if (code == -0x405 || code == -0x404 || code == -0x403 || code == -0x402 || code == -0x411 || code == -0x410 || code == -0x40F || code == -0x40E) {
                    NeedMesWinWH_sub(&len, max_len, &width, max_width, this->char_width, this->GetMesLen_system(code));
                } else if (code >= -0x506 && code < -0x500) {
                    NeedMesWinWH_sub(&len, max_len, &width, max_width, this->char_width, this->GetNameLen(code + 0x506));
                } else if (code >= -0x300 && code < -0x251) {
                    len += *(int *) &EditGaijiTbl[code];
                    width += this->GetGaijiW(code);

                    if (*max_len < len) {
                        *max_len = len;
                    }

                    if (*max_width < width) {
                        *max_width = width;
                    }
                } else {
                    NeedMesWinWH_sub(&len, max_len, &width, max_width, this->char_width, 1);
                }

                break;
        }
    }
}

void MakeRandTbl(float lo, float hi);

void MakeRandTbl2(float lo, float hi);

int ClsMes::MakeMesWin(int mes_no) {
    if (this->mes_made == mes_no) {
        if (this->auto_page == 0) {
            this->fade = 1.0f;
            this->fade_in = true;
            GoNextPage();
        }

        return 0;
    }

    this->fukidashi_shape = 0;
    this->text_no = 0;
    this->text_from = 0;
    this->page_from = 0;
    this->text_at = 0.0f;
    MakeMesTexture(mes_no);

    if (this->fukidashi_shape == 0) {
        MakeRandTbl(0.0f, 4.0f);
    } else if (this->fukidashi_shape == 1) {
        MakeRandTbl(2.0f, 6.0f);
    } else if (this->fukidashi_shape == 4) {
        MakeRandTbl(6.8f, 15.2f);
        MakeRandTbl2(0.42f, 0.58f);
    }

    return 0;
}

void ClsMes::MakeMesTexture(int mes_no) {
    int needed[4];

    this->mes_made = mes_no;

    if (this->fukidashi != 0) {
        this->fade = 0.0f;
        this->fade_in = true;
    }

    this->waiting = false;
    this->blink = 0;
    MyTextureMake_InitAll();
    NeedMesWinWH(mes_no, needed);

    needed[1] += 1;
    needed[3] += this->char_height;
    this->text_columns = needed[0];
    this->text_rows = needed[1];
    this->text_width = needed[2];
    this->text_height = needed[3];

    if (this->text_columns < 4) {
        this->win_width = this->char_width * 10;
    } else {
        this->win_width = this->char_width * (int) (3.0f + (3.0f + (float) this->text_columns));
    }

    this->win_height = this->char_height * (int) (1.5f + (1.5f + (float) this->text_rows));
    this->win_x = this->text_x - (int) (3.0f * (float) this->char_width);
    this->win_y = this->text_y - (int) (1.5f * (float) this->char_height);

    if (MakeMesWinTbl(mes_no) != 0) {
        this->text_len = this->win_line_num;
    }
}

void ClsMes::Myset2DSprite_Fuchi(sceVif1Packet *packet, CTexture *texture, int x, int y, int width, int height, int u, int v, int u_width, int v_height) {
    switch (this->style) {
        case MES_EDGE_WHITE:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0xFF, 0xFF, 0xFF, this->edge_alpha < 0x40 ? this->edge_alpha : 0x40);
            break;
        case MES_EDGE_BLACK:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0, 0, 0, this->edge_alpha < 0x40 ? this->edge_alpha : 0x40);
            break;
        case MES_EDGE_TABLE:
            for (int i = 0; FuchiTbl_E[i].alpha > 0; i++) {
                set2DSprite_Core(packet, texture, CRect_i_(x + FuchiTbl_E[i].x, y + FuchiTbl_E[i].y, width, height), CRect_i_(u, v, u_width, v_height), FuchiTbl_E[i].r, FuchiTbl_E[i].g, FuchiTbl_E[i].b, this->edge_alpha < FuchiTbl_E[i].alpha ? this->edge_alpha : FuchiTbl_E[i].alpha);
            }

            break;
        case MES_EDGE_DOUBLE:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0x40, 0x40, 0x40, this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
            set2DSprite_Core(packet, texture, CRect_i_(x + 2, y + 2, width, height), CRect_i_(u, v, u_width, v_height), 0, 0, 0, this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
            break;
    }
}

void DrawMaru(sceVif1Packet *packet, int x, int y, int width, int height, int lift, int unused, int rough, int prim);

#pragma opt_propagation off

#pragma opt_propagation reset

void SetspRGBA(spRGBA *colour, int r, int g, int b, int a) {
    colour->r = r;
    colour->g = g;
    colour->b = b;
    colour->a = a;
}

void ClsMes::DrawMesWin_sub(CTexture *texture, int dx, int dy, int frame) {
    int area[4];
    int pos[2];
    int tex_x;
    int tex_y;
    int screen_y;
    int draw_width;
    int draw_height;
    int screen_x;
    int top;

    area[0] = this->win_x;
    area[1] = this->win_y;
    area[2] = this->win_x + this->win_width;
    area[3] = this->win_y + this->win_height;
    pos[0] = this->grow_x;
    pos[1] = this->grow_y;
    AreaAddPos(area, pos, area);

    if (this->tail_on != 0) {
        pos[0] = this->tail_tip_x;
        pos[1] = this->tail_tip_y;
        AreaAddPos(area, pos, area);
        pos[0] = this->tail_left_x;
        pos[1] = this->tail_left_y;
        AreaAddPos(area, pos, area);
        pos[0] = this->tail_right_x;
        pos[1] = this->tail_right_y;
        AreaAddPos(area, pos, area);

        if (this->fukidashi_shape == 2) {
            pos[0] = this->tail_tip_x - 5;
            pos[1] = this->tail_tip_y - 5;
            AreaAddPos(area, pos, area);
            pos[0] = this->tail_tip_x + 5;
            pos[1] = this->tail_tip_y + 5;
            AreaAddPos(area, pos, area);
        }
    }

    int left = area[0];
    top = area[1];
    int right = area[2];
    int bottom = area[3];
    draw_width = right - left;
    int height = bottom - top;
    tex_x = (int) LinerInterpolation(this->grow_x, left, this->fade);
    tex_y = (int) LinerInterpolation(this->grow_y, top, this->fade);
    draw_width = (int) (draw_width * this->fade);
    draw_height = (int) (height * this->fade);
    screen_y = tex_y;
    tex_y -= top;
    screen_x = tex_x + dx;
    screen_y += dy;

    if (frame != 0) {
        spRGBA bright;
        spRGBA dim;
        int    outer_x;
        int    core_x;
        int    outer_y;
        int    outer_w;
        int    outer_h;
        int    core_y;
        int    core_w;
        int    core_h;
        int    core_screen_y;
        int    upper_screen_y;
        int    middle_screen_y;
        int    lower_screen_y;
        int    upper_h;
        int    lower_h;
        int    lower_tex_y;
        int    upper_tex_h;
        int    lower_tex_h;
        int    right_x;
        int    left_w;
        int    right_w;

        SetspRGBA(&bright, 0x80, 0x80, 0x80, 0x80);
        SetspRGBA(&dim, 0x60, 0x60, 0x60, 0x80);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y, draw_width, draw_height), CRect_i_(tex_x, tex_y, draw_width, draw_height), &dim, &dim, &dim, &dim, 1);
        }

        float win_left = this->win_x;
        float win_top = this->win_y;
        float win_w = this->win_width;
        float win_h = this->win_height;

        outer_x = (int) LinerInterpolation(this->grow_x, win_left, this->fade);
        outer_y = (int) LinerInterpolation(this->grow_y, win_top, this->fade);
        outer_w = (int) (win_w * this->fade);
        outer_h = (int) (win_h * this->fade);

        float core_left = this->win_x + this->win_width / 6.0f;
        float core_top = this->win_y + this->win_height / 6.0f;
        float core_width = 2.0f * this->win_width / 3.0f;
        float core_height = 2.0f * this->win_height / 3.0f;

        core_x = (int) LinerInterpolation(this->grow_x, core_left, this->fade);
        core_y = (int) LinerInterpolation(this->grow_y, core_top, this->fade);
        core_w = (int) (core_width * this->fade);
        core_h = (int) (core_height * this->fade);

        right_x = core_x + core_w;
        upper_screen_y = outer_y;
        core_screen_y = core_y;
        lower_screen_y = core_y + core_h;
        left_w = core_x - outer_x;
        right_w = outer_x + outer_w - core_x - core_w;
        upper_h = core_y - outer_y;
        lower_h = outer_y + outer_h - core_y - core_h;
        outer_y -= top;
        core_y -= top;
        middle_screen_y = core_screen_y;
        lower_tex_y = core_y + core_h;
        upper_tex_h = core_y - outer_y;
        lower_tex_h = outer_y + outer_h - core_y - core_h;

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, CRect_i_(outer_x, upper_screen_y, left_w, upper_h), CRect_i_(outer_x, outer_y, left_w, upper_tex_h), &dim, &dim, &dim, &bright, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(core_x, upper_screen_y, core_w, upper_h), CRect_i_(core_x, outer_y, core_w, upper_tex_h), &dim, &dim, &bright, &bright, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(right_x, upper_screen_y, right_w, upper_h), CRect_i_(right_x, outer_y, right_w, upper_tex_h), &dim, &dim, &bright, &dim, 0);
            set2DSprite(Vif1Packet, texture, CRect_i_(outer_x, middle_screen_y, left_w, core_h), CRect_i_(outer_x, core_y, left_w, core_h), &dim, &bright, &dim, &bright, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(core_x, middle_screen_y, core_w, core_h), CRect_i_(core_x, core_y, core_w, core_h), &bright, &bright, &bright, &bright, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(right_x, middle_screen_y, right_w, core_h), CRect_i_(right_x, core_y, right_w, core_h), &bright, &dim, &bright, &dim, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(outer_x, lower_screen_y, left_w, lower_h), CRect_i_(outer_x, lower_tex_y, left_w, lower_tex_h), &dim, &bright, &dim, &dim, 0);
            set2DSprite(Vif1Packet, texture, CRect_i_(core_x, lower_screen_y, core_w, lower_h), CRect_i_(core_x, lower_tex_y, core_w, lower_tex_h), &bright, &bright, &dim, &dim, 1);
            set2DSprite(Vif1Packet, texture, CRect_i_(right_x, lower_screen_y, right_w, lower_h), CRect_i_(right_x, lower_tex_y, right_w, lower_tex_h), &bright, &dim, &dim, &dim, 1);
        }
    } else if (MesAbsDrawOff == 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y, draw_width, draw_height), CRect_i_(tex_x, tex_y, draw_width, draw_height), 0, 0, 0, this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
    }
}

/**
 * Draws a help window from nine texture pieces, stretching the edges and the
 * middle to the requested size.
 *
 * @mangled MyMenuHelpWinDraw__FiiiiiiiP8CTexture
 * @address 0x152930
 * @size 0x380
 */
void MyMenuHelpWinDraw(int x, int y, int width, int height, int shade, int u, int v, CTexture *texture) {
    int bar_width = width < 0x20 ? 0 : width - 0x20;
    int bar_height = height < 0x1C ? 0 : height - 0x1C;
    int alpha;

    setbilinear(0);

    set2DSprite(Vif1Packet, texture, CRect_i_(x, y, 0x10, 0xE), CRect_i_(u, v, 0x10, 0xE), alpha = (shade * 100) >> 7);
    set2DSprite(Vif1Packet, texture, CRect_i_(x, y + 0xE, 0x10, bar_height), CRect_i_(u, v + 0xE, 0x10, 0x24), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(x, bar_height + (y + 0xE), 0x10, 0xE), CRect_i_(u, v + 0x32, 0x10, 0xE), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x10, y, bar_width, 0xE), CRect_i_(u + 0x10, v, 0x20, 0xE), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x10, y + 0xE, bar_width, bar_height), CRect_i_(u + 0x10, v + 0xE, 0x20, 0x24), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x10, bar_height + (y + 0xE), bar_width, 0xE), CRect_i_(u + 0x10, v + 0x32, 0x20, 0xE), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(bar_width + (x + 0x10), y, 0x10, 0xE), CRect_i_(u + 0x30, v, 0x10, 0xE), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(bar_width + (x + 0x10), y + 0xE, 0x10, bar_height), CRect_i_(u + 0x30, v + 0xE, 0x10, 0x24), alpha);
    set2DSprite(Vif1Packet, texture, CRect_i_(bar_width + (x + 0x10), bar_height + (y + 0xE), 0x10, 0xE), CRect_i_(u + 0x30, v + 0x32, 0x10, 0xE), alpha);
}

void GetPos_AbsPosSet(int x, int y, int width, int height, int win_width, int win_height, int align, int *out_x, int *out_y) {
    switch (align) {
        case MES_POS_TOP_LEFT:
        case MES_POS_LEFT:
        case MES_POS_BOTTOM_LEFT:
            if (win_width < width / 3) {
                *out_x = x + width / 6 - (win_width >> 1);
            } else {
                *out_x = x;
            }

            break;
        case MES_POS_TOP:
        case MES_POS_CENTRE:
        case MES_POS_BOTTOM:
            *out_x = x + (width >> 1) - (win_width >> 1);
            break;
        case MES_POS_TOP_RIGHT:
        case MES_POS_RIGHT:
        case MES_POS_BOTTOM_RIGHT:
            if (win_width < width / 3) {
                *out_x = x + width * 5 / 6 - (win_width >> 1);
            } else {
                *out_x = x + width - win_width;
            }

            break;
    }

    switch (align) {
        case MES_POS_TOP_LEFT:
        case MES_POS_TOP:
        case MES_POS_TOP_RIGHT:
            if (win_height < height / 3) {
                *out_y = y + height / 6 - (win_height >> 1);
            } else {
                *out_y = y;
            }

            break;
        case MES_POS_LEFT:
        case MES_POS_CENTRE:
        case MES_POS_RIGHT:
            *out_y = y + (height >> 1) - (win_height >> 1);
            break;
        case MES_POS_BOTTOM_LEFT:
        case MES_POS_BOTTOM:
        case MES_POS_BOTTOM_RIGHT:
            if (win_height < height / 3) {
                *out_y = y + height * 5 / 6 - (win_height >> 1);
            } else {
                *out_y = y + height - win_height;
            }

            break;
    }
}

