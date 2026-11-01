#include "ui/framework/tracker.hpp"

#include <string>
#include <utility>

#include "gamepad.hpp"
#include "localize/localize.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "snd.hpp"
#include "ui/framework/surfaces.hpp"
#include "ui/framework/text.hpp"

namespace dc::ui {

using namespace dc::game;
using namespace dc::localize;

namespace {

// The layout the journal's chest tracker was drawn with, which every tracker keeps.
constexpr float kHeaderScale = 0.92f;
constexpr float kBodyScale = 0.84f;
constexpr int   kLeftPaneX = 92;
constexpr int   kDividerX = 258;
constexpr int   kRightPaneX = 274;
constexpr int   kIconSize = 20;
constexpr int   kLabelX = kRightPaneX + 32;
constexpr int   kCountX = 460;
constexpr int   kListY = 156;
constexpr int   kRowSpacing = 25;
constexpr int   kCursorX = 48;

} // namespace

void Tracker::Open(TrackerSource source) {
    m_source = std::move(source);
    m_selected = 0;
    m_cursor.SnapTo(static_cast<float>(kCursorX), static_cast<float>(kListY - 2));
}

int Tracker::Groups() const {
    return m_source.group_count ? m_source.group_count() : 0;
}

Rect Tracker::GroupRect(int group) const {
    return Rect(84, kListY - 4 + (group * kRowSpacing), 164, kRowSpacing);
}

Rect Tracker::FilterRect() const {
    return m_source.filter_on.empty() ? Rect() : Rect(180, 100, 280, 28);
}

void Tracker::HandlePointer(float x, float y) {
    for (int group = 0; group < Groups(); group++) {
        if (GroupRect(group).Contains(x, y)) {
            if (group != m_selected) {
                m_selected = group;
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }
            return;
        }
    }
    if (FilterRect().Contains(x, y)) {
        m_filtered = !m_filtered;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }
}

bool Tracker::HandleKey() {
    const int groups = Groups();
    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return true;
    }
    if (GamePad.Down(PAD_UP) && groups > 0) {
        if (--m_selected < 0) {
            m_selected = groups - 1;
        }
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_DOWN) && groups > 0) {
        if (++m_selected >= groups) {
            m_selected = 0;
        }
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_CROSS | PAD_SQUARE) && !m_source.filter_on.empty()) {
        m_filtered = !m_filtered;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }
    return false;
}

void Tracker::Draw() {
    const int groups = Groups();

    TrackerWindow::DrawPaper(64, 91, 512, 262, 0x80);
    TrackerWindow::DrawStoneBanner(Rect(150, 74, 340, 28), 0x80, 0x80, m_source.title);

    Divider::DrawVertical(kDividerX, 132, 338, 0x80);
    Divider::DrawHorizontal(84, kDividerX - 10, 285, 0x80);
    Divider::DrawHorizontal(kRightPaneX, 542, 285, 0x80);

    MenuTextureReload(0x1A);

    if (!m_source.filter_on.empty()) {
        const std::string &filter = m_filtered ? m_source.filter_on : m_source.filter_off;
        Label(filter, m_filtered ? ColorYellow : ColorWhite, Alignment::Center, true, kBodyScale).Draw(320, 108, 0x80);
    }

    Label(m_source.groups_heading, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kLeftPaneX, 132, 0x80);

    for (int group = 0; group < groups; group++) {
        const bool selected = group == m_selected;
        Label(m_source.group_name(group), selected ? ColorYellow : ColorWhite, Alignment::Left, true, kBodyScale)
            .Draw(kLeftPaneX, kListY + (group * kRowSpacing), selected ? 0x80 : 0x58);
    }

    TrackerTally all;
    for (int group = 0; group < groups; group++) {
        const TrackerTally tally = m_source.tally(group, m_filtered);
        all.done += tally.done;
        all.total += tally.total;
    }

    Label(m_source.all_label, ColorGold, Alignment::Left, true, kHeaderScale).Draw(kLeftPaneX, 287, 0x80);
    DrawCounter(kLeftPaneX, 316, all.done, all.total, kBodyScale);

    if (groups == 0) {
        return;
    }

    const std::string name = m_source.group_name(m_selected);
    Label(LocalizeFormat(m_source.breakdown, {name}), ColorYellow, Alignment::Left, true, kHeaderScale)
        .Draw(kRightPaneX, 132, 0x80);

    int row_y = kListY;
    for (const TrackerRow &row : m_source.rows(m_selected, m_filtered)) {
        const bool icon = row.item_no >= 0 && ItemIcon::Draw(row.item_no, kRightPaneX + 2, row_y - 1, kIconSize, 0x80);

        MenuTextureReload(0x1A);
        Label(row.label, ColorWhite, Alignment::Left, true, kBodyScale).Draw(icon ? kLabelX : kRightPaneX, row_y, 0x80);
        DrawCounter(kCountX, row_y, row.tally.done, row.tally.total, kBodyScale);
        row_y += kRowSpacing;
    }

    const TrackerTally tally = m_source.tally(m_selected, m_filtered);
    Label(m_source.group_label, ColorYellow, Alignment::Left, true, kHeaderScale).Draw(kRightPaneX, 287, 0x80);
    DrawCounter(kCountX, 287, tally.done, tally.total, kBodyScale);

    const float progress = tally.total > 0 ? static_cast<float>(tally.done) / static_cast<float>(tally.total) : 0.0f;
    const int   percent = tally.total > 0 ? (tally.done * 100) / tally.total : 0;

    ProgressBarStyle bar_style{};
    bar_style.text_scale = 0.76f;
    ProgressBar::Draw(Rect(kRightPaneX, 314, 268, 22), progress, LocalizeFormat(m_source.progress, {std::to_string(percent)}), bar_style, 0x80);

    MenuTextureReload(0x1A);
    Label(m_source.controls, ColorWhite, Alignment::Center).Draw(320, 418, 0x80);

    const int selected_y = kListY + (m_selected * kRowSpacing);
    m_brackets.Draw(Rect(kLeftPaneX - 4, selected_y - 1, 152, 22), 0x80);

    m_cursor.SetTarget(static_cast<float>(kCursorX), static_cast<float>(selected_y - 2));
    m_cursor.Draw(0x80);
}

} // namespace dc::ui
