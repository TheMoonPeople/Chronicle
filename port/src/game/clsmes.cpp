#include "clsmes.hpp"

#include <libpkt.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <string>

#include "character.hpp"
#include "editpartsdata.hpp"
#include "editpartsinfo.hpp"
#include "gameutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

void MakeRandTbl(float lo, float hi);
void MakeRandTbl2(float lo, float hi);

float RandTbl[64];
float RandTbl2[64];

// The entry with an alpha of zero or less ends the table.
MES_FUCHI FuchiTbl_E[] = {
    {.x = 0,  .y = -2, .r = 0, .g = 0, .b = 0, .alpha = 128},
    {.x = 0,  .y = 2,  .r = 0, .g = 0, .b = 0, .alpha = 128},
    {.x = 2,  .y = 0,  .r = 0, .g = 0, .b = 0, .alpha = 128},
    {.x = -2, .y = 0,  .r = 0, .g = 0, .b = 0, .alpha = 128},
    {.x = 0,  .y = 0,  .r = 0, .g = 0, .b = 0, .alpha = -1 },
};

namespace {

s16 NameRegistTbl[8][11];

/**
 * Gives back the gaiji code of a value digit or sign, or -1 for any other
 * character.
 */
int ValueGlyphCode(char glyph) {
    if (glyph == '+') {
        return -0x2A4;
    }

    if (glyph == '-') {
        return -0x2A3;
    }

    if (glyph >= '0' && glyph <= '9') {
        return -0x291 + (glyph - '0');
    }

    return -1;
}

std::string FormatValue(int value, bool show_sign) {
    if (show_sign && value > 0) {
        return std::format("+{}", value);
    }

    return std::format("{}", value);
}

void LayOutValue(ClsMes &mes, int value, int *x, int *y) {
    for (const char glyph : FormatValue(value, mes.value_signed != 0)) {
        const int code = ValueGlyphCode(glyph);

        if (code == -1) {
            continue;
        }

        mes.SetMesWinTbl(code, 0, *x, *y);

        if (mes.value_narrow != 0) {
            *x = *x + (mes.char_width >> 2) * 3;
        } else {
            *x = *x + mes.char_width;
        }
    }
}

/**
 * Resolves a system-message code to the message it names, or nothing where
 * the code is out of range or prints no message.
 */
std::optional<int> ResolveSystemCode(const ClsMes &mes, int code) {
    if (code < -0x500 || code >= -0x400) {
        return std::nullopt;
    }

    const int no = code + 0x500;

    switch (no) {
        case 0xFF:
        case 0xFA:
        case 0xF9:
        case 0xF8:
        case 0xF7:
        case 0xF6:
        case 0xF5:
        case 0xF4:
        case 0xF3:
            return std::nullopt;
        case 0xFE:
            return mes.mes_no[0];
        case 0xFD:
            return mes.mes_no[1];
        case 0xFC:
            return mes.mes_no[2];
        case 0xFB:
            return mes.mes_no[3];
        case 0xF2:
            return mes.mes_no[4];
        case 0xF1:
            return mes.mes_no[5];
        case 0xF0:
            return mes.mes_no[6];
        case 0xEF:
            return mes.mes_no[7];
        case 0xEE:
            return mes.mes_no[8];
        case 0xED:
            return mes.mes_no[9];
        default:
            return no;
    }
}

} // namespace

s16 ClsMes::GetGaijiW(int code) {
    if (code >= -0x300 && code < -0x251) {
        if (this->narrow_gaiji != 0 && (code == -0x2BD || code == -0x2BA || code == -0x2AB)) {
            return 5;
        }

        const s16 width = this->char_width;

        return width * EditGaijiTbl[code].width;
    }

    return this->char_width;
}

int ClsMes::GetNameLen(int chara) {
    if (chara < 0 || chara >= 6) {
        return -1;
    }

    const s16 *name = SaveData->GetCharaName(chara);

    for (int i = 0; i < 11; i++) {
        if (name[i] == 0) {
            return i;
        }
    }

    return -1;
}

int ClsMes::GetNameWidth(int chara) {
    if (chara < 0 || chara >= 6) {
        return -1;
    }

    const s16 *name = SaveData->GetCharaName(chara);
    s16        width = 0;

    for (int i = 0; i < 11; i++) {
        if (name[i] == 0) {
            return width;
        }

        const int code = NameRegistCodeJtoE(static_cast<u16>(name[i]) - 1);

        if (code >= -0x300 && code < -0x250) {
            width += this->GetGaijiW(code);
        } else {
            width += this->char_width;
        }
    }

    return -1;
}

void ClsMes::AutoSetSub(CCharacter *first, CCharacter *second, int *out_pos) {
    GetScrPosFromChar(first, out_pos);
    GetScrPosFromChar(second, &out_pos[2]);
}

void ClsMes::AutoSet(int *pos) {
    RECT      candidate[3][3];
    int       outside[3][3];
    float     clearance[3][3];
    const int speaker_x = pos[0];
    const int speaker_y = pos[1];
    const int other_x = pos[2];
    const int other_y = pos[3];
    int       width = 0xCA;
    int       height = 0x8A;

    if (this->win_width > 0xCA) {
        width = this->win_width;
    }

    if (this->win_height > 0x8A) {
        height = this->win_height;
    }

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
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

    int best_col = 0;
    int best_row = 0;

    if (this->auto_pos == MES_POS_AUTO) {
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                RECT *rect = &candidate[row][col];

                outside[row][col] = CheckPosInOutForRect(rect, speaker_x, speaker_y);

                if (outside[row][col] == 0) {
                    outside[row][col] = CheckPosInOutForRect(rect, other_x, other_y);
                }
            }
        }

        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                if (outside[row][col] != 0) {
                    clearance[row][col] = -1.0f;
                }

                RECT *rect = &candidate[row][col];

                clearance[row][col] = GetDisPosToRect(rect, speaker_x, speaker_y);

                const float other_clearance = GetDisPosToRect(rect, other_x, other_y);

                clearance[row][col] = std::min(clearance[row][col], other_clearance);
            }
        }

        float best_clearance = 0.0f;

        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
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

    this->text_x = this->win_x + static_cast<int>(3.0f * this->char_width);
    this->text_y = this->win_y + static_cast<int>(1.5f * this->char_height);

    if (this->tail_on != 0) {
        this->tail_to_x = speaker_x;
        this->tail_to_y = speaker_y;
    }

    this->grow_x = this->win_x + (this->win_width >> 1);
    this->grow_y = this->win_y + (this->win_height >> 1);

    if (this->tail_on != 0) {
        if (this->tail_to_x <= this->win_x + (this->win_width >> 2)) {
            this->tail_x = this->win_x + (this->win_width >> 2);
        } else if (this->win_x + (this->win_width * 3 >> 2) <= this->tail_to_x) {
            this->tail_x = this->win_x + (this->win_width * 3 >> 2);
        } else {
            this->tail_x = this->tail_to_x;
        }

        if (this->tail_to_y <= this->win_y + (this->win_height >> 2)) {
            this->tail_y = this->win_y + (this->win_height >> 2);
        } else if (this->win_y + (this->win_height * 3 >> 2) <= this->tail_to_y) {
            this->tail_y = this->win_y + (this->win_height * 3 >> 2);
        } else {
            this->tail_y = this->tail_to_y;
        }
    }
}

void ClsMes::AbsFukidashiIn() {
    if (this->win_x < 0x10) {
        this->win_x = 0x10;
        this->text_x = this->win_x + static_cast<int>(3.0f * this->char_width);
    } else if (this->win_x + this->win_width >= 0x270) {
        this->win_x = 0x270 - this->win_width;
        this->text_x = this->win_x + static_cast<int>(3.0f * this->char_width);
    }

    if (this->win_y < 0x10) {
        this->win_y = 0x10;
        this->text_y = this->win_y + static_cast<int>(1.5f * this->char_height);
    } else if (this->win_y + this->win_height >= 0x1B0) {
        this->win_y = 0x1B0 - this->win_height;
        this->text_y = this->win_y + static_cast<int>(1.5f * this->char_height);
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

    std::ranges::fill(this->mes_no, -1);
    std::ranges::fill(this->values, 0);

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

    for (MES_LINE_POS &pos : this->line_pos) {
        pos.x = -1;
        pos.y = -1;
    }

    this->buff = nullptr;
    this->buff_system = nullptr;
    this->text = nullptr;
    this->text_system = nullptr;
    this->tex_buff = nullptr;
    this->clut = MesWinClut;
}

void ClsMes::SetBuff(short *buff) {
    this->buff = buff;
    this->text = reinterpret_cast<char *>(this->buff) + static_cast<u16>(this->buff[1]);
}

void ClsMes::SetBuff_system(short *buff) {
    this->buff_system = buff;
    this->text_system = reinterpret_cast<char *>(this->buff_system) + static_cast<u16>(this->buff_system[1]);
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
        default:
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

        this->fade = std::min(this->fade, 1.0f);
    } else {
        if (0.0f < this->fade) {
            this->fade = this->fade - this->fade_speed;
        }

        this->fade = std::max(this->fade, 0.0f);
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

    float angle = atan2(static_cast<float>(this->tail_to_x - this->tail_x), static_cast<float>(this->tail_to_y - this->tail_y));

    left[0] = this->tail_x - this->tail_half_width;
    left[1] = this->tail_y;
    RollPos(centre, left, angle, left_out);
    this->tail_left_x = static_cast<int>(left_out[0]);
    this->tail_left_y = static_cast<int>(left_out[1]);

    right[0] = this->tail_x + this->tail_half_width;
    right[1] = this->tail_y;
    RollPos(centre, right, angle, right_out);
    this->tail_right_x = static_cast<int>(right_out[0]);
    this->tail_right_y = static_cast<int>(right_out[1]);

    float dx = this->tail_to_x - this->tail_x;
    float dy = this->tail_to_y - this->tail_y;
    float length = sqrt(dx * dx + dy * dy);

    float px = this->tail_length * dx / length;
    float py = this->tail_length * dy / length;

    this->tail_tip_x = static_cast<int>(px) + this->tail_x;
    this->tail_tip_y = static_cast<int>(py) + this->tail_y;
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
    if (this->tex_buff != nullptr) {
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
        default:
            break;
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

    this->text_at = std::min(this->text_at, static_cast<float>(this->text_len));

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

namespace {

s16 *SetAndGetNameRegistTbl(int chara) {
    if (chara < 0 || chara >= 6) {
        return nullptr;
    }

    std::ranges::fill(NameRegistTbl[chara], static_cast<s16>(MES_CODE_NEWLINE));

    const s16 *name = SaveData->GetCharaName(chara);

    for (int i = 0; i < 11; i++) {
        if (name[i] == 0) {
            NameRegistTbl[chara][i] = MES_CODE_NEWLINE;
        } else {
            NameRegistTbl[chara][i] = NameRegistCodeJtoE(static_cast<u16>(name[i]) - 1);
        }
    }

    return NameRegistTbl[chara];
}

} // namespace

void ClsMes::MakeMesWinTbl_value(int *x, int *y) {
    if (this->value_show == 0 && this->value == 0) {
        return;
    }

    LayOutValue(*this, this->value, x, y);
}

void ClsMes::MakeMesWinTbl_value(int which, int *x, int *y) {
    if (this->value_show == 0 && this->values[which] == 0) {
        return;
    }

    LayOutValue(*this, this->values[which], x, y);
}

int ClsMes::MakeMesWinTbl_system(int code, int *x, int *y) {
    const std::optional<int> no = ResolveSystemCode(*this, code);

    if (!no) {
        return 0;
    }

    if (this->buff_system == nullptr) {
        return 0;
    }

    short *at = this->GetTextLineDataTop_system(*no);

    if (at == nullptr) {
        return 0;
    }

    while (true) {
        const int text_code = *at;
        at++;

        if (text_code >= -0x506 && text_code < -0x500) {
            const s16 *name = SetAndGetNameRegistTbl(text_code + 0x506);

            if (name != nullptr) {
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
    const std::optional<int> no = ResolveSystemCode(*this, mes_no);

    if (!no) {
        return -1;
    }

    if (this->buff_system == nullptr) {
        return -1;
    }

    short *at = this->GetTextLineDataTop_system(*no);

    if (at == nullptr) {
        return -1;
    }

    int len[2] = {0, 0};
    int line = 0;

    while (true) {
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
            default:
                break;
        }

        len[line]++;
    }
}

int ClsMes::GetMesWidth_system(int mes_no) {
    if (this->buff_system == nullptr) {
        return -1;
    }

    short *at = this->GetTextLineDataTop_system(mes_no);

    if (at == nullptr) {
        return -1;
    }

    int width[2] = {0, 0};
    int line = 0;

    while (true) {
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
            default:
                break;
        }

        if (code >= -0x300 && code < -0x250) {
            width[line] += this->GetGaijiW(code);
        } else {
            width[line] += this->char_width;
        }
    }
}

short *ClsMes::GetTextLineDataTop_system(int line) {
    const int count = this->buff_system[0];
    short    *body = &this->buff_system[1];

    for (int i = 0; i < count; i++) {
        if (line == body[i * 2 + 1]) {
            return &body[count + body[i * 2 + 2]];
        }
    }

    return nullptr;
}

void ClsMes::InitMesWinTbl() {
    for (MES_WIN_LINE &entry : this->win_line) {
        entry = {};
    }

    this->win_line_num = 0;
}

int ClsMes::CalcSpaceW(int width, int glyph_width, short *text) {
    short *at = text;
    int    used = 0;
    int    spaces = 0;

    while (true) {
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
            default:
                break;
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
    if (this->buff == nullptr) {
        return 0;
    }

    short *at = this->GetTextLineDataTop(mes_no);

    if (at == nullptr) {
        return 0;
    }

    this->InitMesWinTbl();

    int x = 0;
    int y = 0;

    while (true) {
        const int code = *at;
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
                    const s16 *name = SetAndGetNameRegistTbl(code + 0x506);

                    if (name != nullptr) {
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
                } else if (this->MakeMesWinTbl_system(code, &x, &y) == 0 && this->SetMesWinTbl(code, 0, x, y) != 0) {
                    x += this->char_width;
                }

                break;
        }
    }
}

namespace {

void NeedMesWinWH_sub(int &len, int &max_len, int &width, int &max_width, int char_width, int count) {
    if (count <= 0) {
        return;
    }

    len += count;
    max_len = std::max(max_len, len);
    width += count * char_width;
    max_width = std::max(max_width, width);
}

} // namespace

void ClsMes::NeedMesWinWH(int mes_no, int *out) {
    if (this->buff == nullptr) {
        return;
    }

    short *at = this->GetTextLineDataTop(mes_no);

    if (at == nullptr) {
        return;
    }

    int &max_len = out[0];
    int &max_lines = out[1];
    int &max_width = out[2];
    int &max_height = out[3];

    max_len = 0;
    max_lines = 0;
    max_width = 0;
    max_height = 0;

    int len = 0;
    int lines = 0;
    int width = 0;
    int height = 0;

    auto add_value = [&](int value) {
        if (this->value_show == 0 && value == 0) {
            return;
        }

        const int digits = static_cast<int>(FormatValue(value, this->value_signed != 0).size());
        const int glyph_width = this->value_narrow != 0 ? (this->char_width >> 2) * 3 : this->char_width;

        NeedMesWinWH_sub(len, max_len, width, max_width, glyph_width, digits);
    };

    while (true) {
        const int code = *at;
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

                max_len = std::max(max_len, len);

                if (this->space_area >= 0 || this->space_width >= 0) {
                    width += this->space_width;
                } else {
                    width += this->char_width;
                }

                max_width = std::max(max_width, width);

                break;

            case MES_CODE_NEWLINE:
                len = 0;
                lines++;

                max_lines = std::max(max_lines, lines);

                width = 0;
                height += this->char_height;

                max_height = std::max(max_height, height);

                break;

            case MES_CODE_PAGE:
                len = 0;
                lines = 0;
                width = 0;
                height = 0;
                break;

            default:
                if ((code >= -0x200 && code <= -0x101) || (code >= -0x400 && code <= -0x301) || (code >= -0xA00 && code <= -0x901)) {
                    continue;
                }

                if (code >= -0x900 && code < -0x800) {
                    this->space_area = (code + 0x900) * 4;
                    this->space_width = this->CalcSpaceW(this->space_area, this->char_width, at - 1);
                } else if (code >= -0x800 && code < -0x700) {
                    this->space_width = code + 0x800;
                } else if (code >= -0x700 && code < -0x600) {
                    len++;

                    max_len = std::max(max_len, len);

                    width += code + 0x700;

                    max_width = std::max(max_width, width);
                } else if (code == -0x401) {
                    add_value(this->value);
                } else if (code >= -0x40D && code < -0x405) {
                    add_value(this->values[-0x406 - code]);
                } else if (code == -0x405 || code == -0x404 || code == -0x403 || code == -0x402 || code == -0x411 || code == -0x410 || code == -0x40F || code == -0x40E) {
                    NeedMesWinWH_sub(len, max_len, width, max_width, this->char_width, this->GetMesLen_system(code));
                } else if (code >= -0x506 && code < -0x500) {
                    NeedMesWinWH_sub(len, max_len, width, max_width, this->char_width, this->GetNameLen(code + 0x506));
                } else if (code >= -0x300 && code < -0x251) {
                    // Reads the whole first word of the entry, not just its s16 width.
                    int cells = 0;
                    std::memcpy(&cells, &EditGaijiTbl[code], sizeof(cells));

                    len += cells;
                    width += this->GetGaijiW(code);

                    max_len = std::max(max_len, len);

                    max_width = std::max(max_width, width);
                } else {
                    NeedMesWinWH_sub(len, max_len, width, max_width, this->char_width, 1);
                }

                break;
        }
    }
}

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
        this->win_width = this->char_width * static_cast<int>(3.0f + (3.0f + static_cast<float>(this->text_columns)));
    }

    this->win_height = this->char_height * static_cast<int>(1.5f + (1.5f + static_cast<float>(this->text_rows)));
    this->win_x = this->text_x - static_cast<int>(3.0f * static_cast<float>(this->char_width));
    this->win_y = this->text_y - static_cast<int>(1.5f * static_cast<float>(this->char_height));

    if (MakeMesWinTbl(mes_no) != 0) {
        this->text_len = this->win_line_num;
    }
}

void ClsMes::Myset2DSprite_Fuchi(sceVif1Packet *packet, CTexture *texture, int x, int y, int width, int height, int u, int v, int u_width, int v_height) {
    switch (this->style) {
        case MES_EDGE_WHITE:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0xFF, 0xFF, 0xFF, std::min(this->edge_alpha, 0x40));
            break;
        case MES_EDGE_BLACK:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0, 0, 0, std::min(this->edge_alpha, 0x40));
            break;
        case MES_EDGE_TABLE:
            for (const MES_FUCHI &pass : FuchiTbl_E) {
                if (pass.alpha <= 0) {
                    break;
                }

                set2DSprite_Core(packet, texture, CRect_i_(x + pass.x, y + pass.y, width, height), CRect_i_(u, v, u_width, v_height), pass.r, pass.g, pass.b, std::min(this->edge_alpha, pass.alpha));
            }

            break;
        case MES_EDGE_DOUBLE:
            set2DSprite_Core(packet, texture, CRect_i_(x + 1, y + 1, width, height), CRect_i_(u, v, u_width, v_height), 0x40, 0x40, 0x40, std::min(this->edge_alpha, 0x80));
            set2DSprite_Core(packet, texture, CRect_i_(x + 2, y + 2, width, height), CRect_i_(u, v, u_width, v_height), 0, 0, 0, std::min(this->edge_alpha, 0x80));
            break;
        default:
            break;
    }
}

namespace {

void SetspRGBA(spRGBA *colour, int r, int g, int b, int a) {
    colour->r = r;
    colour->g = g;
    colour->b = b;
    colour->a = a;
}

} // namespace

void ClsMes::DrawMesWin_sub(CTexture *texture, int dx, int dy, int frame) {
    int area[4];
    int pos[2];

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
    int top = area[1];
    int right = area[2];
    int bottom = area[3];
    int draw_width = right - left;
    int height = bottom - top;
    int tex_x = static_cast<int>(LinerInterpolation(this->grow_x, left, this->fade));
    int tex_y = static_cast<int>(LinerInterpolation(this->grow_y, top, this->fade));
    draw_width = static_cast<int>(draw_width * this->fade);
    int draw_height = static_cast<int>(height * this->fade);
    int screen_y = tex_y;
    tex_y -= top;
    int screen_x = tex_x + dx;
    screen_y += dy;

    if (frame != 0) {
        spRGBA bright;
        spRGBA dim;

        SetspRGBA(&bright, 0x80, 0x80, 0x80, 0x80);
        SetspRGBA(&dim, 0x60, 0x60, 0x60, 0x80);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y, draw_width, draw_height), CRect_i_(tex_x, tex_y, draw_width, draw_height), &dim, &dim, &dim, &dim, 1);
        }

        float win_left = this->win_x;
        float win_top = this->win_y;
        float win_w = this->win_width;
        float win_h = this->win_height;

        int outer_x = static_cast<int>(LinerInterpolation(this->grow_x, win_left, this->fade));
        int outer_y = static_cast<int>(LinerInterpolation(this->grow_y, win_top, this->fade));
        int outer_w = static_cast<int>(win_w * this->fade);
        int outer_h = static_cast<int>(win_h * this->fade);

        float core_left = this->win_x + this->win_width / 6.0f;
        float core_top = this->win_y + this->win_height / 6.0f;
        float core_width = 2.0f * this->win_width / 3.0f;
        float core_height = 2.0f * this->win_height / 3.0f;

        int core_x = static_cast<int>(LinerInterpolation(this->grow_x, core_left, this->fade));
        int core_y = static_cast<int>(LinerInterpolation(this->grow_y, core_top, this->fade));
        int core_w = static_cast<int>(core_width * this->fade);
        int core_h = static_cast<int>(core_height * this->fade);

        int right_x = core_x + core_w;
        int upper_screen_y = outer_y;
        int core_screen_y = core_y;
        int lower_screen_y = core_y + core_h;
        int left_w = core_x - outer_x;
        int right_w = outer_x + outer_w - core_x - core_w;
        int upper_h = core_y - outer_y;
        int lower_h = outer_y + outer_h - core_y - core_h;
        outer_y -= top;
        core_y -= top;
        int middle_screen_y = core_screen_y;
        int lower_tex_y = core_y + core_h;
        int upper_tex_h = core_y - outer_y;
        int lower_tex_h = outer_y + outer_h - core_y - core_h;

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
        set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y, draw_width, draw_height), CRect_i_(tex_x, tex_y, draw_width, draw_height), 0, 0, 0, std::min(this->edge_alpha, 0x80));
    }
}

/**
 * Draws a help window from nine texture pieces, stretching the edges and the
 * middle to the requested size.
 */
void MyMenuHelpWinDraw(int x, int y, int width, int height, int shade, int u, int v, CTexture *texture) {
    int bar_width = width < 0x20 ? 0 : width - 0x20;
    int bar_height = height < 0x1C ? 0 : height - 0x1C;

    setbilinear(0);

    const int alpha = (shade * 100) >> 7;

    set2DSprite(Vif1Packet, texture, CRect_i_(x, y, 0x10, 0xE), CRect_i_(u, v, 0x10, 0xE), alpha);
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
        default:
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
        default:
            break;
    }
}
