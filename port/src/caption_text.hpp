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
 * Draws a character's name as the painted name alphabet looks (the ally card's): near-white letters drawn wide (the
 * stretch made before they are outlined) in the menus' blue outline swept a pixel down and right for depth, over a
 * near-black copy of it two pixels further; centred on center_x, the letters' cells from top_y, as wide as
 * max_width. False when there is no font to draw it with.
 */
bool DrawName(const std::u32string &text, float center_x, float top_y, float max_width, int alpha);

/**
 * Draws a name as the floor-select board's painted names look (the boss floors'): pale yellow letters of the narrower
 * set in a thin navy outline, each as wide as the painting has that letter and spaced as it spaces them (the stretch
 * made before it is outlined), the whole squeezed by squeeze as the painting squeezes a long name, from left_x, the
 * caps from cap_top; adjust changes how far a letter reaches either side in this name alone. False when there is no
 * font to draw it with.
 */
struct BoardAdjust {
    char32_t ch;
    float    left, right; // added to how far the letter reaches past its place either side
};
bool DrawBoardName(std::string_view utf8, float left_x, float cap_top, float squeeze, std::span<const BoardAdjust> adjust,
                   int alpha);

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
