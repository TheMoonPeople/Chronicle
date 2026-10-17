#pragma once

#include <string_view>
#include <utility>

#include "ui_framework/text.hpp"
#include "ui_framework/types.hpp"

namespace UI {

/**
 * Visual material presets for window background plates.
 */
enum class WindowPreset {
    BlueStone,
    Paper,
    DarkTranslucent,
    Recessed,
    None,
};

/**
 * Preset constant for the procedural blue-slate stone window surface.
 */
constexpr WindowPreset BLUE_STONE_MENU = WindowPreset::BlueStone;

/**
 * Preset constant for the procedural parchment paper window surface.
 */
constexpr WindowPreset PAPER_MENU = WindowPreset::Paper;

/**
 * Preset constant for the dark translucent 9-slice help window surface.
 */
constexpr WindowPreset DARK_TRANSLUCENT_MENU = WindowPreset::DarkTranslucent;

/**
 * Procedural and 9-slice window surface renderer.
 */
class TrackerWindow {
public:
    /**
     * Draws a window plate of the specified preset at (x, y).
     */
    static void Draw(int x, int y, int width_px, int height_px, int alpha = 0x80,
                     WindowPreset preset = WindowPreset::BlueStone);

    /**
     * Draws a window plate of the specified preset inside a bounding rectangle.
     */
    static void Draw(const Rect &box, int alpha = 0x80,
                     WindowPreset preset = WindowPreset::BlueStone) {
        Draw(box.x, box.y, box.w, box.h, alpha, preset);
    }

    /**
     * Draws a parchment paper window plate at (x, y).
     */
    static void DrawPaper(int x, int y, int width_px, int height_px, int alpha = 0x80) {
        Draw(x, y, width_px, height_px, alpha, WindowPreset::Paper);
    }

    /**
     * Draws a parchment paper window plate inside a bounding rectangle.
     */
    static void DrawPaper(const Rect &box, int alpha = 0x80) {
        Draw(box.x, box.y, box.w, box.h, alpha, WindowPreset::Paper);
    }

    /**
     * Draws a dark translucent window plate at (x, y).
     */
    static void DrawDark(int x, int y, int width_px, int height_px, int alpha = 0x80) {
        Draw(x, y, width_px, height_px, alpha, WindowPreset::DarkTranslucent);
    }

    /**
     * Draws a dark translucent window plate inside a bounding rectangle.
     */
    static void DrawDark(const Rect &box, int alpha = 0x80) {
        Draw(box.x, box.y, box.w, box.h, alpha, WindowPreset::DarkTranslucent);
    }

    /**
     * Draws a recessed inner well with top-left shadow and bottom-right rim highlight.
     */
    static void DrawIndentation(const Rect &box, bool dark_variant = true, int alpha = 0x80);

    /**
     * Draws a 3-piece stone title banner with an optional centered title label.
     */
    static void DrawStoneBanner(const Rect &box, int alpha = 0x80, int brightness = 0x80,
                                std::string_view title = {},
                                const TextStyle &title_style = {
                                    .color       = ColorGold,
                                    .drop_shadow = true,
                                    .align       = Alignment::Center,
                                    .scale       = Metrics::kScaleHeader,
                                });
};

/**
 * Convenience wrapper for drawing parchment paper window plates.
 */
class PaperWindow {
public:
    /**
     * Draws a parchment paper window plate at (x, y).
     */
    static void Draw(int x, int y, int width_px, int height_px, int alpha = 0x80) {
        TrackerWindow::Draw(x, y, width_px, height_px, alpha, WindowPreset::Paper);
    }

    /**
     * Draws a parchment paper window plate inside a bounding rectangle.
     */
    static void Draw(const Rect &box, int alpha = 0x80) {
        TrackerWindow::Draw(box.x, box.y, box.w, box.h, alpha, WindowPreset::Paper);
    }
};

/**
 * Translucent 9-slice help box and titled description panel renderer.
 */
class HelpBox {
public:
    /**
     * Draws an untitled translucent 9-slice help box at (x, y).
     */
    static void Draw(int x, int y, int width_px, int height_px, int alpha = 0x80);

    /**
     * Draws an untitled translucent 9-slice help box inside a bounding rectangle.
     */
    static void Draw(const Rect &box, int alpha = 0x80) {
        Draw(box.x, box.y, box.w, box.h, alpha);
    }

    /**
     * Draws a bottom-anchored help box with a title line and word-wrapped description.
     */
    static void DrawTitled(int box_x, int bottom_y, int box_w, int text_offset_x,
                           std::string_view title, std::string_view desc,
                           int alpha = 0x80, int line_height = 28);
};

/**
 * Alias wrapper for drawing standard window plates.
 */
class Window {
public:
    /**
     * Draws a window plate at (x, y) using the specified preset.
     */
    static void Draw(int x, int y, int width_px, int height_px, int alpha = 0x80,
                     WindowPreset preset = WindowPreset::BlueStone);

    /**
     * Draws a window plate inside a bounding rectangle using the specified preset.
     */
    static void Draw(const Rect &box, int alpha = 0x80,
                     WindowPreset preset = WindowPreset::BlueStone) {
        Draw(box.x, box.y, box.w, box.h, alpha, preset);
    }
};

/**
 * Horizontal backing plate for tab strips.
 */
class TabPlate {
public:
    /**
     * Draws a horizontal tab backing plate spanning from left to right at vertical position y.
     */
    static void Draw(int left, int right, int y, int alpha = 0x80);
};

/**
 * Beveled separator line renderer for multi-pane layouts.
 */
class Divider {
public:
    /**
     * Draws a vertical recessed divider line from top_y to bottom_y at horizontal position x.
     */
    static void DrawVertical(int x, int top_y, int bottom_y, int alpha = 0x80);

    /**
     * Draws a horizontal recessed divider line from left_x to right_x at vertical position y.
     */
    static void DrawHorizontal(int left_x, int right_x, int y, int alpha = 0x80);
};

/**
 * Layout container representing a window region with outer bounds and inset content bounds.
 */
class SubWindow {
public:
    /**
     * Constructs an empty sub-window.
     */
    constexpr SubWindow() = default;

    /**
     * Constructs a sub-window with outer bounds, surface preset, and inner content padding.
     */
    constexpr explicit SubWindow(const Rect &bounds, WindowPreset preset = WindowPreset::None,
                                 int pad_x = 0, int pad_y = 0)
        : m_outer(bounds),
          m_content(bounds.Inset(pad_x, pad_y)),
          m_preset(preset) {}

    /**
     * Returns a sub-window spanning the full 640x480 logical screen with optional padding.
     */
    static constexpr SubWindow Screen(int pad_x = 0, int pad_y = 0) {
        return SubWindow(Rect::Screen(), WindowPreset::None, pad_x, pad_y);
    }

    /**
     * Returns the outer bounding rectangle of the sub-window.
     */
    constexpr const Rect &OuterRect() const { return m_outer; }

    /**
     * Returns the inset content bounding rectangle of the sub-window.
     */
    constexpr const Rect &ContentRect() const { return m_content; }

    /**
     * Draws the configured window surface preset over the outer bounds.
     */
    void DrawBackground(int alpha = 0x80) const;

    /**
     * Draws a stone title banner anchored to the top center of the outer bounds.
     */
    void DrawBanner(std::string_view title, int banner_w = Metrics::kBannerWidth,
                    int banner_h = Metrics::kBannerHeight, int y_offset = -17,
                    int alpha = 0x80) const;

    /**
     * Computes a child rectangle anchored within the sub-window's content bounds.
     */
    constexpr Rect AnchorRect(Anchor anchor, int w, int h, int dx = 0, int dy = 0) const {
        return m_content.Anchored(anchor, w, h, dx, dy);
    }

    /**
     * Draws a text label anchored within the sub-window's content bounds.
     */
    void DrawAnchoredLabel(std::string_view text, Anchor anchor, const TextStyle &style = {},
                           int dx = 0, int dy = 0, int alpha = 0x80) const;

    /**
     * Splits the content area into left and right child sub-windows with an optional vertical divider.
     */
    std::pair<SubWindow, SubWindow> SplitVertical(int left_w, int gap = Metrics::kPaneGap,
                                                  bool draw_divider = false, int alpha = 0x80) const;

    /**
     * Splits the content area into top and bottom child sub-windows with an optional horizontal divider.
     */
    std::pair<SubWindow, SubWindow> SplitHorizontal(int top_h, int gap = Metrics::kPaneGap,
                                                    bool draw_divider = false, int alpha = 0x80) const;

private:
    Rect         m_outer{};                       /**< Outer window bounding rectangle. */
    Rect         m_content{};                     /**< Inset inner content bounding rectangle. */
    WindowPreset m_preset{WindowPreset::None};    /**< Background surface preset. */
};

} // namespace UI