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
#include "texture.hpp"

namespace options {

namespace {

// A help window's frame round its middle (MenuHelpWinDraw): 24 pixels a side and 22 above and below.
constexpr int kPlateSide = 24;
constexpr int kPlateHeight = 44;

constexpr int kBarFade = 15;

constexpr int   kBracketBeats = 30;
constexpr float kBracketClose = 4.0f;

void DrawSprite(int x, int y, int u, int v, int width, int height, int alpha) {
    DrawMenu2DSprite(g_screen.texture, CRect_i_(x, y, width, height), CRect_i_(u, v, width, height), alpha);
}

void DrawTabPlate(int alpha) {
    int   left = L1X();
    int   right = R1X() + GetTexts().r1.Width();
    float middle = static_cast<float>(right - left + 2 * 14 - 2 * kPlateSide) / 16.0f;
    MenuHelpWinDraw(left - 14, kTabY - (kPlateHeight - 20) / 2, middle, 0.0f, alpha);
}

// The help window's fill as the track, retail's screen-adjust arrows at its ends and option.pac's
// stone as the thumb.
void DrawScrollBar(int alpha) {
    if (!Scrolls(g_screen.page) || g_screen.bar_frames <= 0) {
        return;
    }
    alpha = alpha * std::min(g_screen.bar_frames, kBarFade) / kBarFade;
    int rows = RowCount(g_screen.page);
    int top = kRowY + 14;
    int height = kVisibleRows * kRowStep - 32;
    int thumb = std::max(12, height * kVisibleRows / rows);
    int thumb_y = top + (height - thumb) * FirstRow() / (rows - kVisibleRows);
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    DrawMenu2DSprite(StayTex, CRect_i_(kBarX, top, 8, height), CRect_i_(22, 22, 16, 20), alpha);
    MenuTextureReload(g_screen.block_no);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX, thumb_y, 8, thumb), CRect_i_(140, 280, 24, 32), alpha);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX - 4, kRowY - 4, 16, 16), CRect_i_(452, 136, 24, 24), alpha);
    DrawMenu2DSprite(g_screen.texture, CRect_i_(kBarX - 4, top + height + 2, 16, 16), CRect_i_(452, 164, 24, 24),
                     alpha);
}

// Retail's bracket corners round the chosen part, and its bobbing hand left of them or at the pointer.
void DrawCursor(int left, int right, int top, int alpha, int hand_left = -1) {
    static int bracket_count = 0;
    static int hand_count = 0;
    float      pulse = kBracketClose * static_cast<float>(bracket_count) / static_cast<float>(kBracketBeats - 1);
    int        x0 = static_cast<int>(left + pulse);
    int        x1 = static_cast<int>(right - pulse);
    int        y0 = static_cast<int>(top + pulse);
    int        y1 = static_cast<int>(top + 26 - pulse);
    DrawSprite(x0, y0, 0xB2, 0xF8, 16, 16, alpha);
    DrawSprite(x1, y0, 0xC2, 0xF8, 16, 16, alpha);
    DrawSprite(x0, y1, 0xB2, 0x108, 16, 16, alpha);
    DrawSprite(x1, y1, 0xC2, 0x108, 16, 16, alpha);
    bracket_count = (bracket_count + 1) % kBracketBeats;

    float target_x = static_cast<float>(hand_left >= 0 ? hand_left : left - 38);
    float target_y = static_cast<float>(top + 9);
    g_screen.cursor_x += (target_x - g_screen.cursor_x) / 4.0f;
    g_screen.cursor_y += (target_y - g_screen.cursor_y) / 4.0f;
    float hand_x = g_screen.cursor_x + 7.0f * cosf(0.0805536583f * hand_count);
    float hand_y = g_screen.cursor_y + 5.0f * sinf(0.116355285f * hand_count);
    if (g_screen.pointing) {
        hand_x = g_screen.pointer_x - 26.0f;
        hand_y = g_screen.pointer_y - 14.0f;
    }
    CRect_i_ hand(0xD2, 0xF8, 0x20, 0x20);
    DrawMenu2DSprite(g_screen.texture, CRect_i_((int) (5.0f + hand_x), (int) (3.0f + hand_y), 0x20, 0x20), hand, 0, 0, 0,
                     (alpha * 100) >> 7);
    DrawMenu2DSprite(g_screen.texture, CRect_i_((int) hand_x, (int) hand_y, 0x20, 0x20), hand, alpha);
    hand_count = (hand_count + 1) % 0x107AC0;
}

void DrawSelection(int alpha) {
    Texts &texts = GetTexts();
    Glyph  glyph = g_screen.pointing ? g_screen.glyph : Glyph::None;
    int    rows = RowCount(g_screen.page);
    if (glyph == Glyph::L1 || glyph == Glyph::R1) {
        int x = glyph == Glyph::L1 ? L1X() : R1X();
        DrawCursor(x - 10, x + (glyph == Glyph::L1 ? texts.l1 : texts.r1).Width() - 6, kTabY - 6, alpha);
    } else if (glyph != Glyph::None) {
        int y = glyph == Glyph::Up ? kRowY - 4 : kRowY + kVisibleRows * kRowStep - 16;
        DrawCursor(kBarX - 12, kBarX + 4, y - 13, alpha);
    } else if (g_screen.row == kOnTabs) {
        int x = TabX(g_screen.page);
        DrawCursor(x - 10, x + texts.tabs[g_screen.page].Width() - 6, kTabY - 6, alpha, L1X() - 44);
    } else if (g_screen.row < rows) {
        DrawCursor(kValueX - 22, kValueRight - 2, RowTop(g_screen.row) - 7, alpha, kLabelX - 48);
    } else {
        DrawCursor(kExitX - 8, kExitX + kExitWidth - 8, kExitY - 4, alpha);
    }
}

void DrawTabs(int alpha) {
    Texts &texts = GetTexts();
    int    last = LastVisibleTab();
    texts.l1.Draw(L1X(), kTabY, alpha);
    texts.r1.Draw(R1X(), kTabY, alpha);
    if (g_screen.first_tab > 0) {
        texts.left.Draw(kLabelX, kTabY, alpha / 2);
    }
    if (last < static_cast<int>(Pages().size()) - 1) {
        texts.right.Draw(kTabsRight - texts.right.Width(), kTabY, alpha / 2);
    }
    for (int t = g_screen.first_tab; t <= last; ++t) {
        bool shown = t == g_screen.page;
        texts.tabs[t].SetColour(shown ? FONT_COLOR_GOLD : FONT_COLOR_WHITE);
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
        bool       selected = g_screen.row == r;
        GameText &label = texts.labels[g_screen.page][r];
        label.RefreshGlyphs();
        label.Draw(kLabelX, y, alpha);
        GameText &value = texts.values[g_screen.page][r];
        if (row.action != nullptr && g_screen.binding_row == r) {
            value.Set(g_screen.binding_prompt);
        } else {
            value.Set(RowValue(row, config));
        }
        value.RefreshGlyphs();
        value.FitWidth(kValueRight - kValueX - 32);
        value.SetColour(selected ? FONT_COLOR_YELLOW : FONT_COLOR_WHITE);
        value.Draw((kValueX + kValueRight - value.Width()) / 2, y, alpha);
        if (selected && row.action == nullptr) {
            int choice = row.get(config);
            if (choice > 0) {
                texts.left.Draw(kValueX + 8, y, alpha);
            }
            if (choice < row.count(config) - 1) {
                texts.right.Draw(kValueRight - texts.right.Width(), y, alpha);
            }
        }
    }
}

void DrawTitleHelp(int alpha) {
    float win_x;
    float win_y;
    float win_w;
    float win_h;
    int   text_x;
    int   text_y;
    GetMainMenuRightHelpWinLangOffset(win_x, win_y, win_w, win_h);
    MenuHelpWinDraw((int) win_x, (int) win_y, win_w, win_h, alpha);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
    CommonMenuMes2.edge_alpha = alpha;
    MenuTextureReload(CommonMenuMes2.tex_block);
    DrawMenuClsMes(&CommonMenuMes2, (int) (win_x + text_x), (int) (win_y + text_y));
}

} // namespace

void Draw() {
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
    }
    alpha = std::clamp(alpha, 0, 0x80);

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        DrawSprite(0x50, 0x28, 0xB3, 0x118, 0xAA, 0x28, alpha);
    }
    DrawTabPlate(alpha);
    DrawScrollBar(alpha);
    MenuTextureReload(g_screen.block_no);
    DrawSprite(kExitX, kExitY, 452, 224, kExitWidth, kExitHeight, alpha);

    if (g_screen.step == OPTION_STEP_RUN) {
        g_screen.step_count = 0;
    } else {
        g_screen.step_count++;
    }

    if (g_screen.mode == OPTION_OPEN_TITLE) {
        DrawTitleHelp(alpha);
    }

    MenuTextureReload(0x1A);
    GetTexts().shortcuts.Draw(kExitX, kHelpY, alpha);
    DrawTabs(alpha);
    DrawRows(alpha);

    // The pointer and its bracket are the topmost layer: draw them after the menu text so the
    // item the mouse is over cannot cover the cursor.
    if (g_screen.step == OPTION_STEP_RUN) {
        DrawSelection(alpha);
    }

    setbilinear(1);
}

} // namespace options
