#pragma once

#include <cstdint>
#include <functional>
#include <utility>

#include "gameutil.hpp"
#include "rect.hpp"

namespace dc::ui {

/**
 * Standard white font palette index.
 */
constexpr u32 ColorWhite = game::FONT_COLOR_WHITE;

/**
 * Standard gold font palette index.
 */
constexpr u32 ColorGold = game::FONT_COLOR_GOLD;

/**
 * Standard yellow font palette index.
 */
constexpr u32 ColorYellow = game::FONT_COLOR_YELLOW;

/**
 * Standard blue font palette index.
 */
constexpr u32 ColorBlue = 3;

namespace metrics {

/**
 * Logical screen width in pixels.
 */
constexpr int kScreenWidth = 640;

/**
 * Logical screen height in pixels.
 */
constexpr int kScreenHeight = 480;

/**
 * Horizontal center of the logical screen in pixels.
 */
constexpr int kScreenCenterX = 320;

/**
 * Vertical center of the logical screen in pixels.
 */
constexpr int kScreenCenterY = 240;

/**
 * Default text scale factor for primary buttons.
 */
constexpr float kScaleButton = 1.08f;

/**
 * Default unit text scale factor.
 */
constexpr float kScaleNormal = 1.00f;

/**
 * Default text scale factor for section headers.
 */
constexpr float kScaleHeader = 0.92f;

/**
 * Default text scale factor for body copy.
 */
constexpr float kScaleBody = 0.84f;

/**
 * Default text scale factor for compact labels.
 */
constexpr float kScaleSmall = 0.76f;

/**
 * Standard horizontal outer window margin in logical pixels.
 */
constexpr int kWindowMarginX = 28;

/**
 * Standard vertical outer window margin in logical pixels.
 */
constexpr int kWindowMarginY = 18;

/**
 * Default inner padding for sub-windows in logical pixels.
 */
constexpr int kSubWindowPadding = 12;

/**
 * Default gap between split sub-window panes in logical pixels.
 */
constexpr int kPaneGap = 16;

/**
 * Default margin around divider lines in logical pixels.
 */
constexpr int kDividerMargin = 10;

/**
 * Standard list row height in logical pixels.
 */
constexpr int kRowHeight = 22;

/**
 * Standard vertical stride between list rows in logical pixels.
 */
constexpr int kRowStride = 25;

/**
 * Small item icon dimension in logical pixels.
 */
constexpr int kIconSizeSmall = 20;

/**
 * Standard item icon dimension in logical pixels.
 */
constexpr int kIconSizeStandard = 24;

/**
 * Horizontal spacing between an icon and its label in logical pixels.
 */
constexpr int kIconTextGap = 12;

/**
 * Total horizontal width of a bracketed numeric counter in logical pixels.
 */
constexpr int kCounterWidth = 82;

/**
 * Default modal button width in logical pixels.
 */
constexpr int kButtonWidth = 100;

/**
 * Default modal button height in logical pixels.
 */
constexpr int kButtonHeight = 26;

/**
 * Vertical gap between stacked modal buttons in logical pixels.
 */
constexpr int kButtonGap = 6;

/**
 * Default stone title banner width in logical pixels.
 */
constexpr int kBannerWidth = 340;

/**
 * Default stone title banner height in logical pixels.
 */
constexpr int kBannerHeight = 28;

/**
 * Default progress bar height in logical pixels.
 */
constexpr int kProgressBarHeight = 22;

/**
 * Minimum width of a modal dialog box in logical pixels.
 */
constexpr int kModalMinWidth = 260;

/**
 * Maximum width of a modal dialog box in logical pixels.
 */
constexpr int kModalMaxWidth = 360;

} // namespace metrics

/**
 * Horizontal text alignment modes.
 */
enum class Alignment {
    Left,
    Center,
    Right,
};

/**
 * TrueType font weight and slot variants.
 */
enum class FontWeight {
    Thin,
    Normal,
    Bold,
    Status,
    Title, /**< The title menu's lettering (Chronicle Title Bold), as the title's painted rows have it. */
};

/**
 * Rectangular layout anchor positions.
 */
enum class Anchor {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    TopWide,
    BottomWide,
    LeftWide,
    RightWide,
    FullRect,
};

/**
 * Axis-aligned 2D rectangle in logical screen coordinates.
 */
struct Rect {
    int x = 0; /**< Left X coordinate in logical pixels. */
    int y = 0; /**< Top Y coordinate in logical pixels. */
    int w = 0; /**< Width in logical pixels. */
    int h = 0; /**< Height in logical pixels. */

    /**
     * Constructs an empty rectangle at the origin.
     */
    constexpr Rect() = default;

    /**
     * Constructs a rectangle with the given position and dimensions.
     */
    constexpr Rect(int in_x, int in_y, int in_w, int in_h)
        : x(in_x), y(in_y), w(in_w), h(in_h) {}

    /**
     * Returns a rectangle covering the full 640x480 logical screen.
     */
    static constexpr Rect Screen() {
        return Rect(0, 0, metrics::kScreenWidth, metrics::kScreenHeight);
    }

    /**
     * Returns the left X coordinate.
     */
    constexpr int Left() const { return x; }

    /**
     * Returns the right X coordinate.
     */
    constexpr int Right() const { return x + w; }

    /**
     * Returns the top Y coordinate.
     */
    constexpr int Top() const { return y; }

    /**
     * Returns the bottom Y coordinate.
     */
    constexpr int Bottom() const { return y + h; }

    /**
     * Returns the horizontal center X coordinate.
     */
    constexpr int CenterX() const { return x + (w / 2); }

    /**
     * Returns the vertical center Y coordinate.
     */
    constexpr int CenterY() const { return y + (h / 2); }

    /**
     * Returns whether the rectangle has zero or negative area.
     */
    constexpr bool Empty() const { return w <= 0 || h <= 0; }

    /**
     * Returns whether the point (px, py) lies inside the rectangle.
     */
    constexpr bool Contains(float px, float py) const {
        return px >= static_cast<float>(x) && px < static_cast<float>(x + w) &&
               py >= static_cast<float>(y) && py < static_cast<float>(y + h);
    }

    /**
     * Returns a copy of the rectangle translated by (dx, dy).
     */
    constexpr Rect Offset(int dx, int dy) const {
        return Rect(x + dx, y + dy, w, h);
    }

    /**
     * Returns a copy of the rectangle inset uniformly on all four sides.
     */
    constexpr Rect Inset(int all) const {
        return Inset(all, all, all, all);
    }

    /**
     * Returns a copy of the rectangle inset horizontally by pad_x and vertically by pad_y.
     */
    constexpr Rect Inset(int pad_x, int pad_y) const {
        return Inset(pad_x, pad_y, pad_x, pad_y);
    }

    /**
     * Returns a copy of the rectangle inset by individual edge margins.
     */
    constexpr Rect Inset(int left, int top, int right, int bottom) const {
        const int inner_w = w - (left + right);
        const int inner_h = h - (top + bottom);
        return Rect(x + left, y + top, inner_w > 0 ? inner_w : 0, inner_h > 0 ? inner_h : 0);
    }

    /**
     * Computes a child rectangle aligned to the specified anchor within this rectangle.
     */
    constexpr Rect Anchored(Anchor anchor, int child_w, int child_h,
                            int offset_x = 0, int offset_y = 0) const {
        int cx = x;
        int cy = y;
        int cw = child_w;
        int ch = child_h;

        switch (anchor) {
            case Anchor::TopLeft:
                cx = x;
                cy = y;
                break;
            case Anchor::TopCenter:
                cx = x + (w - cw) / 2;
                cy = y;
                break;
            case Anchor::TopRight:
                cx = x + w - cw;
                cy = y;
                break;
            case Anchor::CenterLeft:
                cx = x;
                cy = y + (h - ch) / 2;
                break;
            case Anchor::Center:
                cx = x + (w - cw) / 2;
                cy = y + (h - ch) / 2;
                break;
            case Anchor::CenterRight:
                cx = x + w - cw;
                cy = y + (h - ch) / 2;
                break;
            case Anchor::BottomLeft:
                cx = x;
                cy = y + h - ch;
                break;
            case Anchor::BottomCenter:
                cx = x + (w - cw) / 2;
                cy = y + h - ch;
                break;
            case Anchor::BottomRight:
                cx = x + w - cw;
                cy = y + h - ch;
                break;
            case Anchor::TopWide:
                cx = x;
                cy = y;
                cw = w;
                break;
            case Anchor::BottomWide:
                cx = x;
                cy = y + h - ch;
                cw = w;
                break;
            case Anchor::LeftWide:
                cx = x;
                cy = y;
                ch = h;
                break;
            case Anchor::RightWide:
                cx = x + w - cw;
                cy = y;
                ch = h;
                break;
            case Anchor::FullRect:
                cx = x;
                cy = y;
                cw = w;
                ch = h;
                break;
        }
        return Rect(cx + offset_x, cy + offset_y, cw, ch);
    }

    /**
     * Splits the rectangle into left and right panes separated by a horizontal gap.
     */
    constexpr std::pair<Rect, Rect> SplitVertical(int left_w, int gap = metrics::kPaneGap,
                                                  int *out_divider_x = nullptr) const {
        if (out_divider_x != nullptr) {
            *out_divider_x = x + left_w + (gap / 2);
        }
        const int right_x = x + left_w + gap;
        const int right_w = (x + w > right_x) ? (x + w - right_x) : 0;
        return {Rect(x, y, left_w, h), Rect(right_x, y, right_w, h)};
    }

    /**
     * Splits the rectangle into top and bottom panes separated by a vertical gap.
     */
    constexpr std::pair<Rect, Rect> SplitHorizontal(int top_h, int gap = metrics::kPaneGap,
                                                    int *out_divider_y = nullptr) const {
        if (out_divider_y != nullptr) {
            *out_divider_y = y + top_h + (gap / 2);
        }
        const int bot_y = y + top_h + gap;
        const int bot_h = (y + h > bot_y) ? (y + h - bot_y) : 0;
        return {Rect(x, y, w, top_h), Rect(x, bot_y, w, bot_h)};
    }

    /**
     * Returns the bounding rectangle of the Nth vertical row inside this rectangle.
     */
    constexpr Rect Row(int index, int row_h = metrics::kRowHeight,
                       int stride = metrics::kRowStride) const {
        return Rect(x, y + index * stride, w, row_h);
    }
};

/**
 * Rendering layer priorities for deferred UI draw calls.
 */
enum class ZLayer : int {
    Background = 0,
    Window = 10,
    Recess = 20,
    Content = 30,
    Widgets = 40,
    Brackets = 50,
    Modal = 60,
    Overlay = 70,
    Cursor = 80,
};

/**
 * Deferred draw queue that sorts and flushes UI draw callbacks by Z-layer.
 */
class LayerQueue {
public:
    /**
     * Clears the deferred draw queue at the start of a frame.
     */
    static void Begin();

    /**
     * Enqueues a draw callback to execute at the given Z-layer priority.
     */
    static void Submit(ZLayer layer, std::function<void()> draw_fn);

    /**
     * Sorts and executes all queued draw callbacks in ascending Z-layer order.
     */
    static void Flush();
};

} // namespace dc::ui
