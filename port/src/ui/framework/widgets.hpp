#pragma once

#include <string_view>

#include "texture.hpp"
#include "ui/framework/types.hpp"

namespace dc::ui {

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
    static bool Draw(int item_no, int x, int y, int size = metrics::kIconSizeStandard, int alpha = 0x80,
                     bool drop_shadow = true, bool with_socket = false);

    /**
     * Draws item_no's icon in an item board's slot at (x, y), as retail's boards draw it: the whole
     * 32-pixel cell with its shadow, cut to the rows from top to bottom (the board's window).
     */
    static void DrawInSlot(int item_no, int x, int y, int top, int bottom, int alpha = 0x80);
};

/**
 * A region of one of the game's pictures, stretched over a rectangle.
 */
class Picture {
public:
    /**
     * Draws the source region of texture over the destination.
     */
    static void Draw(game::CTexture *texture, const Rect &destination, const Rect &source, int alpha = 0x80);

    /**
     * Draws a column of three pieces from texture, as retail's list boards draw their scroll
     * indicator: the cap (cap_source), length pixels of the body (the cap_source.h rows below it)
     * stretched, and the foot (the rows below those) after it.
     */
    static void DrawColumn(game::CTexture *texture, int x, int y, float length, const Rect &cap_source, int alpha = 0x80);
};

/**
 * A number in a picture's digits, as retail's menus draw prices and ranks: digit 0 at digits, the
 * others to its right, each digits.w wide, drawn leftward from right_x and overlapping by overlap.
 */
class PictureNumber {
public:
    /**
     * Draws value ending at right_x.
     */
    static void Draw(int value, int right_x, int y, game::CTexture *texture, const Rect &digits, int overlap, int alpha = 0x80);
};

/**
 * Retail's menu hand: its sprite in the stay-frame picture, easing a quarter of the way to where it
 * points each frame and swinging with the count every menu shares. It goes to the mouse while a
 * screen's pointer is shown (MenuPointerShow). Cursor is the framework's own hand; this one is
 * retail's, for screens that look as retail's do.
 */
class MenuHand {
public:
    /**
     * Puts the hand at (x, y) at once.
     */
    void SnapTo(float x, float y);

    /**
     * Moves the hand a quarter of the way to (x, y).
     */
    void Ease(float x, float y);

    /**
     * Where the hand is.
     */
    float X() const { return m_x; }
    float Y() const { return m_y; }

    /**
     * Draws the hand with its shadow; icon_u picks the hand in the picture's row of them.
     */
    void Draw(int icon_u = 0x40) const;

private:
    float m_x{0.0f}; /**< Where the hand is. */
    float m_y{0.0f}; /**< Where the hand is. */
};

/**
 * Retail's stay frame: the four pulsing corners a menu puts around what its hand is on.
 */
class StayFrame {
public:
    /**
     * Draws the corners around box.
     */
    static void Draw(const Rect &box, int alpha = 0x80);
};

/**
 * Where a list board's rows are: rows_shown of them, the first at first_row, each stride below the
 * last.
 */
struct ListLayout {
    Rect first_row{};     /**< The first row in view: what a pointer finds it by. */
    int  stride = 0x22;   /**< From one row to the next. */
    int  rows_shown = 5;  /**< Rows in view. */
};

/**
 * A list board's cursor, as retail's lists move it (the fish exchange, the fishing records): Up and
 * Down a row, the rows in view following; a page of rows_shown at a time with L and R. Retail lets a
 * page down leave the view one row past the end, and so does this. It eases the hand's height a
 * quarter of the way (whole pixels, at once within two) and says which row is where.
 */
class ListCursor {
public:
    ListCursor() = default;
    explicit ListCursor(const ListLayout &layout) : m_layout(layout) {}

    /**
     * The cursor on the first of last + 1 entries, the first rows in view, the hand at hand_y.
     */
    void Reset(int last, int hand_y);

    /**
     * Changes the last entry, as a list that grows or shrinks does.
     */
    void SetLast(int last) { m_last = last; }

    void Up();       /**< A row up. */
    void Down();     /**< A row down. */
    void PageUp();   /**< A page up. */
    void PageDown(); /**< A page down. */

    /**
     * Keeps the cursor on an entry and the rows in view inside the list, as retail does after every
     * move.
     */
    void Clamp();

    /**
     * Puts the cursor and the first row in view where a screen's own rule puts them.
     */
    void Set(int cursor, int top);

    /**
     * Eases the hand's height toward target_y.
     */
    void EaseHand(int target_y);

    /**
     * Puts the hand at height y at once.
     */
    void SetHandY(int y) { m_hand_y = y; }

    int Cursor() const { return m_cursor; } /**< The entry the cursor is on. */
    int Top() const { return m_top; }       /**< The first entry in view. */
    int Last() const { return m_last; }     /**< The last entry. */
    int HandY() const { return m_hand_y; }  /**< Where the hand is. */
    const ListLayout &Layout() const { return m_layout; }

    /**
     * Where row (0 for the first in view) is.
     */
    Rect RowRect(int row) const;

    /**
     * The entry under (x, y), -1 for none.
     */
    int Hit(float x, float y) const;

private:
    ListLayout m_layout{};
    int        m_cursor{0};
    int        m_top{0};
    int        m_last{0};
    int        m_hand_y{0};
};

/**
 * Where an item board's slots are: columns of slots stride_x apart and rows stride_y apart, seen
 * through a window rows_shown rows high.
 */
struct SlotGridLayout {
    Rect window{};       /**< The slots' window: the first row's place, and what is cut away outside it. */
    int  columns = 5;    /**< Slots in a row. */
    int  stride_x = 42;  /**< From one slot to the next. */
    int  stride_y = 40;  /**< From one row to the next. */
    int  rows_shown = 2; /**< Rows the window shows. */
};

/**
 * An item board's slots, as retail's boards move through them: the cursor goes a slot left or right
 * or a row up or down, never past the ends, empty slots included; the rows in view follow it, and the
 * rows ease a quarter of the way (whole pixels, at once within four) to where they belong each frame.
 * It says where each slot is drawn now, so the mouse finds what the eye sees.
 */
class SlotGrid {
public:
    SlotGrid() = default;
    explicit SlotGrid(const SlotGridLayout &layout) : m_layout(layout) {}

    /**
     * Puts the cursor on slot 0 of slots, the first rows in view, the rows where they belong.
     */
    void Reset(int slots);

    /**
     * Moves the cursor dx slots and dy rows, as far as the board lets it: true when it moved.
     */
    bool Move(int dx, int dy);

    /**
     * Puts the cursor on a slot, keeping its row in view.
     */
    void SetCursor(int slot);

    /**
     * Eases the rows toward where they belong; once a frame.
     */
    void Ease();

    int Cursor() const { return m_cursor; }   /**< The slot the cursor is on. */
    int TopRow() const { return m_top_row; }  /**< The first row in view. */
    int Rows() const;                         /**< Rows of slots, one at least. */
    int RowsY() const { return m_rows_y; }    /**< Where the first row is drawn now. */
    const SlotGridLayout &Layout() const { return m_layout; }

    /**
     * Where slot is drawn now, the rows' easing included.
     */
    Rect SlotRect(int slot) const;

    /**
     * Where the cursor's slot will rest, its row in view: where the hand and frame go.
     */
    Rect CursorRect() const;

    /**
     * The slot under (x, y) among the first filled, inside the window; -1 for none.
     */
    int Hit(float x, float y, int filled) const;

private:
    void Follow();

    SlotGridLayout m_layout{};
    int            m_slots{0};
    int            m_cursor{0};
    int            m_top_row{0};
    int            m_rows_y{0};
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
    float           m_cur_x{0.0f};      /**< Current interpolated X coordinate. */
    float           m_cur_y{0.0f};      /**< Current interpolated Y coordinate. */
    float           m_target_x{0.0f};   /**< Target X coordinate. */
    float           m_target_y{0.0f};   /**< Target Y coordinate. */
    bool            m_pointing{false};  /**< Whether explicit pointer override is enabled. */
    float           m_pointer_x{0.0f};  /**< Explicit pointer X coordinate. */
    float           m_pointer_y{0.0f};  /**< Explicit pointer Y coordinate. */
    int             m_anim_count{0};    /**< Oscillation animation frame counter. */
    game::CTexture *m_texture{nullptr}; /**< Cached pointer to the gaiji sprite sheet. */
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
    int             m_beat{0};          /**< Pulse animation frame counter. */
    game::CTexture *m_texture{nullptr}; /**< Cached pointer to the option2 texture. */
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
    int             m_bar_frames{0};      /**< Remaining active highlight frames after movement. */
    game::CTexture *m_track_tex{nullptr}; /**< Cached pointer to the track texture. */
    game::CTexture *m_thumb_tex{nullptr}; /**< Cached pointer to the thumb and arrow texture. */
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

} // namespace dc::ui
