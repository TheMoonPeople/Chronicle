#pragma once

#include <array>
#include <optional>
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
 * Caption translation keys and painted English defaults.
 */
std::vector<std::pair<std::string, std::string>> Strings();

bool Ready();

bool ThinReady();

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
