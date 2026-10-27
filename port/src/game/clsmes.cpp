#include "clsmes.hpp"
#include "mathconst.h"

#include <libpkt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <map>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "character.hpp"
#include "editpartsdata.hpp"
#include "editpartsinfo.hpp"
#include "gameutil.hpp"
#include "localize/localize.hpp"
#include "mglib.hpp"
#include "platform/config.hpp"
#include "platform/glyphs.hpp"
#include "platform/ttffont.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "render/gs_state.hpp"
#include "render/screen.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "text/caption_text.hpp"
#include "text/captionfont.hpp"
#include "text/gametext.hpp"
#include "text/glyph_draw.hpp"
#include "text/text_shadow.hpp"
#include "text/textwipe.hpp"
#include "texture.hpp"
#include "ui/menu_pointer.hpp"

namespace dc::game {

using namespace dc::localize;
using namespace dc::platform;
using namespace dc::render;
using namespace dc::text;
using namespace dc::ui;

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

// The linker script names EditGaijiTbl inside EditPartsData, and GetGaijiW indexes it with codes -0x300
// and up, which on the PS2 lands on the last word of each GaijiDataTbl entry: EditGaijiTbl is
// GaijiDataTbl + 0x601C. The port gives the table its own storage, copied from GaijiDataTbl before
// main (which nothing writes), and points EditGaijiTbl at its end.
namespace {

constexpr int kEditGaijiCodes = 0x300;

alignas(16) EDIT_GAIJI EditGaijiStorage[kEditGaijiCodes + 1];

static_assert(kEditGaijiCodes * sizeof(EDIT_GAIJI) == 0x6000);

[[gnu::constructor]] void CopyEditGaijiTable() {
    const auto            table = std::as_bytes(std::span(GaijiDataTbl));
    constexpr std::size_t first_word_offset = 0x1C;
    for (std::size_t i = 0; i < kEditGaijiCodes; i++) {
        std::size_t start = first_word_offset + i * sizeof(EDIT_GAIJI);
        if (start >= sizeof(GaijiDataTbl)) {
            break;
        }
        const auto entry = table.subspan(start, std::min(sizeof(EDIT_GAIJI), table.size() - start));
        std::memcpy(&EditGaijiStorage[i], entry.data(), entry.size());
    }
}

} // namespace

EDIT_GAIJI *const EditGaijiTbl = EditGaijiStorage + kEditGaijiCodes;

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

// Named the other way round from set2DSprite: src is where the sprite lands, dst the texels.
void Myset2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &src, const CRect_i_ &dst, u8 r,
                   u8 g, u8 b, u8 a) {
    if (texture == nullptr) {
        return;
    }

    gfx::DrawState state = draw2d::Get().draw_state();
    state.blend = true;
    state.fog = false;
    state.cull = gfx::CullMode::None;
    MGPortApplyZbuf(state, mgZBuffer);

    const auto x0 = static_cast<float>(src.x);
    const auto y0 = static_cast<float>(src.y);
    const auto x1 = static_cast<float>(src.x + src.width);
    const auto y1 = static_cast<float>(src.y + src.height);
    const auto u0 = static_cast<float>(dst.x);
    const auto v0 = static_cast<float>(dst.y);
    const auto u1 = static_cast<float>(dst.x + dst.width);
    const auto v1 = static_cast<float>(dst.y + dst.height);

    std::array<gfx::Vertex2D, 4> quad = {
        draw2d::Vertex(x0, y0, 0.0f, u0, v0, r, g, b, a),
        draw2d::Vertex(x1, y0, 0.0f, u1, v0, r, g, b, a),
        draw2d::Vertex(x1, y1, 0.0f, u1, v1, r, g, b, a),
        draw2d::Vertex(x0, y1, 0.0f, u0, v1, r, g, b, a),
    };
    draw2d::DrawTextured(gfx::Primitive::Quads, quad, texture->tex0, 0x61, state);
    draw2d::Get().set_zbuf(nullptr);
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

namespace {

// The window ClsMesSetMessageOverlay names, and the messages laid over its own.
const ClsMes *g_overlay_mes = nullptr;
short        *g_overlay_file = nullptr;

// Message line's text in a message file, laid out as SetBuff reads one; null where it has none.
short *FindMessage(short *file, int line) {
    int    count = file[0];
    short *body = &file[1];

    for (int i = 0; i < count; i++) {
        if (line == body[i * 2 + 1]) {
            return &body[count] + body[i * 2 + 2];
        }
    }

    return nullptr;
}

} // namespace

short *ClsMes::GetTextLineDataTop(int line) {
    if (this == g_overlay_mes) {
        if (short *text = FindMessage(g_overlay_file, line)) {
            return text;
        }
    }

    return FindMessage(this->buff, line);
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

// Keep the message layout within the fixed character table.
int ClsMes::SetMesWinTbl(int code, int mode, short x, short y) {
    if (code >= -0x200 && code <= -0x101) {
        if (this->win_line_num > 0 && this->win_line_num <= MES_WIN_LINE_MAX) {
            this->win_line[this->win_line_num - 1].space += code + 0x200;
        }

        return 0;
    }

    if (code >= -0x400 && code <= -0x301) {
        if (code + 0x400 == 0) {
            this->clut_now = this->clut_default;
        } else if (code == -0x301) {
            this->clut_now = 0xFF;
        } else {
            this->clut_now = code + 0x401;
        }

        return 0;
    }

    if (code >= -0xA00 && code <= -0x901) {
        this->fukidashi_shape = code + 0xA00;
        return 0;
    }

    if (this->win_line_num < 0 || this->win_line_num >= MES_WIN_LINE_MAX) {
        return 0;
    }

    if (mode == 1) {
        this->win_line[this->win_line_num].code = code + 0x1000;
        this->win_line[this->win_line_num].x = x;
        this->win_line[this->win_line_num].y = y;
        this->win_line[this->win_line_num].clut = this->clut_now;
        this->win_line_num++;
    } else {
        this->win_line[this->win_line_num].code = code;
        this->win_line[this->win_line_num].x = x;
        this->win_line[this->win_line_num].y = y;
        this->win_line[this->win_line_num].clut = this->clut_now;
        this->win_line_num++;
    }

    return 1;
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

void MakeRandTbl(float lo, float hi) {
    if (GameTextKeepsRandom()) {
        return;
    }
    for (float &value : RandTbl) {
        value = ((hi - lo) * static_cast<float>(std::rand())) / 2147483648.0f;
        value += lo;
    }
}

void MakeRandTbl2(float lo, float hi) {
    if (GameTextKeepsRandom()) {
        return;
    }
    for (float &value : RandTbl2) {
        value = ((hi - lo) * static_cast<float>(std::rand())) / 2147483648.0f;
        value += lo;
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

struct Shape {
    gfx::Primitive             primitive;
    std::vector<gfx::Vertex2D> vertices;
};

// What MakeFukidashi_sub and DrawMaru put in the packet for MakeFukidashi to draw: every PRIM write
// starts a fan or a triangle list, every XYZF2 is a point in fukidashibase's texel rows.
std::vector<Shape> g_shapes;

void BeginShape(gfx::Primitive primitive) { g_shapes.push_back({.primitive = primitive, .vertices = {}}); }

void ShapePoint(int x, int y) {
    if (g_shapes.empty()) {
        BeginShape(gfx::Primitive::TriangleFan);
    }
    g_shapes.back().vertices.push_back(draw2d::Vertex(x, y, 0.0f, 0.0f, 0.0f, 0xBF, 0xBF, 0xBF, 0x80));
}

constexpr const char *kBubbleTarget = "fukidashibase";

gfx::TextureHandle BubbleTarget(CTexture *texture) {
    if (texture != nullptr) {
        PortTextureRef ref = draw2d::Get().texture(texture->tex0, 0);
        if (ref.valid) {
            std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(ref.binding.texture);
            if (info && info->render_target) {
                return ref.binding.texture;
            }
        }
    }
    return gfx::FindNamedRenderTarget(kBubbleTarget);
}

void DrawMaru(sceVif1Packet * /*packet*/, int x, int y, int width, int height, int lift, int /*unused*/, int rough,
              int /*prim*/) {
    BeginShape(gfx::Primitive::TriangleFan);

    int px = x + (width >> 1);
    int py = y + (height >> 1) - lift;

    ShapePoint(px, py);

    float turn = 0.0f;
    int   step = 0;

    while (turn < 128.0f) {
        float angle = TWO_PI * turn / 128.0f;

        float across = std::cos(angle);
        float down = std::sin(angle);

        if (rough == 1 && step % 2 == 1) {
            across *= 0.9f;
            down *= 0.85f;
        }

        across += 1.0f;
        down += 1.0f;
        across *= width;
        down *= height;
        across /= 2.0f;
        down /= 2.0f;

        px = static_cast<int>(across);
        py = static_cast<int>(down);
        px += x;
        py += y - lift;

        ShapePoint(px, py);

        if (rough == 1) {
            turn += RandTbl[step];
        } else {
            turn += 4.0f;
        }

        step++;
    }

    float across = 1.0f;
    float down = 0.0f;

    if (rough == 1 && step % 2 == 1) {
        across *= 0.9f;
        down *= 0.85f;
    }

    across += 1.0f;
    down += 1.0f;
    across *= width;
    down *= height;
    across /= 2.0f;
    down /= 2.0f;

    px = static_cast<int>(across);
    py = static_cast<int>(down);
    px += x;
    py += y - lift;

    ShapePoint(px, py);
}

} // namespace

void ClsMes::MakeFukidashi_sub(sceVif1Packet *packet, int prim) {
    int x = 0;
    int y = 0;
    int top = std::min(this->win_y, this->grow_y);

    if (this->tail_on != 0) {
        top = std::min({top, this->tail_left_y, this->tail_right_y, this->tail_tip_y});

        if (this->fukidashi_shape == 2) {
            top = std::min(top, this->tail_tip_y - 5);
        }
    }

    const float grown_width = this->win_width * this->fade;
    const float grown_height = this->win_height * this->fade;

    constexpr std::array<std::array<float, 2>, 15> shape = {
        {
         {0.5f, 0.5f},
         {0.02f, 0.29f},
         {0.22f, 0.03f},
         {0.51f, 0.06f},
         {0.73f, 0.0f},
         {0.92f, 0.1f},
         {0.98f, 0.36f},
         {0.98f, 0.74f},
         {0.9f, 0.91f},
         {0.7f, 0.99f},
         {0.43f, 0.91f},
         {0.2f, 0.97f},
         {0.03f, 0.8f},
         {0.0f, 0.52f},
         {0.02f, 0.29f},
         }
    };

    if (this->fukidashi_shape == 0) {
        BeginShape(gfx::Primitive::TriangleFan);

        int base_x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        int base_y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        const int mirror = static_cast<int>(RandTbl[0]);

        for (const auto &point : shape) {
            if (mirror % 2 != 0) {
                x = static_cast<int>(grown_width * (1.0f - point[0]));
            } else {
                x = static_cast<int>(grown_width * point[0]);
            }

            if (mirror > 1) {
                y = static_cast<int>(grown_height * (1.0f - point[1]));
            } else {
                y = static_cast<int>(grown_height * point[1]);
            }

            x += base_x;
            y += base_y;
            y -= top;

            ShapePoint(x, y);
        }
    } else if (this->fukidashi_shape == 1 || this->fukidashi_shape == 2) {
        x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        int bubble_shape = this->fukidashi_shape;

        if (bubble_shape == 1) {
            DrawMaru(packet, x, y, static_cast<int>(grown_width), static_cast<int>(grown_height), top, 0x20, bubble_shape, prim);
        } else {
            int  upper_width = static_cast<int>(grown_width) - this->char_width;
            int  half_height = static_cast<int>(grown_height) >> 1;
            auto half_height_float = static_cast<float>(half_height);

            DrawMaru(packet, x, y, upper_width, static_cast<int>(half_height_float + 1.5f * this->char_height), top,
                     0x20, 0, prim);

            float pad = 1.5f * this->char_height;
            int   indent = this->char_width;

            y = static_cast<int>(y + half_height - pad);
            DrawMaru(packet, x + indent, y, static_cast<int>(grown_width) - indent, static_cast<int>(half_height + pad), top, 0x20,
                     0, prim);
        }
    } else {
        x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        BeginShape(gfx::Primitive::TriangleFan);

        int oval_width = static_cast<int>(grown_width);
        int oval_height = static_cast<int>(grown_height);
        int px = x + (oval_width >> 1);
        int py = y + (oval_height >> 1) - top;

        ShapePoint(px, py);

        float turn = 0.0f;
        int   step = 0;

        while (turn < 128.0f) {
            float angle = TWO_PI * turn / 128.0f;

            float across = std::cos(angle);
            float down = std::sin(angle);

            across = across * RandTbl2[step];
            down = down * RandTbl2[step];
            across += 0.5f;
            down += 0.5f;
            across = across * oval_width;
            down = down * oval_height;

            px = static_cast<int>(across);
            py = static_cast<int>(down);

            px += x;
            py += y - top;

            ShapePoint(px, py);

            turn += RandTbl[step];
            step++;
        }

        float across = 1.0f;
        float down = 0.0f;

        across = across * RandTbl2[0];
        down = down * RandTbl2[0];
        across += 0.5f;
        down += 0.5f;
        across = across * oval_width;
        down = down * oval_height;

        px = static_cast<int>(across);
        py = static_cast<int>(down);

        px += x;
        py += y - top;

        ShapePoint(px, py);
    }

    if (this->tail_on != 0) {
        if (this->fukidashi_shape == 2) {
            x = static_cast<int>(LinerInterpolation(this->grow_x, (this->tail_x + this->tail_tip_x) >> 1, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, (this->tail_y + this->tail_tip_y) >> 1, this->fade));
            DrawMaru(packet, x - 10, y - 10, 20, 20, top, 0x20, 0, prim);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade));
            DrawMaru(packet, x - 5, y - 5, 10, 10, top, 0x20, 0, prim);
        } else {
            BeginShape(gfx::Primitive::Triangles);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade));
            y -= top;
            ShapePoint(x, y);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_left_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_left_y, this->fade));
            y -= top;
            ShapePoint(x, y);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_right_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_right_y, this->fade));
            y -= top;
            ShapePoint(x, y);
        }
    }
}

// Retail renders the bubble into fukidashibase in three GS passes: a full-height fill with alpha 0
// that leaves the colour and clears destination alpha, the bubble's fans in 0xBFBFBF with alpha
// 0x80 (once antialiased, once solid), then fuki256 tiled over the whole target under DATE/DATM=1,
// which only lets the tiles through where destination alpha has bit 7 set: inside the fans. The
// destination alpha test is a shape mask. Here the target's own depth buffer carries the mask:
// cleared to near, the fans write far, and the tiles test GEQUAL at far, which passes exactly on
// the fan pixels and nowhere else, once per pixel as DATE does. Colour and alpha come out as
// retail's: fans written through the current ALPHA, tiles blended over them, alpha 0 outside.
// Lost: the antialiased fan edges, and retail's tiles writing Z=0 into the frame's own Z buffer at
// the target's coordinates.
void ClsMes::MakeFukidashi(sceVif1Packet *packet) {
    const draw2d::Services &services = draw2d::Get();

    g_shapes.clear();
    this->MakeFukidashi_sub(packet, 1);
    this->MakeFukidashi_sub(packet, 0);

    const gfx::TextureHandle target = BubbleTarget(services.find_texture(kBubbleTarget));
    if (target != gfx::kNullTexture) {
        const gfx::TextureHandle previous = gfx::CurrentRenderTarget();
        gfx::SetRenderTarget(target);

        gfx::DrawState state = services.draw_state();
        state.blend = true;
        state.alpha_test = false;
        state.depth_test = gfx::DepthTest::Always;
        state.depth_write = false;
        state.fog = false;
        state.cull = gfx::CullMode::None;
        state.scissor = false;

        constexpr std::array<uint8_t, 4> none{};
        gfx::Clear(false, none.data(), true, 1.0f);

        const auto                   fill_bottom = static_cast<float>(SCREEN_HALF_HEIGHT);
        std::array<gfx::Vertex2D, 4> fill = {
            draw2d::Vertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(kScreenWidth, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(kScreenWidth, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(0.0f, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
        };
        textwipe::Draw2D(gfx::Primitive::Quads, fill, {}, state);

        gfx::DrawState fans = state;
        fans.depth_write = true;
        for (const Shape &shape : g_shapes) {
            if (shape.vertices.size() >= 3) {
                textwipe::Draw2D(shape.primitive, shape.vertices, {}, fans);
            }
        }

        CTexture *paper = services.find_texture("fuki256");
        if (paper != nullptr) {
            gfx::DrawState tiles = state;
            tiles.depth_test = gfx::DepthTest::GEqual;
            const u_char               alpha = std::min(this->edge_alpha, 0x80);
            std::vector<gfx::Vertex2D> quads;
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 5; col++) {
                    const auto x0 = static_cast<float>(col * 0x80);
                    const auto y0 = static_cast<float>(row * 0x80);
                    quads.push_back(draw2d::Vertex(x0, y0, 0.0f, 0.0f, 0.0f, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(draw2d::Vertex(x0 + 0x80, y0, 0.0f, 0x80, 0.0f, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(
                        draw2d::Vertex(x0 + 0x80, y0 + 0x80, 0.0f, 0x80, 0x80, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(draw2d::Vertex(x0, y0 + 0x80, 0.0f, 0.0f, 0x80, 0x80, 0x80, 0x80, alpha));
                }
            }
            draw2d::DrawTextured(gfx::Primitive::Quads, quads, paper->tex0, 0x61, tiles);
        }

        gfx::SetRenderTarget(previous);
    }

    g_shapes.clear();
    services.set_test(nullptr);
    services.set_zbuf(nullptr);
    services.set_alpha(nullptr);
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

namespace {

// Columns of a GaijiDataTbl row.
constexpr int kGaijiCode = 0;
constexpr int kGaijiU = 1;
constexpr int kGaijiV = 2;
constexpr int kGaijiWidth = 3;
constexpr int kGaijiHeight = 4;
constexpr int kGaijiXOff = 5;
constexpr int kGaijiYOff = 6;
constexpr int kGaijiCells = 7;

// The window ClsMesSetChoiceRows names, its first choice row, and whether its choices run on past it.
const ClsMes *g_choice_mes = nullptr;
int           g_choice_first_row = 0;
bool          g_choice_more_above = false;
bool          g_choice_more_below = false;

// The wide argument DrawGaijiFont takes for a character of palette entry clut on row. 1 dims it, as retail
// does, by doubling its colour: dark text on the paper bubble pales. 2, the port's, halves its opacity
// instead: a coloured choice in a ClsMesSetChoiceRows window, which doubling would brighten rather than
// pale. With a cursor, every row but the cursor's dims (only the cursor's, with cursor_lit), save the text
// rows above a window's choices.
int ChoiceRowDim(const ClsMes &mes, int row, int clut) {
    const bool ours = &mes == g_choice_mes;
    if (mes.cursor_row < 0 || (ours && row < g_choice_first_row)) {
        return 0;
    }
    const bool dim = mes.cursor_lit == 1 ? row == mes.cursor_row : row != mes.cursor_row;
    if (!dim) {
        return 0;
    }
    return ours && clut != mes.clut_default ? 2 : 1;
}

// ---- Message text from a TrueType font ----
// Without Chronicle Sharp's letters (below: they need font.ttf and font_thin.ttf), a character font.ttf in a lang folder
// (Chronicle Sharp, the save's or the executable's, or the font --font names) has is drawn from it at the screen's own
// resolution, in the cell the game laid the bitmap character out in, instead of from the 14x20 bitmaps of gaiji.img.
// Icons and characters the font lacks draw as before.

bool TtfReady() {
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::filesystem::path file = LocalizeFindFile("font.ttf");
        if (!file.empty() && ttffont::Load(file)) {
            std::println(stderr, "font: message text from {}", file.string());
        } else if (file.empty()) {
            std::println(stderr, "font: no font.ttf in lang/ beside the executable or in the save folder, and no --font; "
                                 "message text uses the game's bitmaps (docs/LOCALIZATION.md)");
        }
    }
    return ttffont::Loaded();
}

// ---- Message letters from Chronicle Sharp ----
// The 88 letters of gaiji.img (nine 14x20 cells a row from u = 128 for the bold set and u = 256 for the narrower one),
// drawn from Chronicle Sharp at the screen's own resolution: font.ttf's letters for the bold set, font_thin.ttf's for the
// narrower. Each letter's cell is drawn once at the size the screen shows it, the narrower letters with the drop shadow
// they carry in their own texels, into the captions' font atlas, and the game's letters are cut from it as the game cuts
// them from its bitmaps, so it lays them out and edges them as it always has; only their shapes are the font's. The
// fonts lay each letter out on its cell, 50 units a cell pixel with the baseline at its row 15.

constexpr std::string_view kLetterOrder =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'=\"!?#&+-*/%()@|<>{}[]:,.$0123456789";
constexpr int kCellW = 14;
constexpr int kCellH = 20;
constexpr int kLettersU = 128; // the letters' left edge in gaiji.img
constexpr int kLettersW = 256; // both sets

// A letter DrawMesWin draws: the part of its cell (in gaiji.img's texels), where (logical), and its tint.
struct PendingLetter {
    CRect_i_ texel;
    float    x, y, width, height;
    u_char   r, g, b, a;
};

std::vector<PendingLetter> *g_letters = nullptr; // where DrawMesWin's letters go while it draws them

// The cells drawn so far, by set, letter and height in screen pixels, while the atlas they are in lasts.
std::map<std::tuple<int, int, int>, capfont::Glyph> g_cells;
unsigned                                            g_cells_generation = 0;

// The drop shadow the game's narrower letters carry in their own texels (the bold ones have none): the letter's
// coverage moved down and right, blurred a little and thickened, at most half dark. Its shape was measured on the
// game's 88 cells; `scale` is pixels per game pixel.
std::vector<float> BakedShadow(const std::vector<float> &cov, int w, int h, float scale) {
    constexpr float shift_x = 0.866f;
    constexpr float shift_y = 0.841f;
    constexpr float blur = 0.741f;
    constexpr float gain = 9.21f;
    constexpr float most = 0.512f;
    const float     sigma = blur * scale;
    const int       reach = static_cast<int>(std::ceil(3 * sigma + std::max(shift_x, shift_y) * scale)) + 1;
    auto            kernel = [&](float shift) {
        std::vector<float> k(static_cast<size_t>(2 * reach + 1));
        float              sum = 0;
        for (int t = -reach; t <= reach; t++) {
            const float d = static_cast<float>(t) - shift;
            const int   tap = t + reach;
            k[static_cast<size_t>(tap)] = std::exp(-d * d / (2 * sigma * sigma));
            sum += k[static_cast<size_t>(tap)];
        }
        for (float &v : k) {
            v /= sum;
        }
        return k;
    };
    // A pixel's shadow comes from the letter up and to the left of it: the kernels are centred that far back.
    const std::vector<float> kx = kernel(shift_x * scale);
    const std::vector<float> ky = kernel(shift_y * scale);
    std::vector<float>       across(cov.size(), 0.0f);
    std::vector<float>       out(cov.size(), 0.0f);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int t = -reach; t <= reach; t++) {
                const int tap = t + reach;
                if (x - t >= 0 && x - t < w) {
                    sum += kx[static_cast<size_t>(tap)] * cov[static_cast<size_t>(y) * w + (x - t)];
                }
            }
            across[static_cast<size_t>(y) * w + x] = sum;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int t = -reach; t <= reach; t++) {
                const int tap = t + reach;
                const int from_y = y - t;
                if (from_y >= 0 && from_y < h) {
                    sum += ky[static_cast<size_t>(tap)] * across[static_cast<size_t>(from_y) * w + x];
                }
            }
            out[static_cast<size_t>(y) * w + x] = most * std::min(1.0f, gain * sum);
        }
    }
    return out;
}

// Whether the fonts the letters are drawn from are both there.
bool LettersReady() {
    static const bool ready = caption::Ready() && caption::ThinReady();
    return ready;
}

// Letter `index` of `set` drawn into its cell `size` screen pixels high, in the atlas: the white letter over its black
// shadow.
bool CellGlyph(int set, int index, int size, capfont::Glyph &out) {
    if (g_cells_generation != capfont::Generation()) {
        g_cells.clear();
        g_cells_generation = capfont::Generation();
    }
    const auto key = std::make_tuple(set, index, size);
    if (const auto found = g_cells.find(key); found != g_cells.end()) {
        out = found->second;
        return true;
    }
    const float                scale = static_cast<float>(size) / kCellH;
    const int                  width = static_cast<int>(std::lround(kCellW * scale));
    const int                  height = size;
    const int                  baseline = static_cast<int>(std::lround(15 * scale));
    std::vector<unsigned char> alpha;
    int                        w = 0;
    int                        h = 0;
    int                        x0 = 0;
    int                        y0 = 0;
    if (!capfont::RenderGlyphScaled(static_cast<unsigned char>(kLetterOrder[static_cast<size_t>(index)]), set, scale / 50.0f,
                                    alpha, w, h, x0, y0)) {
        return false;
    }
    std::vector<float> cov(static_cast<size_t>(width) * height, 0.0f);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const int cx = x0 + x;
            const int cy = baseline + y0 + y;
            if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
                cov[static_cast<size_t>(cy) * width + cx] = alpha[static_cast<size_t>(y) * w + x] / 255.0f;
            }
        }
    }
    const std::vector<float>   shadow = set == 1 ? BakedShadow(cov, width, height, scale) : std::vector<float>(cov.size(), 0.0f);
    std::vector<unsigned char> rgba(cov.size() * 4);
    for (size_t k = 0; k < cov.size(); k++) {
        const float a = cov[k] + (1 - cov[k]) * shadow[k];
        rgba[k * 4] = rgba[k * 4 + 1] = rgba[k * 4 + 2] = static_cast<unsigned char>(a > 0 ? std::lround(255 * cov[k] / a) : 255);
        rgba[k * 4 + 3] = static_cast<unsigned char>(std::lround(a * 255));
    }
    capfont::Glyph glyph;
    if (!capfont::AddImage(rgba, width, height, glyph)) {
        return false;
    }
    // Adding it may have started the atlas again.
    if (g_cells_generation != capfont::Generation()) {
        g_cells.clear();
        g_cells_generation = capfont::Generation();
    }
    g_cells[key] = glyph;
    out = glyph;
    return true;
}

// Whether a texel rect of gaiji.img lies in one of the letters' cells.
bool FontLetter(const CRect_i_ &texel) {
    const int across = texel.x - kLettersU;
    if (across < 0 || texel.width <= 0 || texel.height <= 0 || across + texel.width > kLettersW || texel.y < 0) {
        return false;
    }
    const int in_set = across % 128;
    const int column = in_set / kCellW;
    const int row = texel.y / kCellH;
    return column < 9 && (in_set + texel.width - 1) / kCellW == column && (texel.y + texel.height - 1) / kCellH == row &&
           row * 9 + column < static_cast<int>(kLetterOrder.size());
}

// The letter at texel, to be drawn at x, y (logical) from the font once DrawMesWin has laid out its window.
void LetterQuad(const CRect_i_ &texel, float x, float y, float width, float height, int r, int g, int b, int alpha) {
    auto clamp = [](int v) { return static_cast<u_char>(std::clamp(v, 0, 255)); };
    g_letters->push_back({.texel = texel,
                          .x = x,
                          .y = y,
                          .width = width,
                          .height = height,
                          .r = clamp(r),
                          .g = clamp(g),
                          .b = clamp(b),
                          .a = clamp(alpha)});
}

// The window's letters, each from its cell drawn at the screen's height for a cell.
void DrawLetters(const std::vector<PendingLetter> &letters) {
    if (letters.empty()) {
        return;
    }
    const gfx::LogicalMapping  mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float                rows = draw2d::RowScale();
    const int                  size = std::clamp(static_cast<int>(std::lround(kCellH * mapping.scale_y * rows)), kCellH, 512);
    std::vector<gfx::Vertex2D> quads;
    // Made again, once, if the atlas started over while the cells were drawn into it (that would leave the first ones
    // stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        quads.clear();
        for (const PendingLetter &letter : letters) {
            const int      across = letter.texel.x - kLettersU;
            const int      set = across / 128;
            const int      in_set = across % 128;
            const int      cell_u = kLettersU + set * 128 + in_set / kCellW * kCellW;
            const int      cell_v = letter.texel.y / kCellH * kCellH;
            capfont::Glyph cell;
            if (!CellGlyph(set, (letter.texel.y / kCellH) * 9 + in_set / kCellW, size, cell)) {
                continue;
            }
            const float du = (cell.u1 - cell.u0) / kCellW;
            const float dv = (cell.v1 - cell.v0) / kCellH;
            const float u0 = cell.u0 + static_cast<float>(letter.texel.x - cell_u) * du;
            const float u1 = u0 + static_cast<float>(letter.texel.width) * du;
            const float v0 = cell.v0 + static_cast<float>(letter.texel.y - cell_v) * dv;
            const float v1 = v0 + static_cast<float>(letter.texel.height) * dv;
            const float x0 = letter.x;
            const float x1 = letter.x + letter.width;
            const float y0 = letter.y * rows;
            const float y1 = (letter.y + letter.height) * rows;
            quads.push_back(draw2d::Vertex(x0, y0, 0.0f, u0, v0, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x1, y0, 0.0f, u1, v0, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x1, y1, 0.0f, u1, v1, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x0, y1, 0.0f, u0, v1, letter.r, letter.g, letter.b, letter.a));
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    if (!quads.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, quads, capfont::Binding(), draw2d::SpriteState());
        draw2d::RestoreTestZbuf();
    }
}

// Myset2DSprite_Fuchi's outline and shadow passes for a font letter. Retail's pass is the 14x20 letter again, a whole
// pixel off: its soft one-texel edges make the shadow a soft one. The font's letter is sharp, so each pass is spread
// over that pixel instead (wider for the outline table), as a grid of fainter copies that together come to the pass's
// strength. False where the letter is not the font's.
bool LetterFuchi(const ClsMes &mes, const CRect_i_ &screen, const CRect_i_ &texel) {
    if (g_letters == nullptr || !FontLetter(texel)) {
        return false;
    }
    constexpr int steps = 4;
    float         spread = 1.0f;
    float         weaken = 1.0f;
    auto          pass = [&](float dx, float dy, int r, int g, int b, int alpha) {
        const float strength = weaken * static_cast<float>(std::min(alpha, 0x80)) / 128.0f;
        const float each = 1.0f - std::pow(1.0f - std::min(strength, 0.999f), 1.0f / (steps * steps));
        const int   tap_alpha = static_cast<int>(std::lround(each * 128.0f));
        for (int i = 0; i < steps; i++) {
            for (int j = 0; j < steps; j++) {
                const float sx = ((static_cast<float>(i) + 0.5f) / steps - 0.5f) * spread;
                const float sy = ((static_cast<float>(j) + 0.5f) / steps - 0.5f) * spread;
                LetterQuad(texel, static_cast<float>(screen.x) + dx + sx, static_cast<float>(screen.y) + dy + sy,
                           static_cast<float>(screen.width), static_cast<float>(screen.height), r, g, b, tap_alpha);
            }
        }
    };
    const int cap = mes.edge_alpha;
    switch (mes.style) {
        case MES_EDGE_WHITE:
            pass(1, 1, 0xFF, 0xFF, 0xFF, std::min(cap, 0x40));
            break;
        case MES_EDGE_BLACK:
            pass(1, 1, 0, 0, 0, std::min(cap, 0x40));
            break;
        case MES_EDGE_TABLE:
            // The outline the table makes (a weapon's name), two pixels out each way in the game: the game's soft letters
            // fade it and keep it short of black, so the sharp letters' copies sit 1.6 pixels out, spread a little wider
            // and a little lighter.
            spread = 1.2f;
            weaken = 0.85f;
            for (int i = 0; FuchiTbl_E[i].alpha > 0; i++) {
                constexpr float reach = 0.8f;
                pass(static_cast<float>(FuchiTbl_E[i].x) * reach, static_cast<float>(FuchiTbl_E[i].y) * reach, FuchiTbl_E[i].r,
                     FuchiTbl_E[i].g, FuchiTbl_E[i].b, std::min(cap, FuchiTbl_E[i].alpha));
            }
            break;
        case MES_EDGE_DOUBLE:
            pass(1, 1, 0x40, 0x40, 0x40, std::min(cap, 0x80));
            pass(2, 2, 0, 0, 0, std::min(cap, 0x80));
            break;
        default:
            break;
    }
    return true;
}

// What DrawGaijiFont tints a character with: the colour table's entry, the flashing cursor row's,
// and double for a lit row.
void GlyphTint(int clut, int dark, int wide, int &r, int &g, int &b, int &alpha) {
    alpha = 0x80;
    switch (FontColorTbl[clut]) {
        case FONT_COLOR_WHITE:
            r = g = b = 0x5F;
            break;
        case FONT_COLOR_BROWN:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            break;
        case FONT_COLOR_YELLOW:
            r = 0x5E;
            g = 0x5E;
            b = 0x20;
            break;
        case FONT_COLOR_CYAN:
            r = 0x20;
            g = 0x5E;
            b = 0x5E;
            break;
        case FONT_COLOR_GREEN:
            r = 0x20;
            g = 0x5E;
            b = 0x20;
            break;
        case FONT_COLOR_BROWN_OPAQUE:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            break;
        case FONT_COLOR_GOLD:
            r = 0x73;
            g = 0x67;
            b = 0x33;
            break;
        case FONT_COLOR_GREY:
            r = g = b = 0x47;
            break;
        case FONT_COLOR_MAGENTA:
            r = 0x5F;
            g = 0x1F;
            b = 0x5F;
            break;
        default:
            r = 0x28;
            g = 0x28;
            b = 0x20;
            break;
    }
    if (clut == 0xFF) {
        if (dark != 0) {
            r = 0x7F;
            g = 0x22;
            b = 0x22;
        } else {
            r = 0x40;
            g = 0x11;
            b = 0x11;
        }
    }
    if (wide == 1) {
        r *= 2;
        g *= 2;
        b *= 2;
    }
    if (wide == 2) {
        alpha /= 2;
    }
}

void TtfQuad(std::vector<gfx::Vertex2D> &out, const ttffont::Glyph &glyph, float x, float y, float width,
             float height, int r, int g, int b, int alpha) {
    auto         clamp = [](int v) { return static_cast<u_char>(std::clamp(v, 0, 255)); };
    const u_char red = clamp(r);
    const u_char green = clamp(g);
    const u_char blue = clamp(b);
    const u_char a = clamp(alpha);
    out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y, 0.0f, glyph.u1, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y + height, 0.0f, glyph.u1, glyph.v1, red, green, blue, a));
    out.push_back(draw2d::Vertex(x, y + height, 0.0f, glyph.u0, glyph.v1, red, green, blue, a));
}

// The em the font is drawn at: its line fits 82% of the cell's height and its widest glyph the cell's
// width, so a glyph never leaves the cell the game sized the window for.
// Height and baseline follow the bitmap font's proportions.
constexpr float kTextHeight = 1.28f;
constexpr float kMinSqueeze = 0.78f;
constexpr float kBaselineShift = 0.6f;

float TtfEm(const ClsMes &mes, const ttffont::Metrics & /*metrics*/) {
    return mes.char_height * 0.82f * kTextHeight * textwipe::Get(textwipe::MessageKind()).size;
}

// Where each laid-out character of a window starts, in the window's own coordinates. A word (a run of
// characters the font draws) keeps the place the game gave it: its letters go one after another by
// their own widths and the word is centred on the cells the game gave it, so rows, indents, centring and
// the icons between words stay where the game put them. A glyph is never given more than its cell, so
// a word is never wider than the game's.
void TtfLayoutRows(ClsMes &mes, std::vector<float> &xs, std::vector<float> &squeeze) {
    const int count = std::clamp(mes.win_line_num, 0, MES_WIN_LINE_MAX);
    xs.assign(static_cast<size_t>(count), 0.0f);
    squeeze.assign(static_cast<size_t>(count), 1.0f);
    for (int i = 0; i < count; i++) {
        xs[static_cast<size_t>(i)] = static_cast<float>(mes.win_line[i].x);
    }
    if (mes.char_height <= 0 || mes.char_width <= 0) {
        return;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const ttffont::Metrics    metrics = ttffont::GetMetrics();
    const float               em = TtfEm(mes, metrics);
    const int                 em_px = static_cast<int>(em * mapping.scale_x + 0.5f);
    const textwipe::Tuning   &tune = textwipe::Get(textwipe::MessageKind());
    const float               gap = em * 0.1f + tune.letter;

    auto glyph_of = [&](int i, ttffont::Glyph &glyph) {
        const int      code = mes.win_line[i].code;
        const char32_t ch = code >= -0x2DF && code < -0x251 ? GameTextChar(static_cast<s16>(code)) : 0;
        return ch != 0 && ttffont::GetGlyph(ch, em_px, glyph);
    };
    auto cell_of = [&](int i) {
        const bool next = i + 1 < count && mes.win_line[i + 1].y == mes.win_line[i].y;
        return next ? static_cast<float>(mes.win_line[i + 1].x - mes.win_line[i].x) : static_cast<float>(mes.char_width);
    };

    int i = 0;
    int word = 0;
    int word_y = -1;
    while (i < count) {
        ttffont::Glyph glyph{};
        if (!glyph_of(i, glyph)) {
            i++;
            continue;
        }
        if (mes.win_line[i].y != word_y) {
            word_y = mes.win_line[i].y;
            word = 0;
        }
        int                last = i;
        float              natural = 0.0f;
        std::vector<float> pen;
        for (; last < count && mes.win_line[last].y == mes.win_line[i].y && glyph_of(last, glyph); last++) {
            pen.push_back(natural);
            natural += glyph.width / mapping.scale_x * tune.xscale + gap;
        }
        const float span = static_cast<float>(mes.win_line[last - 1].x - mes.win_line[i].x) + cell_of(last - 1);
        const float width = natural - gap;
        const bool  fit = tune.fit >= 0.5f;
        float       sx = fit ? std::clamp(span / std::max(width, 1.0f), kMinSqueeze, 1.0f) : 1.0f;
        float       extra = 0.0f;
        if (fit && width * sx < span && last - i > 1) {
            extra = std::min((span - width * sx) / static_cast<float>(last - i - 1), em * 0.25f);
        }
        const float used = width * sx + extra * static_cast<float>(last - i - 1);
        const float start = static_cast<float>(mes.win_line[i].x) + (fit ? (span - used) / 2 : (span - used) * tune.align) +
                            tune.word * static_cast<float>(word);
        word++;
        for (int k = i; k < last; k++) {
            xs[static_cast<size_t>(k)] = start + pen[static_cast<size_t>(k - i)] * sx + extra * static_cast<float>(k - i);
            squeeze[static_cast<size_t>(k)] = sx * tune.xscale;
        }
        if (tune.lock > 0.0f) {
            // Sprite centering preserves retail's narrow-character offsets and integer rounding.
            for (int k = i; k < last; k++) {
                ttffont::Glyph letter{};
                if (!glyph_of(k, letter)) {
                    continue;
                }
                const float ink = letter.width / mapping.scale_x * squeeze[static_cast<size_t>(k)];
                const auto &data = GaijiDataTbl[mes.win_line[k].code + 0x300];
                int         dx = (mes.char_width * data[kGaijiCells] - data[kGaijiWidth]) / 2 + data[kGaijiXOff];
                if (mes.narrow_gaiji && (data[kGaijiCode] == -0x2BD || data[kGaijiCode] == -0x2BA)) {
                    dx -= 3;
                }
                const float centred = static_cast<float>(mes.win_line[k].x + dx) +
                                      (static_cast<float>(data[kGaijiWidth]) - ink) / 2;
                float      &place = xs[static_cast<size_t>(k)];
                place += (centred - place) * std::min(1.0f, tune.lock);
            }
        }
        i = last;
    }
}

// Queues the font's glyph for win_line[index]: an edge pass per outline and one fill. False where
// the font has none, and the bitmap character is drawn.
bool QueueTtfGlyph(ClsMes &mes, int index, int dark, int offset_x, int offset_y, const std::vector<float> &xs,
                   const std::vector<float>   &squeeze,
                   std::vector<gfx::Vertex2D> &edges, std::vector<gfx::Vertex2D> &fills) {
    const MES_WIN_LINE &line = mes.win_line[index];
    const char32_t      ch = GameTextChar(line.code);
    if (ch == 0 || mes.char_height <= 0 || mes.char_width <= 0 || static_cast<size_t>(index) >= xs.size()) {
        return false;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const ttffont::Metrics    metrics = ttffont::GetMetrics();
    const float               em = TtfEm(mes, metrics);
    const int                 em_px = static_cast<int>(em * mapping.scale_x + 0.5f);
    ttffont::Glyph            glyph{};
    if (!ttffont::GetGlyph(ch, em_px, glyph)) {
        return false;
    }

    // The window's origin for this row, as the bitmap characters take it.
    const int row = line.y / mes.char_height;
    float     origin_x = 0.0f;
    float     cell_y = 0.0f;
    if (row < 0 || std::cmp_greater_equal(row, std::size(mes.line_pos)) || mes.line_pos[row].x < 0 ||
        mes.line_pos[row].y < 0) {
        origin_x = static_cast<float>(offset_x + mes.text_x);
        cell_y = static_cast<float>(offset_y + line.y + mes.text_y);
    } else {
        origin_x = static_cast<float>(mes.line_pos[row].x);
        // Explicit row positions omit the default bitmap path's three-pixel upward offset.
        cell_y = static_cast<float>(line.y + mes.line_pos[row].y - row * mes.char_height + 3);
    }

    // The font's line is centred in the cell's height. The bitmap's pixels are screen pixels, so a quad
    // is as many logical units as they come to.
    const textwipe::Tuning &tune = textwipe::Get(textwipe::MessageKind());
    const float             width = glyph.width / mapping.scale_x * squeeze[static_cast<size_t>(index)];
    const float             height = glyph.height / mapping.scale_y * rows * tune.yscale;
    const float             base = cell_y + (mes.char_height - em * (metrics.ascent + metrics.descent)) / 2 + em * metrics.ascent + kBaselineShift;
    float                   x = origin_x + xs[static_cast<size_t>(index)] + tune.dx;
    float                   y = ((base + tune.dy) * rows) + glyph.y_offset / mapping.scale_y * tune.yscale;

    // On whole pixels, so the edges of the strokes are not smeared across two.
    auto snap = [](float value, float scale, float offset) { return (std::round(value * scale + offset) - offset) / scale; };
    x = snap(x, mapping.scale_x, mapping.offset_x);
    y = snap(y, mapping.scale_y, mapping.offset_y);

    const int wide = ChoiceRowDim(mes, row, line.clut);
    int       r = 0;
    int       g = 0;
    int       b = 0;
    int       alpha = 0;
    GlyphTint(line.clut, dark, wide, r, g, b, alpha);

    // The outline and the drop shadow sit at the offsets the bitmaps' use: the shadow is part of the
    // look (it lifts the text off the picture), and at a fraction of the offset it vanishes under the glyph.
    const int cap = static_cast<int>(static_cast<float>(mes.edge_alpha) * tune.opacity);
    // A thin stroke's outline covers little of a pixel, so it is laid down nine times to read as dark as the bitmaps'.
    constexpr int passes = 9;
    auto          edge = [&](float dx, float dy, int er, int eg, int eb, int ea) {
        for (int pass = 0; pass < passes; ++pass) {
            TtfQuad(edges, glyph, x + dx, y + dy * rows, width, height, er, eg, eb, std::min(cap, ea));
        }
    };
    auto cast = [&] {
        // Retail's shadow falls off softly because its bitmaps are soft. A crisp glyph stacked at two
        // offsets reads as a hard outline instead, so it is laid in as taps the texture filter blends.
        TextShadowEach(ConfigGet().text_shadow, [&](const TextShadowTap &tap) {
            TtfQuad(edges, glyph, x + tap.dx, y + tap.dy * rows, width, height, tap.grey, tap.grey, tap.grey,
                    std::min(cap, static_cast<int>(static_cast<float>(tap.alpha) * tune.shadow)));
        });
    };
    switch (mes.style) {
        case MES_EDGE_WHITE:
            edge(1, 1, 0xFF, 0xFF, 0xFF, 0x40);
            break;
        case MES_EDGE_BLACK:
        case MES_EDGE_TABLE:
        case MES_EDGE_DOUBLE:
            cast();
            break;
        default:
            break;
    }
    if (tune.weight > 0.0f) {
        const float                               half = tune.weight / 2;
        const std::array<std::array<float, 2>, 4> shifts = {
            {{-half, 0.0f}, {half, 0.0f}, {0.0f, -half}, {0.0f, half}}
        };
        for (const auto &shift : shifts) {
            TtfQuad(fills, glyph, x + shift[0], y + shift[1] * rows, width, height, r, g, b, std::min(cap, alpha));
        }
    }
    TtfQuad(fills, glyph, x, y, width, height, r, g, b, std::min(cap, alpha));
    return true;
}

} // namespace

// The pad buttons among the font's icons (kNamed in gametext.cpp), drawn from the button symbol
// files (platform/glyphs.hpp) when input.glyphs is "new". The symbol keeps the proportions of its
// art, scaled so a round face button fills the icon's height, and sits in the middle of the icon's
// slot; a wide one (a key) may reach out of it, up to twice the slot's width.
namespace {

bool DrawPadGlyph(int code, const CRect_i_ &screen, int alpha) {
    glyphs::Button button{};
    switch (code) {
        case -0x300:
            button = glyphs::Button::Select;
            break;
        case -0x2FF:
            button = glyphs::Button::Start;
            break;
        case -0x2FE:
            button = glyphs::Button::L1;
            break;
        case -0x2FD:
            button = glyphs::Button::R1;
            break;
        case -0x2FC:
            button = glyphs::Button::L2;
            break;
        case -0x2FB:
            button = glyphs::Button::R2;
            break;
        case -0x2FA:
            button = glyphs::Button::Circle;
            break;
        case -0x2F9:
            button = glyphs::Button::Triangle;
            break;
        case -0x2F8:
            button = glyphs::Button::Cross;
            break;
        case -0x2F7:
            button = glyphs::Button::Square;
            break;
        case -0x2F6:
            button = glyphs::Button::Dpad;
            break;
        case -0x2F5:
            button = glyphs::Button::DpadUpDown;
            break;
        case -0x2F4:
            button = glyphs::Button::DpadSides;
            break;
        default:
            return false;
    }
    return GlyphDrawSlot(button, screen, alpha);
}

} // namespace

// A colour is read only for a palette entry FontColorTbl has; the red text's 0xFF, past the table,
// sets its own.
void ClsMes::DrawGaijiFont(CTexture *texture, int index, const CRect_i_ &texel, const CRect_i_ &screen,
                           int wide, int dark) {
    u8  r = 0;
    u8  g = 0;
    u8  b = 0;
    u8  alpha = 0;
    int u = 0;
    int v = 0;

    const int code = this->win_line[index].code;

    if (code >= -0x300 && code < -0x2DF) {
        if (DrawPadGlyph(code, screen, std::min(this->edge_alpha, 0x80))) {
            return;
        }
        set2DSprite_Core(Vif1Packet, texture, screen, texel, 0x80, 0x80, 0x80,
                         std::min(this->edge_alpha, 0x80));
        return;
    }

    const u8 clut = this->win_line[index].clut;

    switch (clut < std::size(FontColorTbl) ? FontColorTbl[clut] : 0) {
        case FONT_COLOR_WHITE:
            r = 0x5F;
            g = 0x5F;
            b = 0x5F;
            alpha = 0x80;
            break;

        case FONT_COLOR_BROWN:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            alpha = 0x80;
            break;

        case FONT_COLOR_YELLOW:
            r = 0x5E;
            g = 0x5E;
            b = 0x20;
            alpha = 0x80;
            break;

        case FONT_COLOR_CYAN:
            r = 0x20;
            g = 0x5E;
            b = 0x5E;
            alpha = 0x80;
            break;

        case FONT_COLOR_GREEN:
            r = 0x20;
            g = 0x5E;
            b = 0x20;
            alpha = 0x80;
            break;

        case FONT_COLOR_BROWN_OPAQUE:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            alpha = 0x80;
            break;

        case FONT_COLOR_GOLD:
            r = 0x73;
            g = 0x67;
            b = 0x33;
            alpha = 0x80;
            break;

        case FONT_COLOR_GREY:
            r = 0x47;
            g = 0x47;
            b = 0x47;
            alpha = 0x80;
            break;

        case FONT_COLOR_MAGENTA:
            r = 0x5F;
            g = 0x1F;
            b = 0x5F;
            alpha = 0x80;
            break;

        default:
            r = 0x28;
            g = 0x28;
            b = 0x20;
            alpha = 0x80;
            break;
    }

    if (clut == 0xFF) {
        if (dark != 0) {
            alpha = 0x80;
            b = 0x22;
            g = 0x22;
            r = 0x7F;
        } else {
            alpha = 0x80;
            b = 0x11;
            g = 0x11;
            r = 0x40;
        }
    }

    if (wide == 1) {
        r = r * 2;
        g = g * 2;
        b = b * 2;
    }

    if (wide == 2) {
        alpha /= 2;
    }

    if (g_letters != nullptr && FontLetter(texel)) {
        LetterQuad(texel, static_cast<float>(screen.x), static_cast<float>(screen.y), static_cast<float>(screen.width),
                   static_cast<float>(screen.height), r, g, b, std::min(this->edge_alpha, static_cast<int>(alpha)));
    } else {
        set2DSprite_Core(Vif1Packet, texture, screen, texel, r, g, b, std::min(this->edge_alpha, static_cast<int>(alpha)));
    }

    switch (this->win_line[index].code) {
        case -0x284:
        case -0x280:
        case -0x27F:
        case -0x27A:
        case -0x275:
        case -0x271:
        case -0x26C:
        case -0x268:
        case -0x25E:
        case -0x258:
            u = 0xE2;
            v = 0xB4;
            break;

        case -0x282:
        case -0x27D:
        case -0x278:
        case -0x274:
        case -0x26F:
        case -0x26B:
        case -0x260:
        case -0x25C:
        case -0x25A:
        case -0x256:
            u = 0xF0;
            v = 0xB4;
            break;

        case -0x281:
        case -0x27C:
        case -0x277:
        case -0x273:
        case -0x26E:
        case -0x26A:
        case -0x267:
        case -0x266:
        case -0x264:
        case -0x25D:
            u = 0x80;
            v = 0xC8;
            break;

        case -0x27B:
        case -0x276:
        case -0x272:
        case -0x26D:
        case -0x269:
        case -0x25F:
        case -0x25B:
        case -0x259:
        case -0x257:
        case -0x255:
            u = 0x9C;
            v = 0xC8;
            break;

        case -0x270:
        case -0x254:
            u = 0x8E;
            v = 0xC8;
            break;

        default:
            return;
    }

    if (this->narrow_gaiji_set != 1 && (this->narrow_gaiji_set == 2 || this->char_width < 0xB)) {
        u += 0x80;
    }

    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen.x, screen.y, screen.width, screen.height, u, v, 14, 20);
    set2DSprite_Core(Vif1Packet, texture, screen, CRect_i_(u, v, 14, 20), r, g, b,
                     std::min(this->edge_alpha, static_cast<int>(alpha)));
}

void ClsMes::DrawMesWin() {
    if (this->mes_made < 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, this->tex_block);

    CTexture *texture = nullptr;
    int       offset_x = 0;

    if (this->fukidashi != 0 && this->text_columns < 4) {
        offset_x = (4 - this->text_columns) * this->char_width >> 1;
    }

    int offset_y = 0;

    if (this->fukidashi == 0 && this->centre_rows != 0) {
        offset_y = this->char_height * (this->rows - this->text_rows) >> 1;
    }

    if (this->fukidashi != 0) {
        this->MakeFukidashi(Vif1Packet);

        texture = draw2d::Get().find_texture("fukidashibase");
        setbilinear(1);
        this->DrawMesWin_sub(texture, -1, -1, 0);
        this->DrawMesWin_sub(texture, 1, -1, 0);
        this->DrawMesWin_sub(texture, -1, 1, 0);
        this->DrawMesWin_sub(texture, 1, 1, 0);
        this->DrawMesWin_sub(texture, 5, 5, 0);
        this->DrawMesWin_sub(texture, 0, 0, 1);

        if (this->fade_in != 0 && this->fade < 1.0f) {
            return;
        }
    } else {
        this->fade = 1.0f;

        if (this->stay_frame != 0) {
            int width = this->char_width * this->text_columns + 0x20;
            int height = this->char_height * this->text_rows + 0x1C;

            if (0 < this->stay_width) {
                width = this->stay_width;
            }

            if (0 < this->stay_height) {
                height = this->stay_height;
            }

            int px = this->text_x + offset_x - 0x10;
            int py = this->text_y + offset_y - 0xE;

            if (0 < this->auto_pos) {
                GetPos_AbsPosSet(0x10, 0x10, 0x260, 0x1A0, width, height, this->auto_pos, &px, &py);
                this->text_x = px - offset_x + 0x10;
                this->text_y = py - offset_y + 0xE;
            }

            if (MesAbsDrawOff == 0) {
                MyMenuHelpWinDraw(px, py, width, height, 0x80, 0, 0, draw2d::Get().find_texture("stayframe"));
            }
        }
    }

    this->MyTextureMake();

    texture = draw2d::Get().find_texture("fontbase");

    if (this->clut == nullptr) {
        return;
    }

    int dark = 0;
    if ((this->blink / 0x1C) % 2 != 0) {
        this->clut[100] = 0x80222280;
        dark = 0;
    } else {
        this->clut[100] = 0x804444FF;
        dark = 1;
    }

    SetClut(Vif1Packet, texture, reinterpret_cast<i *>(this->clut));
    this->blink++;

    texture = draw2d::Get().find_texture("gaiji");
    setbilinear(0);
    // The letters drawn from Chronicle Sharp, where both its fonts are there, in place of the font drawn in the TrueType
    // path's cells.
    const bool letters_ready = LettersReady();
    auto       text_pass = [&](bool ttf) {
        std::vector<PendingLetter> letters;
        g_letters = letters_ready ? &letters : nullptr;
        set2DSprite_Start(Vif1Packet, texture);
        std::vector<gfx::Vertex2D> ttf_edges;
        std::vector<gfx::Vertex2D> ttf_fills;
        std::vector<float>         ttf_xs;
        std::vector<float>         ttf_squeeze;
        if (ttf) {
            TtfLayoutRows(*this, ttf_xs, ttf_squeeze);
        }

        const int line_count = std::clamp(this->win_line_num, 0, MES_WIN_LINE_MAX);
        const int first_line = std::clamp(this->text_from, 0, line_count);
        const int end_line = std::clamp(this->text_no, first_line, line_count);
        for (int index = first_line; index < end_line; index++) {
            if (MesAbsDrawOff != 0) {
                continue;
            }

            int code = this->win_line[index].code;

            if (code < -0x300 || code >= -0x251) {
                continue;
            }

            if (ttf && code >= -0x2DF && QueueTtfGlyph(*this, index, dark, offset_x, offset_y, ttf_xs, ttf_squeeze, ttf_edges, ttf_fills)) {
                continue;
            }

            int gaiji_index = code + 0x300;
            int u = GaijiDataTbl[gaiji_index][kGaijiU];

            if (code >= -0x2DF && code < -0x251 && this->narrow_gaiji_set != 1 &&
                (this->narrow_gaiji_set == 2 || this->char_width < 0xB)) {
                u += 0x80;
            }

            int v = GaijiDataTbl[gaiji_index][kGaijiV];
            int glyph_width = GaijiDataTbl[gaiji_index][kGaijiWidth];
            int glyph_height = GaijiDataTbl[gaiji_index][kGaijiHeight];
            int dx = (this->char_width * GaijiDataTbl[gaiji_index][kGaijiCells] - glyph_width) / 2;
            int dy = (this->char_height - glyph_height) / 2;

            dx += GaijiDataTbl[gaiji_index][kGaijiXOff];

            if (this->narrow_gaiji != 0 && (GaijiDataTbl[gaiji_index][kGaijiCode] == -0x2BD ||
                                            GaijiDataTbl[gaiji_index][kGaijiCode] == -0x2BA)) {
                dx -= 3;
            }

            dy += GaijiDataTbl[gaiji_index][kGaijiYOff];

            if (this->char_height == 0x14) {
                dy += 1;
            }

            int row = this->win_line[index].y / this->char_height;

            if (row < 0 || std::cmp_greater_equal(row, std::size(this->line_pos)) || this->line_pos[row].x < 0 ||
                this->line_pos[row].y < 0) {
                int      line_x = this->win_line[index].x + this->text_x;
                int      line_y = this->win_line[index].y + this->text_y;
                int      screen_x = offset_x + (line_x + dx);
                CRect_i_ screen;
                screen.x = screen_x;
                int screen_y = offset_y + (line_y + dy) - 3;
                screen.y = screen_y;
                screen.width = glyph_width;
                screen.height = glyph_height;
                CRect_i_ texel(u, v, glyph_width, glyph_height);

                if (this->win_line[index].code >= -0x2DF && !LetterFuchi(*this, screen, texel)) {
                    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x, screen_y, glyph_width, glyph_height,
                                              u, v, glyph_width, glyph_height);
                }

                this->DrawGaijiFont(texture, index, texel, screen, ChoiceRowDim(*this, row, this->win_line[index].clut), dark);
            } else {
                int screen_x = this->win_line[index].x + this->line_pos[row].x;
                int screen_y = this->win_line[index].y + this->line_pos[row].y;
                screen_y += dy;
                CRect_i_ screen(screen_x + dx, screen_y - row * this->char_height, glyph_width, glyph_height);
                CRect_i_ texel(u, v, glyph_width, glyph_height);

                if (this->win_line[index].code >= -0x2DF && !LetterFuchi(*this, screen, texel)) {
                    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x + dx,
                                              screen_y - row * this->char_height, glyph_width, glyph_height, u, v,
                                              glyph_width, glyph_height);
                }

                this->DrawGaijiFont(texture, index, texel, screen, ChoiceRowDim(*this, row, this->win_line[index].clut), dark);
            }
        }

        set2DSprite_End(Vif1Packet, texture);
        g_letters = nullptr;
        DrawLetters(letters);

        if (!ttf_fills.empty()) {
            const gfx::TextureBinding binding = ttffont::Binding();
            if (!ttf_edges.empty()) {
                textwipe::Draw2D(gfx::Primitive::Quads, ttf_edges, binding, draw2d::SpriteState());
            }
            textwipe::Draw2D(gfx::Primitive::Quads, ttf_fills, binding, draw2d::SpriteState());
            draw2d::RestoreTestZbuf();
        }
    };
    if (textwipe::Active() && !letters_ready && TtfReady()) {
        textwipe::Both(text_pass);
    } else {
        text_pass(!letters_ready && TtfReady());
    }

    if (this->page_arrow != 0 &&
        ((this->text_no >= this->text_len && this->cursor_row < 0) || this->waiting != 0) &&
        (this->blink >> 4) % 2 != 0) {
        texture = draw2d::Get().find_texture("syst04");

        int screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8;
        int screen_y = this->text_y + this->char_height * this->text_rows;

        CRect_i_ screen(screen_x + offset_x, screen_y + offset_y, 0x10, 0x10);
        CRect_i_ texel(0, 0, 0x10, 0x10);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel, std::min(this->edge_alpha, 0x80));
        }
    }

    if (this->end_mark != 0) {
        texture = draw2d::Get().find_texture("syst04");

        int screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8;
        int screen_y = this->text_y + this->char_height * this->text_rows;

        CRect_i_ screen(screen_x + offset_x, screen_y + offset_y, 0x10, 0x10);
        CRect_i_ texel(0, 0, 0x10, 0x10);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel,
                        std::min(this->edge_alpha, 0x80));
        }
    }

    // The dialog framework's choice window, its choices running on past it: the page arrow, blinking as
    // it does, where the page arrow goes for more below, and turned over above the first choice for
    // more above (ClsMesSetChoiceScroll).
    if (this == g_choice_mes && this->cursor_row >= 0 && (g_choice_more_above || g_choice_more_below) &&
        (this->blink >> 4) % 2 != 0 && MesAbsDrawOff == 0) {
        texture = draw2d::Get().find_texture("syst04");

        int       screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8 + offset_x;
        const int alpha = std::min(this->edge_alpha, 0x80);

        if (g_choice_more_above) {
            int screen_y = this->text_y + this->char_height * g_choice_first_row - 0x10;
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y + offset_y, 0x10, 0x10),
                        CRect_i_(0, 0x10, 0x10, -0x10), alpha);
        }

        if (g_choice_more_below) {
            int screen_y = this->text_y + this->char_height * this->text_rows;
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y + offset_y, 0x10, 0x10),
                        CRect_i_(0, 0, 0x10, 0x10), alpha);
        }
    }

    if (this->cursor_row >= 0) {
        texture = draw2d::Get().find_texture("gaiji");

        // The cursor stands two characters further left, clear of the choice text.
        int screen_x = this->text_x;
        screen_x -= 0xC;
        screen_x -= this->char_width * 2;

        this->cursor_y = (this->cursor_y + this->char_height * this->cursor_row) / 2;

        int screen_y = this->cursor_y + this->text_y;

        CRect_i_ screen(screen_x + offset_x, screen_y - 4 + offset_y, 0x20, 0x20);
        CRect_i_ texel(0x60, 0x60, 0x20, 0x20);

        // The dialog framework's window: the hand is at the mouse pointer while one is in use.
        float pointer_x = 0.0f;
        float pointer_y = 0.0f;
        if (this == g_choice_mes && MenuPointerHand(pointer_x, pointer_y)) {
            screen = CRect_i_(static_cast<int>(pointer_x), static_cast<int>(pointer_y), 0x20, 0x20);
        }

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel,
                        std::min(this->edge_alpha, 0x80));
        }
    } else {
        this->cursor_y = 0;
    }
}

void ClsMesSetChoiceRows(const ClsMes *mes, int first_row) {
    g_choice_mes = mes;
    g_choice_first_row = mes != nullptr && first_row > 0 ? first_row : 0;
    if (mes == nullptr) {
        g_choice_more_above = false;
        g_choice_more_below = false;
    }
}

void ClsMesSetChoiceScroll(bool more_above, bool more_below) {
    g_choice_more_above = more_above;
    g_choice_more_below = more_below;
}

void ClsMesSetMessageOverlay(const ClsMes *mes, short *file) {
    g_overlay_mes = mes != nullptr && file != nullptr ? mes : nullptr;
    g_overlay_file = g_overlay_mes != nullptr ? file : nullptr;
}

} // namespace dc::game
