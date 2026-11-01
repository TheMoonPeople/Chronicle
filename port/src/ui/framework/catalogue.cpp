#include "ui/framework/catalogue.hpp"

#include <algorithm>
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
constexpr int   kLeftPaneX = 92;
constexpr int   kDividerX = 258;
constexpr int   kRightPaneX = 274;
constexpr int   kRightPaneWidth = 268;
constexpr int   kCountX = 460;
constexpr int   kListY = 156;
constexpr int   kListWidth = 136;
constexpr int   kRowSpacing = 25;
constexpr int   kVisibleRows = 7;
constexpr int   kLineSpacing = 20;
constexpr int   kDetailBottom = 280;
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
}

int Catalogue::Count() const {
    return m_source.count ? m_source.count() : 0;
}

int Catalogue::EntryAt(float x, float y) const {
    return m_list.HitTest(x, y, Count());
}

bool Catalogue::HandleKey() {
    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return true;
    }
    m_list.HandleInput(Count());
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

    if (count > 0) {
        const CatalogueDetail detail = m_source.detail(m_list.SelectedIndex());
        MenuTextureReload(0x1A);
        Label(detail.heading, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kRightPaneX, 132, 0x80);

        int        y = 158;
        const auto draw_lines = [&y](const std::string &text, u32 color) {
            for (const std::string &line : WrapText(text, kRightPaneWidth, kBodyScale)) {
                if (y > kDetailBottom - kLineSpacing) {
                    return;
                }
                Label(line, color, Alignment::Left, true, kBodyScale).Draw(kRightPaneX, y, 0x80);
                y += kLineSpacing;
            }
        };
        draw_lines(detail.status, detail.status_color);
        if (!detail.body_heading.empty() && !detail.body.empty()) {
            y += kLineSpacing / 2;
            draw_lines(detail.body_heading, ColorGold);
        }
        for (const std::string &paragraph : detail.body) {
            draw_lines(paragraph, ColorWhite);
        }
    }

    if (m_source.summary && !m_source.summary_label.empty()) {
        const TrackerTally tally = m_source.summary();
        Divider::DrawHorizontal(kRightPaneX, 542, 285, 0x80);
        MenuTextureReload(0x1A);
        Label(m_source.summary_label, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kRightPaneX, 287, 0x80);
        DrawCounter(kCountX, 287, tally.done, tally.total, kBodyScale);

        const float progress = tally.total > 0 ? static_cast<float>(tally.done) / static_cast<float>(tally.total) : 0.0f;
        const int   percent = tally.total > 0 ? (tally.done * 100) / tally.total : 0;

        ProgressBarStyle bar_style{};
        bar_style.text_scale = 0.76f;
        ProgressBar::Draw(Rect(kRightPaneX, 314, kRightPaneWidth, 22), progress,
                          LocalizeFormat(m_source.progress, {std::to_string(percent)}), bar_style, 0x80);
    }

    MenuTextureReload(0x1A);
    Label(m_source.controls, ColorWhite, Alignment::Center).Draw(320, 418, 0x80);
}

} // namespace dc::ui
