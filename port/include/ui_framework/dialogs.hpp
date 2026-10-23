#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ui_framework/buttons.hpp"
#include "ui_framework/surfaces.hpp"
#include "ui_framework/types.hpp"
#include "ui_framework/widgets.hpp"

namespace UI {

/**
 * Configurable button option entry for a modal DialogBox.
 */
struct DialogOption {
    std::string           label;                        /**< Button text label. */
    std::function<void()> on_select{};                  /**< Callback invoked when the option is confirmed. */
    ButtonStyle           style = ButtonStyle::Thick(); /**< Visual button style. */
    bool                  enabled = true;               /**< Whether the option can be selected. */
};

/**
 * Layout and surface styling parameters for a modal DialogBox.
 */
struct DialogStyle {
    WindowPreset window_preset = WindowPreset::DarkTranslucent; /**< Background surface preset for the prompt box. */
    int          min_width = Metrics::kModalMinWidth;           /**< Minimum prompt box width in logical pixels. */
    int          max_width = Metrics::kModalMaxWidth;           /**< Maximum prompt box width in logical pixels. */
    int          button_width = Metrics::kButtonWidth;          /**< Option button width in logical pixels. */
    int          button_height = Metrics::kButtonHeight;        /**< Option button height in logical pixels. */
    int          button_gap = Metrics::kButtonGap;              /**< Vertical gap between option buttons in pixels. */
    int          base_y = 164;                                  /**< Baseline top Y coordinate of the prompt box. */
};

/**
 * Stateful multi-option modal dialog box handling gamepad and mouse input.
 */
class DialogBox {
public:
    /**
     * Constructs an inactive dialog box.
     */
    DialogBox() = default;

    /**
     * Configures the dialog title, description, option list, cancel callback, and style.
     */
    void Configure(std::string_view title, std::string_view description,
                   std::vector<DialogOption> options,
                   std::function<void()>     on_cancel = {},
                   const DialogStyle        &style = {});

    /**
     * Opens the dialog box and snaps the cursor to default_choice.
     */
    void Open(int default_choice = 0);

    /**
     * Closes the dialog box.
     */
    void Close() { m_active = false; }

    /**
     * Returns whether the dialog box is currently open.
     */
    bool IsOpen() const { return m_active; }

    /**
     * Returns the index of the currently highlighted option.
     */
    int SelectedIndex() const { return m_selected; }

    /**
     * Sets the highlighted option index with wrap-around clamping.
     */
    void SetSelectedIndex(int index);

    /**
     * Processes gamepad and mouse pointer input while the dialog is open.
     */
    bool HandleInput();

    /**
     * Draws the dialog prompt box, option buttons, selection frame, and cursor.
     */
    void Draw(int alpha = 0x80);

    /**
     * Returns the bounding rectangle of the option button at index.
     */
    Rect GetButtonRect(int index) const;

    /**
     * Returns the option index under the logical point (px, py), or -1 if none.
     */
    int HitTest(float px, float py) const;

private:
    /**
     * Wraps the prompt text and computes the bounding rectangle of the prompt window.
     */
    Rect ComputeBoxRect(std::vector<std::string> &out_lines) const;

    bool                      m_active{false}; /**< Whether the dialog is currently open. */
    int                       m_selected{0};   /**< Currently highlighted option index. */
    std::string               m_title;         /**< Optional header line displayed at the top of the box. */
    std::string               m_description;   /**< Body description text. */
    std::vector<DialogOption> m_options;       /**< List of selectable button options. */
    std::function<void()>     m_on_cancel;     /**< Callback invoked when the dialog is cancelled. */
    DialogStyle               m_style{};       /**< Active layout and surface style. */
    Cursor                    m_cursor;        /**< Animated hand cursor for the option list. */
};

/**
 * Stateless two-button confirmation modal renderer.
 */
class ConfirmModal {
public:
    /**
     * Draws a centered two-button confirmation modal with a selection frame and hand cursor. With a chosen_choice (0
     * or 1, once the choice is made) the frame and cursor are gone, as the game takes them away, and while lit that
     * button shows pressed.
     */
    static void Draw(std::string_view title, std::string_view description,
                     std::string_view yes_label, std::string_view no_label,
                     int selected_choice, Cursor &cursor, int alpha = 0x80,
                     WindowPreset preset = WindowPreset::DarkTranslucent, int chosen_choice = -1, bool lit = false);

    /**
     * Returns the button of the modal Draw lays out for this title and description under the logical point (x, y):
     * 0 for the first (yes), 1 for the second (no), -1 for neither. Each button's row of the stack counts, so the
     * point need not be on the plate itself.
     */
    static int Hit(std::string_view title, std::string_view description, float x, float y);

    /**
     * Returns where Draw puts a button (0 yes, 1 no) of the modal for this title and description, as for placing the
     * hand cursor on one before the modal is first drawn: Draw aims it at 34 left of the plate and 3 below its top.
     */
    static Rect ButtonRect(std::string_view title, std::string_view description, int button);
};

/**
 * Layout and behavior configuration for a scrollable ListView widget.
 */
struct ListViewConfig {
    Rect  bounds{};                         /**< Bounding rectangle of the visible row area. */
    int   row_height = Metrics::kRowHeight; /**< Height of an individual row in logical pixels. */
    int   row_stride = Metrics::kRowStride; /**< Vertical distance between row tops in pixels. */
    int   visible_rows = 6;                 /**< Maximum number of rows visible at once. */
    int   bracket_pad_x = 4;                /**< Horizontal expansion of selection brackets around a row. */
    int   bracket_pad_y = 1;                /**< Vertical expansion of selection brackets around a row. */
    float cursor_offset_x = -44.0f;         /**< Horizontal offset of the hand cursor relative to a row. */
    float cursor_offset_y = -2.0f;          /**< Vertical offset of the hand cursor relative to a row. */
    bool  wrap_navigation = true;           /**< Whether up/down navigation wraps around at the ends. */
    bool  draw_brackets = true;             /**< Whether to draw pulsing brackets on the selected row. */
    bool  draw_scrollbar = true;            /**< Whether to draw a vertical scrollbar when rows overflow. */
    bool  draw_cursor = true;               /**< Whether to draw the hand cursor beside the selected row. */
};

/**
 * Scrollable vertical list view managing selection, viewport scrolling, and input.
 */
class ListView {
public:
    using RowDrawFn = std::function<void(int item_index, const Rect &row_rect, bool selected, int alpha)>;

    /**
     * Constructs a list view with default configuration.
     */
    ListView() = default;

    /**
     * Constructs a list view with the given configuration.
     */
    explicit ListView(const ListViewConfig &cfg) : m_cfg(cfg) {}

    /**
     * Updates the list view configuration.
     */
    void SetConfig(const ListViewConfig &cfg) { m_cfg = cfg; }

    /**
     * Returns the current list view configuration.
     */
    const ListViewConfig &GetConfig() const { return m_cfg; }

    /**
     * Resets the selection and scroll viewport to initial_index.
     */
    void Reset(int initial_index = 0);

    /**
     * Returns the currently selected item index.
     */
    int SelectedIndex() const { return m_selected; }

    /**
     * Returns the index of the first visible item at the top of the viewport.
     */
    int FirstRow() const { return m_first_row; }

    /**
     * Sets the selected item index and scrolls the viewport to keep it visible.
     */
    void SetSelectedIndex(int index, int total_items);

    /**
     * Returns the bounding rectangle for a visible slot index in [0, visible_rows).
     */
    Rect GetVisibleRowRect(int visible_slot) const;

    /**
     * Returns the item index under the logical point (px, py), or -1 if none.
     */
    int HitTest(float px, float py, int total_items) const;

    /**
     * Processes directional navigation, mouse hover, confirm, and cancel inputs.
     */
    bool HandleInput(int                             total_items,
                     const std::function<void(int)> &on_confirm = {},
                     const std::function<void()>    &on_cancel = {});

    /**
     * Draws all visible rows, the scrollbar, selection brackets, and the hand cursor.
     */
    void Draw(int total_items, const RowDrawFn &draw_row, int alpha = 0x80);

    /**
     * Returns a mutable reference to the list's hand cursor.
     */
    Cursor &GetCursor() { return m_cursor; }

    /**
     * Returns a mutable reference to the list's selection brackets.
     */
    Brackets &GetBrackets() { return m_brackets; }

private:
    /**
     * Clamps the first visible row so the selected item stays within the viewport.
     */
    void EnsureVisible(int total_items);

    ListViewConfig m_cfg{};        /**< Active list configuration. */
    int            m_selected{0};  /**< Currently selected item index. */
    int            m_first_row{0}; /**< Index of the top-most visible item. */
    Cursor         m_cursor;       /**< Hand cursor instance. */
    Brackets       m_brackets;     /**< Selection brackets instance. */
    ScrollBar      m_scrollbar;    /**< Vertical scrollbar instance. */
};

} // namespace UI
