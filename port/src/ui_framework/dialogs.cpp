#include <algorithm>
#include <utility>
#include <vector>

#include "gamepad.hpp"
#include "menu_pointer.hpp"
#include "snd.hpp"
#include "ui_framework/internal.hpp"

namespace UI {

namespace {

/**
 * Deferred draw callback associated with a Z-order layer and submission index.
 */
struct QueuedDrawCmd {
    ZLayer                layer; /**< Target Z-layer for sorting. */
    std::size_t           order; /**< Submission sequence number within the frame. */
    std::function<void()> fn;    /**< Deferred draw callback. */
};

/**
 * Vertical line spacing inside modal dialog boxes in logical pixels.
 */
constexpr int kModalLineSpacing = 20;

/**
 * Returns the frame's deferred layer draw queue.
 */
std::vector<QueuedDrawCmd> &GetLayerQueueStorage() {
    static std::vector<QueuedDrawCmd> queue;
    return queue;
}

} // namespace

void LayerQueue::Begin() {
    GetLayerQueueStorage().clear();
}

void LayerQueue::Submit(ZLayer layer, std::function<void()> draw_fn) {
    if (!draw_fn) {
        return;
    }
    auto &queue = GetLayerQueueStorage();
    queue.push_back(QueuedDrawCmd{layer, queue.size(), std::move(draw_fn)});
}

void LayerQueue::Flush() {
    auto &queue = GetLayerQueueStorage();
    if (queue.empty()) {
        return;
    }

    std::stable_sort(queue.begin(), queue.end(), [](const QueuedDrawCmd &a, const QueuedDrawCmd &b) {
        if (a.layer != b.layer) {
            return static_cast<int>(a.layer) < static_cast<int>(b.layer);
        }
        return a.order < b.order;
    });

    for (const auto &cmd : queue) {
        cmd.fn();
    }
    queue.clear();
}

void DialogBox::Configure(std::string_view title, std::string_view description,
                          std::vector<DialogOption> options,
                          std::function<void()>     on_cancel,
                          const DialogStyle        &style) {
    m_title = std::string(title);
    m_description = std::string(description);
    m_options = std::move(options);
    m_on_cancel = std::move(on_cancel);
    m_style = style;
}

void DialogBox::Open(int default_choice) {
    m_active = true;
    SetSelectedIndex(default_choice);
    const Rect btn = GetButtonRect(m_selected);
    m_cursor.SnapTo(static_cast<float>(btn.x - 34), static_cast<float>(btn.y + 3));
}

void DialogBox::SetSelectedIndex(int index) {
    if (m_options.empty()) {
        m_selected = 0;
        return;
    }
    const int count = static_cast<int>(m_options.size());
    m_selected = ((index % count) + count) % count;
}

Rect DialogBox::ComputeBoxRect(std::vector<std::string> &out_lines) const {
    out_lines.clear();
    const int wrap_w = m_style.max_width - 48;

    if (!m_title.empty()) {
        auto t_lines = WrapText(m_title, wrap_w);
        out_lines.insert(out_lines.end(), t_lines.begin(), t_lines.end());
    }
    if (!m_description.empty()) {
        auto d_lines = WrapText(m_description, wrap_w);
        out_lines.insert(out_lines.end(), d_lines.begin(), d_lines.end());
    }
    if (out_lines.empty()) {
        out_lines.emplace_back("");
    }

    int max_text_w = 0;
    for (const std::string &line : out_lines) {
        max_text_w = std::max(max_text_w, Label(line, ColorWhite).GetWidth());
    }

    const int box_w = std::clamp(max_text_w + 48, m_style.min_width, m_style.max_width);
    const int box_h = 38 + static_cast<int>(out_lines.size() - 1) * kModalLineSpacing;
    const int box_x = (Metrics::kScreenWidth - box_w) / 2;
    const int box_y = m_style.base_y - std::max(0, static_cast<int>(out_lines.size()) - 2) * 12;
    return Rect(box_x, box_y, box_w, box_h);
}

Rect DialogBox::GetButtonRect(int index) const {
    std::vector<std::string> lines;
    const Rect               box = ComputeBoxRect(lines);
    const int                btn_x = (Metrics::kScreenWidth - m_style.button_width) / 2;
    const int                btn_y = box.Bottom() + 10 + index * (m_style.button_height + m_style.button_gap);
    return Rect(btn_x, btn_y, m_style.button_width, m_style.button_height);
}

int DialogBox::HitTest(float px, float py) const {
    for (size_t i = 0; i < m_options.size(); ++i) {
        const Rect r = GetButtonRect(static_cast<int>(i)).Inset(-14, -3);
        if (r.Contains(px, py)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool DialogBox::HandleInput() {
    if (!m_active) {
        return false;
    }

    const int count = static_cast<int>(m_options.size());

    float ptr_x = 0.0f;
    float ptr_y = 0.0f;
    if (MenuPointerHand(ptr_x, ptr_y)) {
        const float tip_x = ptr_x + 26.0f;
        const float tip_y = ptr_y + 14.0f;
        const int   hit = HitTest(tip_x, tip_y);
        if (hit >= 0 && hit != m_selected) {
            m_selected = hit;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    }

    if (count > 1) {
        if (GamePad.Down(PAD_UP)) {
            SetSelectedIndex(m_selected - 1);
            ComMenuSePlay(MENU_SOUND_CURSOR);
        } else if (GamePad.Down(PAD_DOWN)) {
            SetSelectedIndex(m_selected + 1);
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    }

    if (GamePad.Down(PAD_CROSS)) {
        if (m_selected >= 0 && m_selected < count) {
            if (!m_options[m_selected].enabled) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                return true;
            }
            ComMenuSePlay(MENU_SOUND_CONFIRM);
            m_active = false;
            if (m_options[m_selected].on_select) {
                m_options[m_selected].on_select();
            }
            return false;
        }
    } else if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        m_active = false;
        if (m_on_cancel) {
            m_on_cancel();
        }
        return false;
    }

    return true;
}

void DialogBox::Draw(int alpha) {
    if (alpha <= 0) {
        return;
    }

    MenuTextureReload(detail::kMessageTextureBlock);
    std::vector<std::string> lines;
    const Rect               box = ComputeBoxRect(lines);

    TrackerWindow::Draw(box, alpha, m_style.window_preset);

    MenuTextureReload(detail::kMessageTextureBlock);
    for (size_t i = 0; i < lines.size(); ++i) {
        Label(lines[i], ColorWhite).Draw(box.x + 22, box.y + 12 + static_cast<int>(i) * kModalLineSpacing, alpha);
    }

    for (size_t i = 0; i < m_options.size(); ++i) {
        const Rect btn_rect = GetButtonRect(static_cast<int>(i));
        const int  btn_a = m_options[i].enabled ? alpha : (alpha >> 1);
        BevelButton::Draw(btn_rect, m_options[i].label, m_options[i].style, btn_a);
    }

    if (!m_options.empty()) {
        const int  clamped = std::clamp(m_selected, 0, static_cast<int>(m_options.size()) - 1);
        const Rect sel_box = GetButtonRect(clamped);

        setbilinear(0);
        MenuTextureReload(BtlMenuReadBlock);
        StayTex = detail::FindMenuTexture("stayframe", StayTex);
        if (StayTex != nullptr) {
            DrawMenuWaku(sel_box.x - 14, sel_box.y - 6, sel_box.w + 12, sel_box.h - 4, 0, StayTex, alpha);
        }

        m_cursor.SetTarget(static_cast<float>(sel_box.x - 34), static_cast<float>(sel_box.y + 3));
        m_cursor.Draw(alpha);
    }
}

void ConfirmModal::Draw(std::string_view title, std::string_view description,
                        std::string_view yes_label, std::string_view no_label,
                        int selected_choice, Cursor &cursor, int alpha,
                        WindowPreset preset, int chosen_choice, bool lit) {
    if (alpha <= 0) {
        return;
    }

    MenuTextureReload(detail::kMessageTextureBlock);

    std::vector<std::string> lines;
    if (!title.empty()) {
        std::vector<std::string> title_lines = WrapText(title, 280);
        lines.insert(lines.end(), title_lines.begin(), title_lines.end());
    }
    if (!description.empty()) {
        std::vector<std::string> desc_lines = WrapText(description, 280);
        lines.insert(lines.end(), desc_lines.begin(), desc_lines.end());
    }
    if (lines.empty()) {
        lines.emplace_back("");
    }

    int max_text_w = 0;
    for (const std::string &line : lines) {
        max_text_w = std::max(max_text_w, Label(line, ColorWhite).GetWidth());
    }

    const int box_w = std::clamp(max_text_w + 48, Metrics::kModalMinWidth, Metrics::kModalMaxWidth);
    const int box_h = 38 + static_cast<int>(lines.size() - 1) * kModalLineSpacing;
    const int box_x = (Metrics::kScreenWidth - box_w) / 2;
    const int box_y = 164 - std::max(0, static_cast<int>(lines.size()) - 2) * 12;

    TrackerWindow::Draw(box_x, box_y, box_w, box_h, alpha, preset);

    MenuTextureReload(detail::kMessageTextureBlock);
    for (size_t i = 0; i < lines.size(); ++i) {
        Label(lines[i], ColorWhite).Draw(box_x + 22, box_y + 12 + static_cast<int>(i) * kModalLineSpacing, alpha);
    }

    // The game's yes/no plates: their size and spacing, lettered as the weapon menu's buttons are.
    constexpr int btn_w = 0x60;
    constexpr int btn_h = 23;
    constexpr int btn_x = (Metrics::kScreenWidth - btn_w) / 2;
    const int     btn_y0 = box_y + box_h + 10;
    const int     btn_y1 = btn_y0 + 0x1C;

    ButtonStyle yes_style = ButtonStyle::Thin(ButtonPreset::Terracotta);
    ButtonStyle no_style = yes_style;
    yes_style.pressed = lit && chosen_choice == 0;
    no_style.pressed = lit && chosen_choice == 1;
    BevelButton::Draw(Rect(btn_x, btn_y0, btn_w, btn_h), yes_label, yes_style, alpha);
    BevelButton::Draw(Rect(btn_x, btn_y1, btn_w, btn_h), no_label, no_style, alpha);
    if (chosen_choice >= 0) {
        return;
    }

    const int sel_y = (selected_choice == 0) ? btn_y0 : btn_y1;
    setbilinear(0);
    MenuTextureReload(BtlMenuReadBlock);
    StayTex = detail::FindMenuTexture("stayframe", StayTex);
    if (StayTex != nullptr) {
        DrawMenuWaku(btn_x - 14, sel_y - 6, btn_w + 12, btn_h - 4, 0, StayTex, alpha);
    }

    cursor.SetTarget(static_cast<float>(btn_x - 34), static_cast<float>(sel_y + 3));
    cursor.Draw(alpha);
}

void ListView::Reset(int initial_index) {
    m_selected = std::max(0, initial_index);
    m_first_row = 0;
    if (m_cfg.visible_rows > 0 && m_selected >= m_cfg.visible_rows) {
        m_first_row = m_selected - m_cfg.visible_rows + 1;
    }
    const Rect row_box = GetVisibleRowRect(m_selected - m_first_row);
    m_cursor.SnapTo(static_cast<float>(row_box.x) + m_cfg.cursor_offset_x,
                    static_cast<float>(row_box.y) + m_cfg.cursor_offset_y);
}

void ListView::SetSelectedIndex(int index, int total_items) {
    if (total_items <= 0) {
        m_selected = 0;
        m_first_row = 0;
        return;
    }
    if (m_cfg.wrap_navigation) {
        m_selected = ((index % total_items) + total_items) % total_items;
    } else {
        m_selected = std::clamp(index, 0, total_items - 1);
    }
    EnsureVisible(total_items);
}

void ListView::EnsureVisible(int total_items) {
    const int vis = std::max(1, m_cfg.visible_rows);
    if (m_selected < m_first_row) {
        m_first_row = m_selected;
    } else if (m_selected >= m_first_row + vis) {
        m_first_row = m_selected - vis + 1;
    }
    const int max_first = std::max(0, total_items - vis);
    m_first_row = std::clamp(m_first_row, 0, max_first);
}

Rect ListView::GetVisibleRowRect(int visible_slot) const {
    return m_cfg.bounds.Row(visible_slot, m_cfg.row_height, m_cfg.row_stride);
}

int ListView::HitTest(float px, float py, int total_items) const {
    if (total_items <= 0) {
        return -1;
    }
    const int vis = std::min(m_cfg.visible_rows, total_items - m_first_row);
    for (int s = 0; s < vis; ++s) {
        if (GetVisibleRowRect(s).Contains(px, py)) {
            return m_first_row + s;
        }
    }
    return -1;
}

bool ListView::HandleInput(int                             total_items,
                           const std::function<void(int)> &on_confirm,
                           const std::function<void()>    &on_cancel) {
    if (total_items <= 0) {
        return false;
    }

    const int prev_sel = m_selected;
    const int prev_first = m_first_row;

    float ptr_x = 0.0f;
    float ptr_y = 0.0f;
    if (MenuPointerHand(ptr_x, ptr_y)) {
        const int hit = HitTest(ptr_x + 26.0f, ptr_y + 14.0f, total_items);
        if (hit >= 0 && hit != m_selected) {
            m_selected = hit;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    }

    if (GamePad.Down(PAD_UP)) {
        SetSelectedIndex(m_selected - 1, total_items);
        ComMenuSePlay(MENU_SOUND_CURSOR);
    } else if (GamePad.Down(PAD_DOWN)) {
        SetSelectedIndex(m_selected + 1, total_items);
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    m_scrollbar.Update(m_selected != prev_sel || m_first_row != prev_first);

    if (GamePad.Down(PAD_CROSS)) {
        if (on_confirm) {
            on_confirm(m_selected);
        }
    } else if (GamePad.Down(PAD_CIRCLE)) {
        if (on_cancel) {
            on_cancel();
        }
    }

    return m_selected != prev_sel;
}

void ListView::Draw(int total_items, const RowDrawFn &draw_row, int alpha) {
    if (total_items <= 0 || alpha <= 0) {
        return;
    }

    EnsureVisible(total_items);
    const int vis = std::min(m_cfg.visible_rows, total_items - m_first_row);

    for (int s = 0; s < vis; ++s) {
        const int  item_idx = m_first_row + s;
        const bool selected = (item_idx == m_selected);
        if (draw_row) {
            draw_row(item_idx, GetVisibleRowRect(s), selected, alpha);
        }
    }

    if (m_cfg.draw_scrollbar && total_items > m_cfg.visible_rows) {
        m_scrollbar.Draw(m_cfg.bounds.Right() + 6, m_cfg.bounds.Top(),
                         m_cfg.bounds.h, total_items, m_cfg.visible_rows, m_first_row, alpha);
    }

    const int sel_slot = m_selected - m_first_row;
    if (sel_slot >= 0 && sel_slot < vis) {
        const Rect sel_rect = GetVisibleRowRect(sel_slot);
        if (m_cfg.draw_brackets) {
            m_brackets.Draw(sel_rect.Inset(-m_cfg.bracket_pad_x, -m_cfg.bracket_pad_y), alpha);
        }
        if (m_cfg.draw_cursor) {
            m_cursor.SetTarget(static_cast<float>(sel_rect.x) + m_cfg.cursor_offset_x,
                               static_cast<float>(sel_rect.y) + m_cfg.cursor_offset_y);
            m_cursor.Draw(alpha);
        }
    }
}

} // namespace UI
