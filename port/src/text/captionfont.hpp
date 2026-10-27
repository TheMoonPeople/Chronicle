#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include "gfx/gfx.hpp"

namespace dc::text::capfont {

/**
 * Loads the main caption font into slot 0 and clears the glyph atlas.
 */
bool Load(const std::filesystem::path &file);

/**
 * Whether the main caption font is loaded.
 */
bool Loaded();

/**
 * A picture in the glyph atlas: where it sits and how it is placed against the pen.
 */
struct Glyph {
    float u0, v0, u1, v1; /**< Atlas rectangle, in texels. */
    float x_offset;       /**< Left of the picture from the pen, in pixels. */
    float y_offset;       /**< Top of the picture from the baseline (or box top), in pixels. */
    float width, height;  /**< Picture size, in pixels. */
    float advance = 0;    /**< How far the pen moves on, in pixels. */
};

/**
 * ch in the active font at em_px pixels to the em, added to the atlas if new.
 */
bool GetGlyph(char32_t ch, int em_px, Glyph &out);

/**
 * As GetGlyph, with an outline outline_px pixels wide grown round the letter.
 */
bool GetOutlinedGlyph(char32_t ch, int em_px, int outline_px, Glyph &out);

/**
 * The glyph drawn stretch times as wide (to a sixty-fourth) before it is outlined, so its edges stay the font's own
 * and the outline is as thick at the sides as at the top; with smooth, drawn and outlined four times finer so the
 * outline's edge is anti-aliased as finely as the letter's and its width can be a fraction of a pixel. Letters the
 * font composes have none stretched.
 */
bool GetStretchedGlyph(char32_t ch, int em_px, float outline_px, float stretch, bool smooth, Glyph &out);

/**
 * ch in the active font at em_px pixels to the em as an alpha bitmap w x h, its top left at x0, y0 from the pen.
 */
bool RenderGlyph(char32_t ch, int em_px, std::vector<unsigned char> &alpha, int &w, int &h, int &x0, int &y0);

/**
 * ch in the font in `slot` at `scale` pixels to a font unit, as RenderGlyph draws it. False where the font has no ch.
 * Safe on another thread once the font is loaded: it reads the font and nothing else.
 */
bool RenderGlyphScaled(char32_t ch, int slot, float scale, std::vector<unsigned char> &alpha, int &w, int &h, int &x0,
                       int &y0);

/**
 * How a painted glyph looks, in screen pixels: the letter swept depth_x, depth_y down and right in body (its depth),
 * its face in fill with `strokes` strokes `stroke` wide inside its edge stepping from body to fill, and an outline
 * `outline` wide round the whole in ring; under it, a soft shadow in shade: shadow_alpha dark against the letter moved
 * shadow_x, shadow_y and grown by shadow_grow, fading away over shadow_soft beyond. From the font in `slot`.
 */
struct Paint {
    float         depth_x, depth_y;
    float         stroke;
    int           strokes;
    float         outline;
    unsigned char fill[3], body[3], ring[3];
    float         shadow_x = 0.0f, shadow_y = 0.0f, shadow_grow = 0.0f, shadow_soft = 0.0f, shadow_alpha = 0.0f;
    unsigned char shade[3] = {0, 0, 0};
    bool          shadow_only = false; /**< The shadow alone, the letter left out. */
    /**
     * A band of the shadow's colour: band_alpha strong within band_core of band_y (from the box's top) and from
     * band_left to band_right (from its left; the letters' span when band_right is not past band_left), fading over
     * band_fade beyond.
     */
    float band_alpha = 0.0f, band_y = 0.0f, band_core = 0.0f, band_fade = 1.0f, band_left = 0.0f, band_right = 0.0f;
    int   slot = 0;
};

/**
 * ch in the font in paint.slot (with part >= 0 only that piece of it, its pieces counted left to right) with its
 * outline's bounds stretched to box_w x box_h pixels, painted; its offsets are from the box's top left. Every layer is
 * an offset of the outline itself, so each edge is anti-aliased as finely as the letter's.
 */
bool GetPaintedGlyph(char32_t ch, int part, float box_w, float box_h, const Paint &paint, Glyph &out);

/**
 * A letter of a word for GetPaintedWord: ch stretched to the box x, y, w, h (from the word's top left, in pixels).
 */
struct PlacedLetter {
    char32_t ch;
    float    x, y, w, h;
};

/**
 * A word's letters painted as one: each letter of the font in paint.slot stretched to its box, every layer of the
 * word the letters' union's, so where letters meet their shadows and outlines merge instead of stacking. Its offsets
 * are from the word's top left.
 */
bool GetPaintedWord(const std::vector<PlacedLetter> &letters, const Paint &paint, Glyph &out);

/**
 * The bounds of ch in the font in `slot` as GetPaintedGlyph stretches them, in font units (y up): left, bottom,
 * right, top.
 */
bool GetGlyphBounds(char32_t ch, float (&bounds)[4], int slot = 0);

/**
 * The left and right of ch's outline in the current slot's font at em_px pixels to the em, in pixels from its origin
 * (as Glyph::x_offset is). A glyph's picture is wider than this by the soft edge round its outline.
 */
bool GetInkExtent(char32_t ch, int em_px, float &left, float &right);

/**
 * Loads the optional thin variant font into slot 1.
 */
bool LoadVariant(const std::filesystem::path &file);

/**
 * Whether the thin variant font (slot 1) is loaded.
 */
bool VariantLoaded();

/**
 * Selects the thin variant (slot 1) when variant is set, else the main font (slot 0).
 */
void UseVariant(bool variant);

/**
 * Loads the weapon status font (SourceSansProSemibold.ttf) into slot 2 from a file.
 */
bool LoadStatusFont(const std::filesystem::path &file);

/**
 * Loads the weapon status font into slot 2 from size bytes at data (copied).
 */
bool LoadStatusFontMemory(const unsigned char *data, std::size_t size);

/**
 * Whether the weapon status font (slot 2) is loaded.
 */
bool StatusFontLoaded();

/**
 * Loads the title menu's font (ChronicleTitle-Bold.ttf) into slot 3 from size bytes at data (copied).
 */
bool LoadTitleFontMemory(const unsigned char *data, std::size_t size);

/**
 * Whether the title menu's font (slot 3) is loaded.
 */
bool TitleFontLoaded();

/**
 * How far the active font kerns right after left at em_px pixels to the em, in screen pixels (negative closes them
 * up), from its kerning table or GPOS pair kerning.
 */
float KernPixels(char32_t left, char32_t right, int em_px);

/**
 * Selects the active font slot: 0 = Main, 1 = Thin, 2 = Status (SourceSansProSemibold), 3 = Title (Chronicle Title
 * Bold). A slot whose font is not loaded selects the main font.
 */
void UseFontSlot(int slot);

/**
 * Copies a w x h RGBA image into the atlas; out gets its atlas rectangle and size, with zero offsets and advance.
 */
bool AddImage(const std::vector<unsigned char> &rgba, int w, int h, Glyph &out);

/**
 * Counts atlas resets; glyphs fetched under an older generation point at stale atlas texels.
 */
unsigned Generation();

/**
 * The atlas texture, linearly filtered, for drawing glyphs.
 */
gfx::TextureBinding Binding();

/**
 * The active font's vertical metrics and figure advance, as fractions of the em.
 */
struct Metrics {
    float ascent;
    float descent;
    float advance;      /**< Advance of the digit 0. */
    float cap;          /**< Height of the H. */
    float cap_drop = 0; /**< How far the H reaches below the baseline. */
};

/**
 * The active font's Metrics, or fixed defaults when it is not loaded.
 */
Metrics GetMetrics();

} // namespace dc::text::capfont
