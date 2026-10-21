#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct PortDecodedTexture;

namespace caption {

constexpr unsigned char kFill[3] = {232, 226, 183};
constexpr unsigned char kOutline[3] = {1, 0, 201};

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

bool Ready();

bool ThinReady();

bool StatusReady();

/**
 * Painted caption bounds in logical pixels relative to its label sprite.
 */
struct CaptionBounds {
    float left, top, width, height;
};

/**
 * Measures stayframe labels without altering the original texture.
 */
void PreparePainted(const char *name, const PortDecodedTexture &texture);
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

// A floor label's letter (counted from 0, spaces not counted) drawn widen pixels wider, the letters after it moved along
// with it, it and the letters after it moved by move, and it alone raised by raise.
struct FloorAdjust {
    int   letter;
    float widen = 0;
    float move = 0;
    float raise = 0;
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
 * Draws a name as the floor-select board's painted names look (the boss floors'): pale yellow letters of the narrower
 * set in a thin navy outline, each as wide as the painting has that letter and spaced as it spaces them (the stretch
 * made before it is outlined), the whole squeezed by squeeze as the painting squeezes a long name, from left_x, the
 * caps from cap_top; adjust changes how far a letter reaches either side, and where the rest of its word goes, in
 * this name alone. Only what lies between clip_top and clip_bottom is drawn (a row the board's list has scrolled
 * part way out). False when there is no
 * font to draw it with.
 */
struct BoardAdjust {
    char32_t ch;
    float    left, right; // added to how far every ch in the name reaches past its place either side
    float    advance = 0; // added to where the rest of its word goes (a space's: the rest of the name)
    float    move = 0;    // how far it is drawn right of its place, the letters after it staying put
    int      nth = -1;    // which ch (from 0) advance and move are for, or every one; left and right are every one's
};
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

    float letter = 0.0f, word = 0.0f, xscale = 1.0f, yscale = 1.0f, dx = 0.0f, dy = 0.0f;
    float weight = 0.0f, opacity = 1.0f;
    float alpha = 1.0f; // how opaque the line is drawn (opacity darkens it instead)

    bool ink_fit = false;
    bool thin = false;
    bool status_font = false;

    const FillField *field = nullptr;
    float            field_x = 0.0f, field_y = 0.0f, field_w = 1.0f, field_h = 1.0f;

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
    bool  blank;
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

} // namespace caption
