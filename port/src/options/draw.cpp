#include <algorithm>
#include <cmath>

#include "clsmes.hpp"
#include "gameutil.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "options/screen.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "text/textwipe.hpp"
#include "texture.hpp"
#include "ui/framework.hpp"

namespace dc::options {

using namespace dc::game;
using namespace dc::platform;
using namespace dc::text;

namespace {

constexpr int bar_fade = 15;

ui::Cursor    s_cursor;
ui::Brackets  s_brackets;
ui::ScrollBar s_scrollbar;

void DrawSprite(int x, int y, int u, int v, int width, int height, int alpha) {
    DrawMenu2DSprite(g_screen.texture, CRect_i_(x, y, width, height), CRect_i_(u, v, width, height), alpha);
}

void DrawTabPlate(int alpha) {
    int left = L1X();
    int right = R1X() + GetTexts().r1.GetWidth();
    ui::TabPlate::Draw(left, right, kTabY, alpha);
}

void DrawScrollBar(int alpha) {
    if (!Scrolls(g_screen.page) || g_screen.bar_frames <= 0) {
        return;
    }
    int bar_alpha = alpha * std::min(g_screen.bar_frames, bar_fade) / bar_fade;
    int rows = RowCount(g_screen.page);
    MenuTextureReload(g_screen.block_no);
    s_scrollbar.Draw(kBarX, kRowY, kVisibleRows * kRowStep, rows, kVisibleRows, FirstRow(), bar_alpha);
}

void DrawSelection(int alpha) {
    Texts &texts = GetTexts();
    Glyph  glyph = g_screen.pointing ? g_screen.glyph : Glyph::None;
    int    rows = RowCount(g_screen.page);

    if (g_screen.pointing) {
        s_cursor.SetPointer(g_screen.pointer_x, g_screen.pointer_y);
    } else {
        s_cursor.ClearPointer();
    }

    MenuTextureReload(g_screen.block_no);

    if (glyph == Glyph::L1 || glyph == Glyph::R1) {
        int x = glyph == Glyph::L1 ? L1X() : R1X();
        int w = (glyph == Glyph::L1 ? texts.l1 : texts.r1).GetWidth();
        s_brackets.Draw(x - 8, x + w + 8, kTabY - 4, kTabY + 22, alpha);
        s_cursor.SetTarget(static_cast<float>(x - 38), static_cast<float>(kTabY + 9));
    } else if (glyph != Glyph::None) {
        int y = glyph == Glyph::Up ? kRowY - 4 : kRowY + kVisibleRows * kRowStep - 16;
        s_brackets.Draw(kBarX - 10, kBarX + 18, y - 6, y + 16, alpha);
        s_cursor.SetTarget(static_cast<float>(kBarX - 38), static_cast<float>(y + 4));
    } else if (g_screen.row == kOnTabs) {
        int x = TabX(g_screen.page);
        int w = texts.tabs[g_screen.page].GetWidth();
        s_brackets.Draw(x - 8, x + w + 8, kTabY - 4, kTabY + 22, alpha);
        s_cursor.SetTarget(static_cast<float>(L1X() - 44), static_cast<float>(kTabY + 9));
    } else if (g_screen.row < rows) {
        const Row    &row = CurrentPage().rows[g_screen.row];
        const Config &config = ConfigGet();

        std::string val_str;
        if (row.activate == nullptr) {
            val_str = (row.action != nullptr && g_screen.binding_row == g_screen.row)
                          ? g_screen.binding_prompt
                          : RowValue(row, config);
        }

        int val_w = ui::Label(val_str).GetWidth();
        int val_center = (kValueX + kValueRight) / 2;
        int val_left = val_center - (val_w / 2);
        int val_right = val_left + val_w;

        int  choice = row.get(config);
        int  count = row.count(config);
        bool has_prev = (choice > 0);
        bool has_next = (choice < count - 1);

        int arrow_pad = 12;
        int arrow_w = texts.left.GetWidth();
        int left_arrow_x = val_left - arrow_pad - arrow_w;
        int right_arrow_x = val_right + arrow_pad + arrow_w;

        int b_left = (has_prev && row.action == nullptr && row.activate == nullptr) ? left_arrow_x : (val_left - 14);
        int b_right = (has_next && row.action == nullptr && row.activate == nullptr) ? right_arrow_x : (val_right + 14);

        int pad_x = 18;
        int box_left = b_left - pad_x;
        int box_right = b_right + pad_x;

        constexpr int minimum_box_width = 120;
        int           cur_box_w = box_right - box_left;
        if (cur_box_w < minimum_box_width) {
            int extra = (minimum_box_width - cur_box_w) / 2;
            box_left -= extra;
            box_right += extra;
        }

        int row_top_y = RowTop(g_screen.row);
        int top_y = row_top_y + 1;
        int bottom_y = row_top_y + 23;

        s_brackets.Draw(box_left, box_right, top_y, bottom_y, alpha);
        s_cursor.SetTarget(static_cast<float>(kLabelX - 44), static_cast<float>(row_top_y + 9));
    } else {
        s_brackets.Draw(kExitX - 6, kExitX + kExitWidth + 6, kExitY - 4, kExitY + kExitHeight + 4, alpha);
        s_cursor.SetTarget(static_cast<float>(kExitX - 38), static_cast<float>(kExitY + 9));
    }

    s_cursor.Draw(alpha);
}

void DrawTabs(int alpha) {
    Texts &texts = GetTexts();
    int    last = LastVisibleTab();
    Glyph  glyph = g_screen.pointing ? g_screen.glyph : Glyph::None;

    texts.l1.Draw(L1X(), kTabY, alpha);
    texts.r1.Draw(R1X(), kTabY, alpha);

    bool has_left_tabs = (g_screen.first_tab > 0);
    bool has_right_tabs = (last < static_cast<int>(Pages().size()) - 1);

    int active_dir = g_screen.last_tab_dir;
    if (glyph == Glyph::L1) {
        active_dir = -1;
    }
    if (glyph == Glyph::R1) {
        active_dir = 1;
    }
    if (active_dir < 0 && !has_left_tabs) {
        active_dir = 1;
    }
    if (active_dir > 0 && !has_right_tabs) {
        active_dir = -1;
    }

    if (has_left_tabs) {
        texts.left.SetColor(active_dir < 0 ? ui::ColorYellow : ui::ColorWhite);
        texts.left.Draw(kLabelX, kTabY, alpha / 2);
    }
    if (has_right_tabs) {
        texts.right.SetColor(active_dir > 0 ? ui::ColorYellow : ui::ColorWhite);
        texts.right.Draw(kTabsRight - texts.right.GetWidth(), kTabY, alpha / 2);
    }

    for (int t = g_screen.first_tab; t <= last; ++t) {
        bool shown = t == g_screen.page;
        texts.tabs[t].SetColor(shown ? ui::ColorGold : ui::ColorWhite);
        texts.tabs[t].Draw(TabX(t), kTabY, shown ? alpha : alpha / 2);
    }
}

void DrawRows(int alpha) {
    const Config &config = ConfigGet();
    Texts        &texts = GetTexts();
    const Page   &page = CurrentPage();
    int           last = std::min(RowCount(g_screen.page), FirstRow() + kVisibleRows);

    for (int r = FirstRow(); r < last; ++r) {
        const Row &row = page.rows[r];
        int        y = RowTop(r) + 2;
        bool       selected = (g_screen.row == r);

        ui::Label &label = texts.labels[g_screen.page][r];
        label.Draw(kLabelX, y, alpha);

        std::string val_str;
        if (row.activate == nullptr) {
            if (row.action != nullptr && g_screen.binding_row == r) {
                val_str = g_screen.binding_prompt;
            } else {
                val_str = RowValue(row, config);
            }
        }

        if (!val_str.empty()) {
            ui::Label val_lbl(val_str, selected ? ui::ColorYellow : ui::ColorWhite, ui::Alignment::Center);
            int       val_w = val_lbl.GetWidth();
            int       val_center = (kValueX + kValueRight) / 2;
            int       val_left = val_center - (val_w / 2);
            int       val_right = val_left + val_w;

            val_lbl.Draw(val_center, y, alpha);

            if (selected && row.action == nullptr && row.activate == nullptr) {
                int choice = row.get(config);
                int count = row.count(config);
                int arrow_pad = 12;

                bool has_prev = (choice > 0);
                bool has_next = (choice < count - 1);

                int active_dir = g_screen.last_row_dir;
                if (g_screen.pointing && g_screen.pointer_x >= kValueX - 8) {
                    active_dir = (g_screen.pointer_x < val_center) ? -1 : 1;
                }
                if (active_dir < 0 && !has_prev) {
                    active_dir = 1;
                }
                if (active_dir > 0 && !has_next) {
                    active_dir = -1;
                }

                if (has_prev) {
                    texts.left.SetColor(active_dir < 0 ? ui::ColorYellow : ui::ColorWhite);
                    texts.left.Draw(val_left - arrow_pad - texts.left.GetWidth(), y, alpha);
                }
                if (has_next) {
                    texts.right.SetColor(active_dir > 0 ? ui::ColorYellow : ui::ColorWhite);
                    texts.right.Draw(val_right + arrow_pad, y, alpha);
                }
            }
        }
    }
}

void DrawHelp(int alpha) {
    float win_x = 0.0f;
    float win_y = 0.0f;
    float win_w = 0.0f;
    float win_h = 0.0f;
    int   text_x = 0;
    int   text_y = 0;
    GetMainMenuRightHelpWinLangOffset(win_x, win_y, win_w, win_h);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);

    if (win_w <= 0.0f || win_h <= 0.0f) {
        win_x = 360.0f;
        win_y = 348.0f;
        win_w = 14.5f;
        win_h = 3.5f;
        text_x = 22;
        text_y = 12;
    }

    MenuHelpWinDraw(static_cast<int>(win_x), static_cast<int>(win_y), win_w, win_h, alpha);
    CommonMenuMes2.edge_alpha = alpha;
    MenuTextureReload(CommonMenuMes2.tex_block);
    DrawMenuClsMes(&CommonMenuMes2, static_cast<int>(win_x + text_x), static_cast<int>(win_y + text_y));
}

} // namespace

void Draw() {
    const textwipe::MessageContext fit(textwipe::Kind::Options);
    setbilinear(0);

    if (!g_screen.texture_ready) {
        return;
    }

    MenuTextureReload(g_screen.block_no);
    int alpha = 0x80;

    switch (g_screen.step) {
        case OPTION_STEP_FADE_IN:
            alpha = g_screen.step_count * 7;
            break;
        case OPTION_STEP_FADE_OUT:
            alpha = 0x80 - g_screen.step_count * 7;
            break;
        default:
            break;
    }
    alpha = std::clamp(alpha, 0, 0x80);

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        DrawSprite(0x50, 0x28, 0xB3, 0x118, 0xAA, 0x28, alpha);
    }

    DrawTabPlate(alpha);
    DrawScrollBar(alpha);

    MenuTextureReload(g_screen.block_no);
    ui::TextureButton::Draw(
        ui::Rect(kExitX, kExitY, kExitWidth, kExitHeight),
        g_screen.texture,
        ui::Rect(452, 224, kExitWidth, kExitHeight),
        alpha);

    if (g_screen.step == OPTION_STEP_RUN) {
        g_screen.step_count = 0;
    } else {
        g_screen.step_count++;
    }

    DrawHelp(alpha);

    MenuTextureReload(0x1A);
    GetTexts().shortcuts.Draw(kExitX, kHelpY, alpha);
    DrawTabs(alpha);
    DrawRows(alpha);

    if (g_screen.step == OPTION_STEP_RUN) {
        DrawSelection(alpha);
    }

    setbilinear(1);
}

} // namespace dc::options
