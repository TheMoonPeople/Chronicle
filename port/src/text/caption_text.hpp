#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc::render {
struct PortDecodedTexture;
} // namespace dc::render

namespace dc::text::caption {

/**
 * RGB of the menu captions' painted letter fill.
 */
constexpr unsigned char kFill[3] = {232, 226, 183};
/**
 * RGB of the menu captions' painted outline.
 */
constexpr unsigned char kOutline[3] = {1, 0, 201};

/**
 * Number of main menu icons that carry a caption.
 */
constexpr int kIcons = 14;

/**
 * Overrides the localization key and English fallback used by the next Draw() call.
 */
void SetNextCaption(const char *key, const char *english);

/**
 * Overrides the text drawn by the next Draw() call.
 */
void SetNextCaptionText(std::string_view text);

/**
 * Caption translation keys and painted English defaults.
 */
std::vector<std::pair<std::string, std::string>> Strings();

/**
 * Whether the caption font is loaded and captions can draw.
 */
bool Ready();

/**
 * Whether the caption font and its narrower variant are loaded.
 */
bool ThinReady();

/**
 * Whether the caption font and the status font are loaded.
 */
bool StatusReady();

/**
 * Whether the title menu's font (Chronicle Title) can draw.
 */
bool TitleReady();

/**
 * Painted caption bounds in logical pixels relative to its label sprite.
 */
struct CaptionBounds {
    float left;
    float top;
    float width;
    float height;
};

/**
 * Measures stayframe labels without altering the original texture.
 */
void PreparePainted(const char *name, const render::PortDecodedTexture &texture);
/**
 * Returns measured bounds, or no value for an absent label.
 */
std::optional<CaptionBounds> PaintedBounds(int icon);

/**
 * Draws within the painted bounds; false requests the bitmap fallback.
 */
bool Draw(int icon, int x, int y, int box_width, int alpha, int bright = 0x80);

/**
 * Draws a title in the menu captions' painted look over a painted one (the name entry's REGISTER NAME, the load screen's
 * LOAD): its letters stretched over the painted letters' fill, which runs from left to right of the picture at x, y and
 * from its capitals' top to bottom, a space space ems, after[i] pixels more after letter i and letter i narrow[i] times
 * as wide (spaces not counted). False with no font.
 */
bool DrawTitle(std::string_view utf8, int x, int y, float left, float right, float top, float bottom, float space,
               std::span<const float> after, int alpha, int bright = 0x80, std::span<const float> narrow = {});

/**
 * Draws one of the weapon menu's button words (EQUIP, CUSTOMIZE...) as the painted buttons letter it, each letter where
 * the painting has it in the 96 by 23 button at x, y, tinted by bright (0x80 as painted); pressed, in a light glow
 * instead of the dark one. False for a word it has no painting of (another language's), or no font.
 */
bool DrawButtonWord(std::string_view word, float x, float y, int bright, int alpha, bool pressed = false);

/**
 * A per-letter adjustment to a floor label drawn by DrawFloorLabel().
 */
struct FloorAdjust {
    int   letter;    /**< Letter index from 0, spaces not counted. */
    float widen = 0; /**< Pixels wider; the letters after it move along with it. */
    float move = 0;  /**< Pixels it and the letters after it move right. */
    float raise = 0; /**< Pixels it alone is raised. */
};

/**
 * Draws a dungeon-entry floor label ("Floor", "Territory"...) as the game paints them: bold warm yellow letters nearly
 * touching, outlined in navy as one word with the outline swept down and right for depth, the letters' ink stretched
 * to the box (left to right, top to bottom; with by_capitals, the capitals standing from top to the baseline at bottom
 * instead; with squeeze, its letters that many times as wide as tall as the font has them, from left); rows outside
 * clip_top to clip_bottom are left out. False with no font.
 */
bool DrawFloorLabel(std::string_view utf8, float left, float top, float right, float bottom, float clip_top, float clip_bottom,
                    int alpha, std::span<const FloorAdjust> adjust = {}, bool by_capitals = false, float squeeze = 0.0f);

/**
 * Draws a cell of the name alphabet's sheet (the 22-pixel cell at u, v; the game cuts it 21 to 23 rows tall) stretched
 * to the screen rectangle as the game draws it, its character drawn from the font in the painted alphabet's look, the
 * game's colour (0x80 as painted) and alpha over it. False for a cell it has no character for, or no font.
 */
bool DrawAlphabetCell(int u, int v, int texel_width, int texel_height, float x, float y, float width, float height, int r,
                      int g, int b, int alpha);

/**
 * A per-letter adjustment to a name drawn by DrawBoardName().
 */
struct BoardAdjust {
    char32_t ch;          /**< The letter adjusted. */
    float    left;        /**< Added to how far every ch in the name reaches past its place on the left. */
    float    right;       /**< Added to how far every ch in the name reaches past its place on the right. */
    float    advance = 0; /**< Added to where the rest of its word goes (a space's: the rest of the name). */
    float    move = 0;    /**< How far it is drawn right of its place, the letters after it staying put. */
    int      nth = -1;    /**< Which ch (from 0) advance and move apply to, or -1 for every one. */
};

/**
 * Draws a name as the floor-select board's painted names look (the boss floors'): pale yellow letters of the narrower
 * set in a thin navy outline, each as wide as the painting has that letter and spaced as it spaces them (the stretch
 * made before it is outlined), the whole squeezed by squeeze as the painting squeezes a long name, from left_x, the
 * caps from cap_top; adjust changes how far a letter reaches either side, and where the rest of its word goes, in
 * this name alone. Only what lies between clip_top and clip_bottom is drawn (a row the board's list has scrolled
 * part way out). False when there is no font to draw it with.
 */
bool DrawBoardName(std::string_view utf8, float left_x, float cap_top, float squeeze, std::span<const BoardAdjust> adjust,
                   float clip_top, float clip_bottom, int alpha);

/**
 * RGB ink colors extended across a width-by-height field for glyph sampling.
 */
struct FillField {
    int                        width = 0;
    int                        height = 0;
    std::vector<unsigned char> rgb;    /**< width * height * 3 bytes. */
    unsigned                   id = 0; /**< Distinguishes fields in the colored-glyph cache. */
};

/**
 * Runtime lettering style in logical units, with optional absolute glyph slots.
 */
struct LineStyle {
    unsigned char fill[3] = {255, 255, 255};
    unsigned char edge[3] = {0, 0, 0};
    unsigned char glow[3] = {0, 0, 0};
    float         edge_width = 1.0f;
    float         glow_width = 0.0f;
    float         glow_amount = 0.0f;

    float letter = 0.0f; /**< Extra space between letters. */
    float word = 0.0f;   /**< Extra space for a word space. */
    float xscale = 1.0f;
    float yscale = 1.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    float weight = 0.0f;  /**< Extra stroke weight, drawn as offset copies of the fill. */
    float opacity = 1.0f; /**< Darkens the line's colours. */
    float alpha = 1.0f;   /**< How opaque the line is drawn. */

    bool  ink_fit = false; /**< Fits the letters' ink exactly to the cap height. */
    bool  thin = false;
    bool  status_font = false;
    bool  title_font = false;
    float kern = 0.0f; /**< Strength of the font's kerning between letters (0 for none). */
    /**
     * Letters set by their outlines rather than their pictures. A picture's soft edge is a pixel of the window, so its
     * share of a logical pixel, and with it the spacing, would change with the window's size.
     */
    bool ink_spacing = false;

    const FillField *field = nullptr; /**< Optional field the fill colours are sampled from. */
    float            field_x = 0.0f;
    float            field_y = 0.0f;
    float            field_w = 1.0f;
    float            field_h = 1.0f;

    std::vector<std::array<float, 2>> slots; /**< Absolute left/right ink bounds per non-space glyph. */
};

/**
 * Horizontal ink extent excluding outlines and glows.
 */
struct LineBounds {
    float left = 0.0f;
    float right = 0.0f;
};

/**
 * Natural glyph ink width, including entries for word spaces.
 */
struct LetterWidth {
    float width;
    bool  blank; /**< A word space rather than a glyph. */
};

/**
 * Measures the font for segmenting matching painted strings.
 */
std::vector<LetterWidth> LetterWidths(std::string_view utf8);

/**
 * Draws centered lettering; cap_top and cap_height bound its ink when ink_fit is enabled.
 */
bool DrawLine(std::string_view utf8, float center_x, float cap_top, float cap_height, float xsq, float max_width,
              const LineStyle &style, float fade, float rows, LineBounds *bounds);

} // namespace dc::text::caption
