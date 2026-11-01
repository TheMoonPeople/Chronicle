#include "ui/framework/catalogue.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "gamepad.hpp"
#include "localize/localize.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "snd.hpp"
#include "ui/framework/surfaces.hpp"
#include "ui/framework/text.hpp"
#include "ui/framework/widgets.hpp"

namespace dc::ui {

using namespace dc::game;
using namespace dc::localize;

namespace {

// The tracker's paper, banner and panes, so the two read as one family.
constexpr float kHeaderScale = 0.92f;
constexpr float kBodyScale = 0.84f;
constexpr float kPageScale = 0.78f;
constexpr int   kLeftPaneX = 92;
constexpr int   kDividerX = 258;
constexpr int   kRightPaneX = 274;
constexpr int   kRightPaneRight = 542;
constexpr int   kRightPaneWidth = kRightPaneRight - kRightPaneX;
constexpr int   kListY = 156;
constexpr int   kListWidth = 136;
constexpr int   kRowSpacing = 25;
constexpr int   kVisibleRows = 5;
constexpr int   kLineSpacing = 20;
constexpr int   kPageLineSpacing = 18;
constexpr int   kDetailBottom = 338;
// Each end of the page indicator, "< 1/3 >", right-aligned on the page title's line.
constexpr int kArrowWidth = 14;
// As the battle menu's captions do, a long label is squeezed rather than cut, but no further than this.
constexpr float kLeastSqueeze = 0.62f;

} // namespace

Catalogue::Catalogue() {
    ListViewConfig config;
    config.bounds = Rect(kLeftPaneX, kListY, kListWidth, kRowSpacing * kVisibleRows);
    config.row_height = 20;
    config.row_stride = kRowSpacing;
    config.visible_rows = kVisibleRows;
    config.wrap_navigation = true;
    m_list.SetConfig(config);
}

void Catalogue::Open(CatalogueSource source) {
    m_source = std::move(source);
    m_list.Reset(0);
    m_page = 0;
    m_page_entry = 0;
}

int Catalogue::Count() const {
    return m_source.count ? m_source.count() : 0;
}

int Catalogue::Pages() const {
    return Count() > 0 ? static_cast<int>(m_source.detail(m_list.SelectedIndex()).pages.size()) : 0;
}

int Catalogue::EntryAt(float x, float y) const {
    return m_list.HitTest(x, y, Count());
}

Rect Catalogue::PageArrowRect(int direction) const {
    if (m_indicator.Empty()) {
        return {};
    }
    // The arrows are the indicator's ends, with a little room around them for the hand.
    return direction < 0 ? Rect(m_indicator.x - 6, m_indicator.y - 4, kArrowWidth + 6, m_indicator.h + 8)
                         : Rect(m_indicator.Right() - kArrowWidth, m_indicator.y - 4, kArrowWidth + 6, m_indicator.h + 8);
}

void Catalogue::TurnPage(int direction) {
    const int pages = Pages();
    if (pages < 2) {
        return;
    }
    m_page = (m_page + direction + pages) % pages;
    ComMenuSePlay(MENU_SOUND_CURSOR);
}

bool Catalogue::HandleKey() {
    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return true;
    }
    m_list.HandleInput(Count());
    if (m_list.SelectedIndex() != m_page_entry) {
        m_page_entry = m_list.SelectedIndex();
        m_page = 0;
    }
    if (GamePad.Down(PAD_LEFT)) {
        TurnPage(-1);
    } else if (GamePad.Down(PAD_RIGHT)) {
        TurnPage(1);
    }
    return false;
}

void Catalogue::Draw() {
    const int count = Count();

    TrackerWindow::DrawPaper(64, 91, 512, 262, 0x80);
    TrackerWindow::DrawStoneBanner(Rect(150, 74, 340, 28), 0x80, 0x80, m_source.title);

    Divider::DrawVertical(kDividerX, 132, 338, 0x80);

    MenuTextureReload(0x1A);
    Label(m_source.list_heading, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kLeftPaneX, 132, 0x80);

    m_list.Draw(count, [this](int index, const Rect &row, bool /*selected*/, int alpha) {
        const CatalogueEntry entry = m_source.entry(index);
        MenuTextureReload(0x1A);
        Label     label(entry.label, entry.color, Alignment::Left, true, kBodyScale);
        const int width = label.GetWidth();
        if (width > row.w) {
            label.SetXScale(std::max(kLeastSqueeze, static_cast<float>(row.w) / static_cast<float>(width)));
        }
        label.Draw(row.x, row.y, alpha);
    });

    if (m_source.summary && !m_source.summary_label.empty()) {
        const TrackerTally tally = m_source.summary();
        Divider::DrawHorizontal(84, kDividerX - 10, 285, 0x80);
        MenuTextureReload(0x1A);
        Label(m_source.summary_label, ColorGold, Alignment::Left, true, kHeaderScale).Draw(kLeftPaneX, 287, 0x80);
        DrawCounter(kLeftPaneX, 316, tally.done, tally.total, kBodyScale);
    }

    if (count > 0) {
        const CatalogueDetail detail = m_source.detail(m_list.SelectedIndex());
        MenuTextureReload(0x1A);
        Label(detail.heading, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kRightPaneX, 132, 0x80);

        int y = 158;
        for (const std::string &line : WrapText(detail.status, kRightPaneWidth, kBodyScale)) {
            Label(line, detail.status_color, Alignment::Left, true, kBodyScale).Draw(kRightPaneX, y, 0x80);
            y += kLineSpacing;
        }

        const int pages = static_cast<int>(detail.pages.size());
        m_indicator = {};
        if (pages > 0) {
            const CataloguePage &page = detail.pages[std::clamp(m_page, 0, pages - 1)];
            y += kLineSpacing / 2;
            Label(page.title, ColorGold, Alignment::Left, true, kBodyScale).Draw(kRightPaneX, y, 0x80);
            if (pages > 1) {
                Label     indicator(std::format("< {}/{} >", std::clamp(m_page, 0, pages - 1) + 1, pages), ColorYellow, Alignment::Right, true, kBodyScale);
                const int width = indicator.GetWidth();
                indicator.Draw(kRightPaneRight, y, 0x80);
                m_indicator = Rect(kRightPaneRight - width, y, width, 20);
            }
            y += kLineSpacing + 2;
            for (const std::string &line : WrapText(page.text, kRightPaneWidth, kPageScale)) {
                if (y > kDetailBottom - kPageLineSpacing) {
                    break;
                }
                Label(line, page.color, Alignment::Left, true, kPageScale).Draw(kRightPaneX, y, 0x80);
                y += kPageLineSpacing;
            }
        }
    }

    MenuTextureReload(0x1A);
    Label(m_source.controls, ColorWhite, Alignment::Center).Draw(320, 418, 0x80);
}

} // namespace dc::ui
