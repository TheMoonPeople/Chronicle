#pragma once

#include <string_view>

#include "texture.hpp"
#include "ui_framework/types.hpp"

namespace UI {

/**
 * Inventory item and weapon icon renderer with optional recessed socket backing.
 */
class ItemIcon {
public:
    /**
     * Draws a square recessed icon socket at (x, y).
     */
    static void DrawSocket(int x, int y, int size = 26, bool filled = false, int alpha = 0x80);

    /**
     * Draws the icon for item_no at (x, y) with optional drop shadow and socket backing.
     */
    static bool Draw(int item_no, int x, int y, int size = Metrics::kIconSizeStandard, int alpha = 0x80,
                     bool drop_shadow = true, bool with_socket = false);
};

/**
 * Animated pointing-hand cursor supporting smooth target interpolation and mouse tracking.
 */
class Cursor {
public:
    /**
     * Constructs a hand cursor bound to the system gaiji texture.
     */
    Cursor();

    /**
     * Sets the target position that the cursor smoothly interpolates toward.
     */
    void SetTarget(float x, float y);

    /**
     * Immediately teleports the cursor and its target to (x, y).
     */
    void SnapTo(float x, float y);

    /**
     * Locks the cursor to an explicit pointer position.
     */
    void SetPointer(float x, float y);

    /**
     * Releases explicit pointer override so normal target interpolation resumes.
     */
    void ClearPointer();

    /**
     * Returns whether explicit pointer override is active.
     */
    bool IsPointing() const { return m_pointing; }

    /**
     * Updates the cursor animation and draws the hand sprite immediately.
     */
    void Draw(int alpha = 0x80);

    /**
     * Enqueues the cursor to be drawn on the top-most ZLayer::Cursor layer.
     */
    void DeferDraw(int alpha = 0x80);

private:
    float     m_cur_x{0.0f};      /**< Current interpolated X coordinate. */
    float     m_cur_y{0.0f};      /**< Current interpolated Y coordinate. */
    float     m_target_x{0.0f};   /**< Target X coordinate. */
    float     m_target_y{0.0f};   /**< Target Y coordinate. */
    bool      m_pointing{false};  /**< Whether explicit pointer override is enabled. */
    float     m_pointer_x{0.0f};  /**< Explicit pointer X coordinate. */
    float     m_pointer_y{0.0f};  /**< Explicit pointer Y coordinate. */
    int       m_anim_count{0};    /**< Oscillation animation frame counter. */
    CTexture *m_texture{nullptr}; /**< Cached pointer to the gaiji sprite sheet. */
};

/**
 * Pulsing four-corner selection brackets for list rows and buttons.
 */
class Brackets {
public:
    /**
     * Constructs selection brackets bound to the option2 texture.
     */
    Brackets();

    /**
     * Draws pulsing corner brackets around the specified edges.
     */
    void Draw(int left, int right, int top, int bottom, int alpha = 0x80);

    /**
     * Draws pulsing corner brackets around a bounding rectangle.
     */
    void Draw(const Rect &box, int alpha = 0x80) {
        Draw(box.Left(), box.Right(), box.Top(), box.Bottom(), alpha);
    }

    /**
     * Enqueues the brackets to be drawn on ZLayer::Brackets.
     */
    void DeferDraw(const Rect &box, int alpha = 0x80);

private:
    int       m_beat{0};          /**< Pulse animation frame counter. */
    CTexture *m_texture{nullptr}; /**< Cached pointer to the option2 texture. */
};

/**
 * Vertical scrollbar with proportional thumb sizing and arrows.
 */
class ScrollBar {
public:
    /**
     * Constructs a scrollbar bound to the menu track and thumb textures.
     */
    ScrollBar();

    /**
     * Updates the scrollbar visibility timer when list selection changes.
     */
    void Update(bool moved);

    /**
     * Draws the vertical scrollbar track, arrows, and proportional thumb.
     */
    void Draw(int x, int top_y, int height, int total_rows, int visible_rows, int first_row, int alpha = 0x80);

private:
    int       m_bar_frames{0};      /**< Remaining active highlight frames after movement. */
    CTexture *m_track_tex{nullptr}; /**< Cached pointer to the track texture. */
    CTexture *m_thumb_tex{nullptr}; /**< Cached pointer to the thumb and arrow texture. */
};

/**
 * Visual and typographic styling parameters for a progress bar.
 */
struct ProgressBarStyle {
    u32        text_color = ColorWhite;          /**< Label color when progress is below 100%. */
    bool       drop_shadow = true;               /**< Whether to draw a drop shadow behind the overlay text. */
    float      text_scale = 0.78f;               /**< Overlay text scale multiplier. */
    FontWeight text_weight = FontWeight::Normal; /**< Overlay text font weight. */
    u8         fill_r = 28;                      /**< Active bar fill base red channel. */
    u8         fill_g = 68;                      /**< Active bar fill base green channel. */
    u8         fill_b = 45;                      /**< Active bar fill base blue channel. */
    u8         hi_r = 45;                        /**< Active bar highlight red channel. */
    u8         hi_g = 95;                        /**< Active bar highlight green channel. */
    u8         hi_b = 65;                        /**< Active bar highlight blue channel. */
    u8         done_r = 95;                      /**< Completed bar fill base red channel. */
    u8         done_g = 70;                      /**< Completed bar fill base green channel. */
    u8         done_b = 18;                      /**< Completed bar fill base blue channel. */
    u8         done_hi_r = 125;                  /**< Completed bar highlight red channel. */
    u8         done_hi_g = 98;                   /**< Completed bar highlight green channel. */
    u8         done_hi_b = 32;                   /**< Completed bar highlight blue channel. */
};

/**
 * Recessed horizontal progress bar with gradient fill and centered overlay text.
 */
class ProgressBar {
public:
    /**
     * Draws a progress bar inside box for progress in [0.0, 1.0] with optional centered text.
     */
    static void Draw(const Rect &box, float progress, std::string_view text = {},
                     const ProgressBarStyle &style = {}, int alpha = 0x80);
};

/**
 * Oval command icon kinds available in the Battle Menu atlas.
 */
enum class MenuIconKind : int {
    Item = 0,
    Weapon = 1,
    Allies = 2,
    Georama = 3,
    Dungeon = 4,
    WorldMap = 5,
    GoOutside = 6,
    Escape = 7,
    Save = 8,
    Option = 9,
    Manual = 10,
    Load = 11,
    Quit = 12,
};

/**
 * Styling parameters for top-left menu screen headers.
 */
struct MenuHeaderStyle {
    float scale = 1.48f;         /**< Header title scale factor. */
    float xscale = 1.06f;        /**< Horizontal stretch factor. */
    float yscale = 0.94f;        /**< Vertical stretch factor. */
    u8    fill_r = 82;           /**< Main face fill red channel. */
    u8    fill_g = 118;          /**< Main face fill green channel. */
    u8    fill_b = 235;          /**< Main face fill blue channel. */
    float outline_width = 1.85f; /**< Outer rim stroke width in logical pixels. */
    u8    outline_r = 235;       /**< Outer rim stroke red channel. */
    u8    outline_g = 225;       /**< Outer rim stroke green channel. */
    u8    outline_b = 170;       /**< Outer rim stroke blue channel. */
    bool  raised_bevel = true;   /**< Whether to draw the inner 3D extrusion pass. */
    u8    bevel_r = 24;          /**< Inner extrusion red channel. */
    u8    bevel_g = 38;          /**< Inner extrusion green channel. */
    u8    bevel_b = 104;         /**< Inner extrusion blue channel. */
};

/**
 * Top-left menu header combining a Battle Menu oval badge icon with a styled TrueType title.
 */
class MenuHeader {
public:
    /**
     * Draws the oval badge for icon and the styled title string at (x, y).
     */
    static void Draw(MenuIconKind icon, std::string_view title,
                     int x = 16, int y = 26, int alpha = 0x80,
                     const MenuHeaderStyle &style = {});
};

} // namespace UI
